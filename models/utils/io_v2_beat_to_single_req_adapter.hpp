// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2BeatToSingleReqAdapter — standalone component auto-inserted by the gvrun2
 * systree on io_v2 bindings whose master side declares signature IoV2Beat and
 * whose slave side declares signature IoV2SingleReq. A single-req slave answers
 * each request with a SINGLE-BEAT response — inline DONE, async GRANTED + one
 * resp(), or DENIED + retry() — but never a multi-beat stream. The adapter
 * normalises those single-beat responses into a uniform per-beat resp() stream
 * observed by the upstream master.
 *
 * Read path — in-order, like the HW axi2mem bridge it models:
 *   An incoming read burst is pushed whole onto the issue chain; each beat-sized
 *   sub-read is generated on the fly from the head burst's beat cursor when it is
 *   issued (one per cycle) — nothing is pre-expanded, exactly like the HW address
 *   generator (base address + counter). Once a burst's last sub-read is issued it
 *   moves to the drain chain (the HW r_id FIFO) where it waits for its responses.
 *   The number of read bursts in flight (accepted but not yet fully delivered) is
 *   bounded by `max_read_bursts` (default 4, the HW r_id FIFO depth): when the
 *   bound is hit the read request channel is back-pressured (IO_REQ_DENIED), and
 *   in.retry() is raised when a burst completes and frees a slot.
 *
 *   Because a single-req slave is in-order (a valid burst maps to one output, and
 *   the io_v2 response comes back on the very object sent), the adapter does NOT
 *   keep a searchable out-of-order reassembly buffer. It just tracks the
 *   sub-reads it has issued in a FIFO (`issued`), pops them in order as responses
 *   arrive, and forwards the SAME sub-read object straight upstream as the beat —
 *   no second allocation, no per-beat search. The in-order assumption is CHECKED
 *   with traces.assert (asserts/debug builds): a response that is not the oldest
 *   outstanding sub-read means the slave reordered, which is forbidden here.
 *   Contrast IoV2BeatAdapter, which is OOO-defensive (it serves IoV2BigPacket
 *   slaves that may legitimately reorder) and so reassembles per beat.
 *
 * Write path — unchanged from the general adapter: the whole write is forwarded
 * as one request and the single-beat response is spread into the per-beat
 * resp() stream by schedule_chunk() (queue) + emit_write_beat() (per-beat cursor).
 *
 * SingleReq contract checks (asserts/debug builds): each forwarded request is
 * answered by exactly one single-beat response covering its whole size, and read
 * responses arrive in issue order.
 *
 * Wire-protocol invariants preserved across the adapter:
 *
 *   - submit() (the IoSlave "input" port's req callback) returns IO_REQ_GRANTED
 *     or IO_REQ_DENIED — never IO_REQ_DONE. An inline DONE from the downstream
 *     slave is converted to a scheduled per-beat resp() stream.
 *   - For each accepted submission, the upstream master receives exactly
 *     ceil(total_size / beat_width) resp() calls in cumulative byte order,
 *     with is_first=true on the first call, is_last=true on the last, and
 *     req->size / req->data / req->addr / req->burst_id / req->status set per
 *     beat. addr is the per-beat start address (burst_addr + cumulative bytes).
 *   - Per-beat ready cycle honours the slave's req->get_full_latency(): the LAST
 *     beat lands at now+latency so a bandwidth annotation is not double-counted
 *     by the per-beat spread.
 *
 * Ownership (initiator-owned request convention): the upstream master owns its
 * burst request for the whole transaction and frees it itself (typically on the
 * last response); the adapter NEVER frees it. Each read beat delivered upstream is
 * a distinct adapter-allocated object the master frees as it consumes it — the
 * master's request is never round-tripped as a read beat, even for a single-beat
 * read. (Writes still round-trip the master's own request as the single ack, which
 * the master frees/recycles as its own object.) Correlation back to the master's
 * per-request state is via req->initiator, copied onto every beat, not via object
 * identity — a beat master (e.g. the FloONoc NI) must therefore correlate by
 * req->initiator, not by assuming the response object is the one it sent.
 *
 * No public Handler API: the file is a private header for the component
 * implementation. The framework auto-inserts the component during the
 * gvrun2 binding-collection pass; no model includes this header.
 */

#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>
// Generated from the IoV2BeatToSingleReqAdapterConfig dataclass in the Python
// generator (config tree). Provides struct IoV2BeatToSingleReqAdapterConfig.
#include <utils/io_v2_beat_to_single_req_adapter/io_v2_beat_to_single_req_adapter_config.hpp>


class IoV2BeatToSingleReqAdapter : public vp::Component, public vp::DebugMemIf
{
public:
    IoV2BeatToSingleReqAdapter(vp::ComponentConf &config);
    void reset(bool active) override;

    // Backdoor debug path (vp/debug_mem.hpp): the adapter is invisible —
    // both calls are forwarded unchanged to the component bound downstream.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

private:
    vp::DebugMemIf *resolve_debug_mem();

    // An accepted WRITE burst awaiting its ack-beat stream. The slave answers the
    // whole write with a single response; we turn that into the per-beat ack stream
    // the upstream beat master expects — but, like the read path, NOTHING is
    // pre-expanded: each ack beat is generated on the fly from emitted_beats when
    // its modeled ready cycle arrives (the HW holds the burst and emits one beat
    // per cycle from a counter). Beat i is due at first_ready + i*step (step = the
    // slave's per-beat time, so the last beat lands at the response's full latency).
    struct WriteBurst
    {
        vp::IoReq *up_req;        // master write req, round-tripped as each ack beat
        uint8_t   *base_data;     // snapshot of req->data (initiator buffer)
        uint64_t   base_addr;     // snapshot of req->addr at submit time
        uint64_t   total_size;
        int64_t    burst_id;
        vp::IoRespStatus status;
        int        nb_beats;      // total==0 ? 1 : ceil(total/beat_width)
        int        emitted_beats; // running cursor: beats already sent upstream
        int64_t    first_ready;   // ready cycle of beat 0
        int64_t    step;          // cycles between consecutive ack beats
    };

    // Per-in-flight WRITE-request progress, keyed by the upstream IoReq*. Alive
    // from submit() until cumulative bytes routed reach total_size.
    struct InFlight
    {
        uint64_t total_size;
        uint64_t bytes_routed;
        uint64_t burst_addr;      // snapshot of req->addr at submit time
    };

    // An accepted read burst, alive from accept until its last response has been
    // received. ALL per-sub-read state lives here: each beat is generated on the
    // fly from the beat index (the HW address-generator model — nothing is
    // pre-expanded into per-beat job objects). issued_beats / completed_beats are
    // the running cursors for issuing downstream and forwarding upstream.
    struct ReadBurst
    {
        vp::IoReq *up_req;        // master burst req (freed on the last beat)
        uint64_t   base_addr;     // snapshot of req->addr at submit time
        uint8_t   *base_data;     // snapshot of req->data (initiator buffer)
        uint64_t   total_size;
        int64_t    burst_id;      // snapshot of req->burst_id
        int        nb_beats;      // total==0 ? 1 : ceil(total/beat_width)
        int        issued_beats;  // beats already issued downstream
        int        completed_beats; // beats whose response has been received
    };

    // A completed read beat waiting for its modeled ready cycle, then forwarded
    // upstream. `beat` is the sub-read object itself (framing already rewritten to
    // the burst position) — it becomes the response the initiator frees; the
    // adapter never frees it or the master's request.
    struct ReadyBeat
    {
        vp::IoReq *beat;
        int64_t    ready_cycle;
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            retry_handler(vp::Block *__this, vp::IoRetryChannel channel);
    // Upstream master is ready to accept responses again: re-send the beat we
    // are holding (response-path back-pressure).
    static void            resp_retry_in_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static void            fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    void reschedule_fsm();

    // Write path.
    void schedule_chunk(vp::IoReq *req, uint8_t *data, uint64_t size,
                        int64_t latency_cycles);
    // Emit the front write burst's next ack beat (generated from its cursor).
    void emit_write_beat(WriteBurst &b);

    // Read path.
    bool issue_one_sub_read();
    void issue_pending_sub_reads();
    void complete_read_beat(vp::IoReq *req, vp::IoRespStatus status,
                            int64_t latency_cycles);
    void emit_read_beat(const ReadyBeat &rb);
    // A read burst's last beat reached the master: free its slot and, if the read
    // request channel was back-pressured, raise in.retry().
    void on_read_burst_complete();

    // Populated from the generated config tree by the Component(config, cfg)
    // constructor (memcpy of tree_config). beat_width / max_read_bursts below are
    // the runtime copies read from it at construction.
    IoV2BeatToSingleReqAdapterConfig cfg;

    int beat_width;
    vp::IoSlave in;
    vp::IoMaster out;
    vp::ClockEvent fsm_event;

    // WRITE bursts awaiting their ack-beat stream, drained FIFO from the front;
    // each beat is generated on the fly from the front burst's cursor.
    std::deque<WriteBurst> write_bursts;
    // READ beats awaiting their ready cycle (the sub-read objects themselves).
    std::deque<ReadyBeat> read_pending;
    std::unordered_map<vp::IoReq *, InFlight> in_flight;

    // Ready cycle of the last read beat scheduled so far, so the read channel never
    // exceeds one beat per cycle while still letting a read and a write proceed in
    // the same cycle (the split-channel behaviour of the beat router upstream). The
    // write channel paces itself from the front burst's cursor (one beat/cycle in
    // fsm_handler), so it needs no separate watermark.
    int64_t read_last_sched_cycle;
    // Cycle of the most recent sub-read issued downstream. Issuance is paced to
    // at most one sub-read per cycle so a *series* of bandwidth routers does not
    // each compound its per-request wait (which would halve throughput).
    int64_t read_issue_last_cycle = -1;

    // Read bursts in flight, split into two in-order chains so each chain's head
    // is the one to act on — no scanning (mirrors the HW: an address generator
    // issuing one burst, plus the r_id FIFO of bursts still draining responses):
    //   issue_bursts  — not yet fully issued; HEAD is the burst being issued.
    //   drain_bursts  — fully issued, awaiting responses; FRONT is the oldest.
    // A burst is accepted onto issue_bursts, moves to drain_bursts once its last
    // sub-read is issued, and is dropped once its last response arrives. The
    // completing burst is drain_bursts.front(), or — while a single burst is
    // still issuing and its early responses arrive — issue_bursts.front().
    std::deque<ReadBurst> issue_bursts;
    std::deque<ReadBurst> drain_bursts;
    // Max read bursts in flight (accepted but not fully delivered upstream). When
    // reached, the read request channel is back-pressured. HW r_id-FIFO analogue.
    int max_read_bursts = 4;
    int outstanding_read_bursts = 0;
    // True when a read request was DENIED for the burst limit and the master is
    // owed an in.retry() once a slot frees.
    bool read_blocked = false;

    // Sub-reads issued downstream (GRANTED) and awaiting their async response, in
    // strict issue order. Holds just the req pointers — the response must come
    // back on the front one (in-order, asserted); the beat framing is derived
    // from the front burst's completed_beats cursor, not stored per sub-read.
    std::deque<vp::IoReq *> issued;
    // Outstanding-window depth: with one-per-cycle issuance the window must cover
    // the downstream round-trip latency to sustain 1 beat/cycle.
    int max_sub_outstanding = 32;

    // True when a sub-read was DENIED by the downstream: no further sub-reads are
    // issued until retry(), where it is re-generated from the burst cursor (the
    // issuing burst's issued_beats was not advanced, so it regenerates the same
    // beat — no separate held-job state needed).
    bool sub_read_denied = false;

    // Response-path back-pressure: the upstream master refused our most recent
    // resp() beat. We hold that exact object and re-send it from
    // resp_retry_in_handler. The adapter frees nothing (the initiator owns its
    // request and the response objects). When the held beat finally lands we need
    // to advance the right cursor: held_write means it is the front write burst's
    // current ack beat (advance emitted_beats, pop if last); held_read_last means
    // it is a read burst's last beat (release the in-flight slot).
    bool resp_held = false;
    vp::IoReq *held_req = nullptr;
    bool held_read_last = false;
    bool held_write = false;

    vp::Trace trace;
};
