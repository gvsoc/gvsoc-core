// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2BeatToSyncAdapter — dedicated adapter auto-inserted by the gvrun2 systree
 * on io_v2 bindings whose master side declares signature IoV2Beat and whose
 * slave side declares signature IoV2Sync.
 *
 * It is a specialisation of IoV2BeatAdapter for the case where the downstream
 * slave honours the *sync* contract: every request completes inline with
 * IO_REQ_DONE (never IO_REQ_GRANTED + resp(), never IO_REQ_DENIED + retry()).
 * That lets the adapter be trivial: each write beat is forwarded to the slave
 * inline (single-beat framing — the sync plane knows no bursts) and consumed
 * on the spot; a read burst — which carries no data under the beat protocol —
 * is streamed one beat at a time, each beat read from the sync slave at its
 * own delivery cycle (not pre-read at submit) so no queue of filled beats is
 * ever held. An FSM drives the queue: the beats of a burst are paced by each
 * beat's own bandwidth occupancy (get_duration(), floored at one cycle), so
 * the transfer spreads naturally across the burst window; writes/atomics
 * stream at one per cycle. The burst's head beat is the one exception — it
 * is issued at submit (the address phase) purely to sample the head latency,
 * and its data rides along to be delivered after that latency. No async/deny
 * bookkeeping.
 *
 * Wire-protocol invariants preserved (identical to the general adapter for the
 * sync case):
 *   - The input port's req callback returns IO_REQ_GRANTED — never IO_REQ_DONE.
 *   - For each READ submission the upstream master receives exactly
 *     ceil(total_size / beat_width) resp() calls in byte order, is_first on the
 *     first, is_last on the last, with size/addr/burst_id/status set per beat.
 *     addr is the per-beat start address (burst_addr + emitted). Read beats are
 *     distinct allocator-backed objects the terminal master copies out of and
 *     frees.
 *   - WRITES are acknowledged once per burst (AXI B-channel semantics, see
 *     io_v2.hpp "Write acknowledgement"). The adapter is the consumer of the
 *     master's allocator-backed write beats: each is forwarded inline and
 *     freed on the spot (parked, for the burst's last beat), and a single
 *     data-less ack — recycling that last beat — is emitted upstream when the
 *     burst completes. Every beat still queues its beat-width worth of SILENT
 *     entries in the same shared FIFO (virtual acks), so the single real ack
 *     drains through the read/write-interleaved 1-entry/cycle stream on
 *     exactly the cycle the last per-beat ack fired before the protocol
 *     change. Atomic opcodes (opcode != WRITE) carry response data and keep
 *     the classic round-trip of the master's own request.
 *   - Read beats are paced by each beat's own bandwidth occupancy: the head
 *     beat lands at the head latency (get_latency()), and every following beat
 *     max(1, get_duration()) cycles after the previous one. A slave with no
 *     duration (e.g. memory_v3) degrades to one beat per cycle. Each beat is
 *     read at the cycle it is delivered (the head beat excepted).
 *
 * Multi-outstanding: a second burst received while one is still streaming is
 * forwarded inline and queued behind the active one. It still pays its own
 * latency, counted from its submission: its first entry leaves at the later
 * of its slot in the stream and submit + latency, so the latency overlaps
 * the streaming of the bursts ahead of it, as on a pipelined AXI slave,
 * instead of being skipped.
 *
 * The file is a private header for the component implementation; no model
 * includes it (the framework auto-inserts the component).
 */

#pragma once

#include <cstdint>
#include <deque>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>


class IoV2BeatToSyncAdapter : public vp::Component, public vp::DebugMemIf
{
public:
    IoV2BeatToSyncAdapter(vp::ComponentConf &config);
    void reset(bool active) override;

    // Backdoor debug path (vp/debug_mem.hpp): the adapter is invisible — both
    // calls are forwarded unchanged to the component bound downstream.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

private:
    vp::DebugMemIf *resolve_debug_mem();

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            retry_handler(vp::Block *__this, vp::IoRetryChannel channel);
    // Upstream master is ready to accept responses again: re-send the held beat
    // (response-path back-pressure) and resume streaming.
    static void            resp_retry_in_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static void            fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    // One queued stream entry.
    //   READ_BEAT  — a read beat descriptor (addr/size/is_first/is_last/
    //                burst_id/initiator). The beat is read from the sync slave
    //                in emit_entry, at its delivery cycle; `beat` is normally
    //                nullptr. The one exception is a burst's head beat issued
    //                early at submit to sample the head latency: its already-
    //                read object is stashed in `beat` and reused at emit.
    //   WRITE_TICK — silent virtual ack: consumes its 1-entry/cycle slot in
    //                the shared FIFO (preserving the pre-burst-ack pacing)
    //                and emits nothing.
    //   WRITE_ACK  — the burst-final entry: emits the single data-less burst
    //                ack, recycling the burst's consumed last beat (`beat`);
    //                the ack fields are stamped onto it at emit time.
    //   ATOMIC     — classic round-trip of the master's own request (`beat`),
    //                emitted unmutated (it already carries data and status).
    struct StreamEntry
    {
        enum Kind { READ_BEAT, WRITE_TICK, WRITE_ACK, ATOMIC };
        Kind kind;
        vp::IoReq *beat;
        // WRITE_ACK fields (burst snapshot):
        uint64_t addr;      // burst base address
        uint64_t size;      // burst total byte count
        int64_t burst_id;
        vp::IoRespStatus status;
        void *initiator;
        bool is_first = false;   // READ_BEAT: upstream burst framing
        bool is_last = false;
        // Cycle before which the entry may not be emitted: submit + latency
        // on the first entry of a submission, 0 (no constraint) on the others.
        int64_t ready_cycle = 0;
    };

    // One upstream write burst currently accepting beats (nullptr between
    // bursts). Every beat completes inline on the sync plane, so the record
    // only lives from the is_first beat to the is_last beat of one burst.
    struct WriteBurst
    {
        uint64_t total = 0;       // bytes submitted (== completed: sync plane)
        uint64_t base_addr = 0;
        int64_t burst_id = -1;    // snapshot from the opening beat
        void *initiator = nullptr;
        vp::IoRespStatus status = vp::IO_RESP_OK;   // OR-latched INVALID
    };

    // Emit one entry. Reads the beat from the sync slave (READ_BEAT, unless
    // pre-issued as the head beat) and delivers it upstream. Sets
    // stream_interval to the delay before the next entry. Returns false if the
    // upstream master back-pressured the beat (held for re-send from
    // resp_retry_in_handler).
    bool emit_entry(const StreamEntry &e);

    // Enqueue the FSM for the next queued entry, stream_interval cycles from
    // now (set by the entry just emitted: a read beat's own bandwidth
    // occupancy, floored at one; everything else one per cycle), or later if
    // that entry's latency has not elapsed yet.
    void schedule_next();

    int beat_width;
    // Shared pool serving beat-sized requests with their payload co-allocated;
    // read beats are drawn from it and freed by the terminal master.
    vp::IoReqAllocator *beat_allocator;
    vp::IoSlave in;
    vp::IoMaster out;
    vp::ClockEvent fsm_event;

    // Entries awaiting their FIFO slot, in submission/byte order.
    std::deque<StreamEntry> entries;

    // Delay (cycles) before the FSM emits the next entry, set by the entry
    // just emitted (a read beat's max(1, get_duration()); one otherwise).
    int64_t stream_interval = 1;

    // The write burst currently accepting beats (nullptr between bursts:
    // after an is_last submission and before the next is_first). The sync
    // plane completes every beat inline, so bursts never pipeline here.
    WriteBurst *open_wr_burst = nullptr;

    // Response-path back-pressure: the upstream master refused the beat we just
    // emitted. We hold that exact object and re-send it from
    // resp_retry_in_handler; nothing else is emitted until it is accepted.
    // held_owned distinguishes, for reset(), an adapter-owned object (read
    // beat / recycled burst ack — freed) from a master-owned atomic
    // round-trip (left alone).
    bool      resp_held = false;
    vp::IoReq *held_req = nullptr;
    bool      held_owned = false;

    vp::Trace trace;
};
