// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2BeatAdapter — standalone component auto-inserted by the gvrun2 systree
 * on io_v2 bindings whose master side declares signature IoV2Beat and whose
 * slave side declares signature IoV2BigPacket. The adapter normalises any of
 * the three v2 slave response forms (sync DONE, async big-packet, beat
 * stream) into a uniform per-beat resp() stream observed by the upstream
 * master.
 *
 * Wire-protocol invariants preserved across the adapter:
 *
 *   - submit() (the IoSlave "input" port's req callback) returns IO_REQ_GRANTED
 *     or IO_REQ_DENIED — never IO_REQ_DONE. An inline DONE from the downstream
 *     slave is converted to a scheduled per-beat resp() stream.
 *   - For each accepted submission, the upstream master receives exactly
 *     ceil(total_size / beat_width) resp() calls in cumulative byte order,
 *     with is_first=true on the first call, is_last=true on the last, and
 *     req->size / req->data / req->addr / req->burst_id / req->status mutated
 *     per beat. addr is set to the per-beat start address (= burst_addr +
 *     cumulative emitted bytes); see the io_v2.hpp contract note about
 *     per-beat addr.
 *   - Per-beat ready cycle honours the slave's req->latency annotation. A
 *     sync DONE / async big resp covering M beats spreads them at
 *     now+latency, now+latency+1, … so the LAST beat lands at now+latency
 *     and the bandwidth annotation is not double-counted by the per-beat
 *     spread (matching the existing semantics inherited from the v1 helper).
 *   - burst_id is captured at schedule time and re-applied per beat at fire
 *     time, so a deeper router that mutates and restores burst_id between
 *     schedule and fire does not leak its restored value upstream.
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


class IoV2BeatAdapter : public vp::Component
{
public:
    IoV2BeatAdapter(vp::ComponentConf &config);
    void reset(bool active) override;

private:
    // One scheduled beat, sitting in the pending deque until its ready cycle.
    struct PendingBeat
    {
        vp::IoReq *req;
        uint8_t *data;
        uint64_t size;
        uint64_t offset;
        uint64_t addr;            // per-beat addr = burst_addr + offset
        int64_t  ready_cycle;
        bool is_first;
        bool is_last;
        vp::IoRespStatus status;
        // burst_id snapshot — see file header.
        int64_t  burst_id;
    };

    // Per-in-flight-request progress. Keyed by the upstream IoReq*; alive
    // from submit() until cumulative bytes routed reach total_size.
    struct InFlight
    {
        uint64_t total_size;
        uint64_t bytes_routed;
        uint64_t burst_addr;      // snapshot of req->addr at submit time
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static void            resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            retry_handler(vp::Block *__this);
    static void            fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    void schedule_chunk(vp::IoReq *req, uint8_t *data, uint64_t size,
                        int64_t latency_cycles);
    void reschedule_fsm();
    void emit_beat(const PendingBeat &ev);

    int beat_width;
    vp::IoSlave in;
    vp::IoMaster out;
    vp::ClockEvent fsm_event;
    std::deque<PendingBeat> pending;
    std::unordered_map<vp::IoReq *, InFlight> in_flight;
    vp::Trace trace;
};
