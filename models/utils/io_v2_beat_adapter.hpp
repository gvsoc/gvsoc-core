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
#include <vp/debug_mem.hpp>


class IoV2BeatAdapter : public vp::Component, public vp::DebugMemIf
{
public:
    IoV2BeatAdapter(vp::ComponentConf &config);
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
    // from submit() until cumulative bytes routed reach total_size. Used by
    // the WRITE path only (reads are tracked by the read_jobs FIFO instead).
    struct InFlight
    {
        uint64_t total_size;
        uint64_t bytes_routed;
        uint64_t burst_addr;      // snapshot of req->addr at submit time
    };

    // One beat-sized downstream sub-read to perform for an in-flight read
    // burst. A read burst received from the upstream beat master is split into
    // ceil(size/beat_width) of these and issued downstream one per cycle, so
    // the big-packet interconnect sees per-cycle beat traffic instead of one
    // whole-burst request.
    struct SubReadJob
    {
        vp::IoReq *up_req;        // upstream req (resp target + burst_id source)
        uint64_t   offset;        // cumulative byte offset within the burst
        uint64_t   beat_bytes;    // min(beat_width, remaining) — short final beat ok
        uint64_t   addr;          // burst_addr + offset
        uint8_t   *data;          // master_data + offset (initiator buffer slice)
        bool       is_first;      // offset == 0
        bool       is_last;       // offset + beat_bytes == total_size
        int64_t    burst_id;      // snapshot of up_req->burst_id
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            retry_handler(vp::Block *__this, vp::IoRetryChannel channel);
    // Upstream master is ready to accept responses again: re-send the beat we
    // are holding (response-path back-pressure).
    static void            resp_retry_in_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static void            fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    void schedule_chunk(vp::IoReq *req, uint8_t *data, uint64_t size,
                        int64_t latency_cycles);
    void reschedule_fsm();
    void emit_beat(const PendingBeat &ev);

    // Read path.
    void enqueue_read_burst(vp::IoReq *req);
    void issue_sub_read(const SubReadJob &job);
    void issue_pending_sub_reads();
    void drain_completed_sub_reads();
    void complete_sub_read(const SubReadJob &job, vp::IoRespStatus status,
                           int64_t latency_cycles);

    int beat_width;
    vp::IoSlave in;
    vp::IoMaster out;
    vp::ClockEvent fsm_event;
    std::deque<PendingBeat> pending;
    std::unordered_map<vp::IoReq *, InFlight> in_flight;
    // Ready cycle of the last beat scheduled so far, tracked separately for the
    // read and write channels. New beats on a channel are forced to be at least
    // one cycle after the previous beat on that *same* channel, so each channel
    // never exceeds one beat per cycle even when the downstream slave delivers a
    // whole burst's chunks in the same cycle. Keeping the cursors independent
    // lets a read and a write proceed in the same cycle (2 beats/cycle), which
    // is the split-channel behaviour of the beat router upstream; serialising
    // R and W onto one channel is the router's shared_rw_channel job, not the
    // adapter's.
    int64_t read_last_sched_cycle;
    int64_t write_last_sched_cycle;
    // Cycle of the most recent sub-read issued downstream. Issuance is paced to
    // at most one sub-read per cycle: a bandwidth router charges burst_duration
    // and advances a per-input watermark per request, so bursting several reads
    // in one cycle makes a *series* of such routers each compound the bandwidth
    // wait — double-charging the transfer (two 8 B/cyc routers would stream at
    // 4 B/cyc). One per cycle lets each watermark catch up so the per-beat
    // latency stays flat, while the outstanding window still hides round-trip.
    int64_t read_issue_last_cycle = -1;

    // Read sub-request pacing. Reads pending issue sit in read_jobs (FIFO, a
    // whole burst enqueued at once so bursts are serviced in arrival order).
    // Several beat-sized sub-reads are kept in flight downstream at once
    // (up to max_sub_outstanding) so reads pipeline at the downstream's
    // throughput rather than its per-beat round-trip latency.
    std::deque<SubReadJob> read_jobs;

    // One in-flight (or just-completed-but-not-yet-drained) downstream
    // sub-read. Each carries its own heap req object so several can be
    // outstanding. Responses may arrive out of order (multi-bank shared L2),
    // so completed entries are buffered and drained to the upstream beat
    // stream strictly in issue/offset order — the upstream master needs
    // is_last to be the genuine last beat.
    struct InflightSubRead
    {
        vp::IoReq        *req;
        SubReadJob        job;
        bool              completed = false;
        vp::IoRespStatus  status    = vp::IO_RESP_OK;
        int64_t           latency   = 0;
    };
    std::deque<InflightSubRead> sub_inflight;
    // Outstanding-window depth. With one-per-cycle issuance the window must be
    // at least the downstream round-trip latency to sustain 1 beat/cycle; sized
    // generously so high-latency paths (e.g. router_latency=10) don't bottleneck.
    int max_sub_outstanding = 32;

    // A sub-read DENIED by the downstream is held here and re-issued from the
    // retry() callback; no further sub-reads are issued until it is accepted.
    bool sub_read_denied = false;
    SubReadJob denied_job;

    // Response-path back-pressure: the upstream master (e.g. a beat router
    // arbitrating its per-input response channel) refused our most recent
    // resp() beat. We hold that exact object and re-send it from
    // resp_retry_in_handler; no further upstream beats are emitted until it is
    // accepted. The master's burst request is never held/freed here — the
    // initiator owns it (initiator-owned request convention).
    bool resp_held = false;
    vp::IoReq *held_req = nullptr;

    vp::Trace trace;
};
