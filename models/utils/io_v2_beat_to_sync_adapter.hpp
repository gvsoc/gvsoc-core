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
 * IO_REQ_DONE (never IO_REQ_GRANTED + resp(), never IO_REQ_DENIED + retry()),
 * and any access size is served in one call. That lets the adapter be trivial:
 * each incoming burst is forwarded to the slave immediately (inline DONE), the
 * response is pushed onto a queue, and an FSM streams it back upstream as one
 * resp() beat per cycle. No per-beat sub-reads, no async/deny bookkeeping, no
 * latency-spread maths.
 *
 * Wire-protocol invariants preserved (identical to the general adapter for the
 * sync case):
 *   - The input port's req callback returns IO_REQ_GRANTED — never IO_REQ_DONE.
 *   - For each submission the upstream master receives exactly
 *     ceil(total_size / beat_width) resp() calls in byte order, is_first on the
 *     first, is_last on the last, with size/data/addr/burst_id/status set per
 *     beat. addr is the per-beat start address (burst_addr + emitted).
 *   - The first beat is delayed by the slave's get_full_latency(); beats then
 *     stream at one per cycle.
 *
 * Multi-outstanding: a second burst received while one is still streaming is
 * forwarded inline and queued behind the active one.
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

    // Emit one beat of the active response (cur_*) at the current cur_offset.
    // Returns false if the upstream master back-pressured the beat (held for
    // re-send); the caller must then not advance the cursor.
    bool emit_beat(bool is_first, bool is_last, uint64_t beat);

    int beat_width;
    vp::IoSlave in;
    vp::IoMaster out;
    vp::ClockEvent fsm_event;

    // Responses awaiting beat streaming. Just the request pointers: a queued
    // response is never mutated until it becomes the active one, so nothing else
    // needs storing.
    std::deque<vp::IoReq *> pending;

    // Snapshot of the response currently being streamed (cur_req == nullptr ⇒
    // idle). Snapshotted on activation because the write / single-beat emit
    // round-trips and mutates cur_req in place.
    vp::IoReq        *cur_req = nullptr;
    uint8_t          *cur_data = nullptr;
    uint64_t          cur_size = 0;
    uint64_t          cur_offset = 0;
    uint64_t          cur_addr = 0;
    int64_t           cur_burst_id = -1;
    vp::IoRespStatus  cur_status = vp::IO_RESP_OK;

    // Response-path back-pressure: the upstream master refused the beat we just
    // emitted. We hold that exact object and re-send it from
    // resp_retry_in_handler; the cursor is not advanced until it is accepted.
    // held_beat snapshots its size (read before the master takes ownership, as a
    // round-tripped write/last object may be reused once accepted).
    bool      resp_held = false;
    vp::IoReq *held_req = nullptr;
    uint64_t  held_beat = 0;

    vp::Trace trace;
};
