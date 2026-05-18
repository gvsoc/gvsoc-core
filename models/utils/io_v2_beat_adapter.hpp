// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * BeatResponseAdapter — normalises any io_v2 slave response form into a uniform
 * per-beat callback stream for a beat-fidelity initiator.
 *
 * The protocol lets a slave reply to a single IoReq in one of three forms:
 *
 *   1. Sync big-packet:  req() returns IO_REQ_DONE inline with req->data filled.
 *   2. Async big-packet: req() returns IO_REQ_GRANTED, one resp() later with the
 *      full size and is_last=true.
 *   3. Beat stream:      req() returns IO_REQ_GRANTED, N resp() calls later, each
 *      one beat-wide, cumulative size matching the request.
 *
 * A beat-fidelity initiator (cycle-accurate iDMA back-end, cycle-accurate router
 * output port) wants to consume the response as a uniform per-beat stream
 * regardless of which form the slave produced. It owns one BeatResponseAdapter
 * per outbound master; the adapter owns the bus-facing IoMaster and exposes a
 * submit() / Handler API.
 *
 * Invariants the adapter guarantees:
 *
 *   - submit() returns IO_REQ_GRANTED or IO_REQ_DENIED. Never IO_REQ_DONE — an
 *     inline DONE is internally converted to a scheduled beat-callback stream so
 *     the owner always sees the same "callbacks pending" semantic.
 *   - For every accepted submit, the handler receives exactly
 *     ceil(total_size / beat_width) on_beat callbacks in cumulative byte order,
 *     with is_first=true on the first and is_last=true on the last. Cumulative
 *     (offset + size) covers the request's full size with no gaps or overlaps.
 *   - Per-beat ready_cycle honors the slave's pacing. A sync DONE / async big
 *     resp covering M beats spreads them at now+latency, now+latency+1, ...
 *     A beat-form resp at cycle T schedules one beat at T+latency. Two
 *     independent requests whose responses arrive on the same cycle (e.g. on
 *     a non-shared-channel router output handling parallel R and W traffic)
 *     both schedule at the same ready_cycle and fire in the same fsm tick —
 *     the adapter does not artificially serialise them.
 *   - Multiple in-flight requests are tracked independently (per-IoReq*
 *     bookkeeping), so interleaved responses don't confuse is_first/is_last
 *     derivation.
 *   - reset(true) clears all internal state.
 *
 * The adapter holds no ownership of the request's data buffer — BeatEvent.data
 * is a pointer into the initiator's buffer.
 *
 * When (not) to use it
 * --------------------
 *
 * A component needs a BeatResponseAdapter on an outbound io_v2 master iff all
 * three of these are true:
 *
 *   1. It drives a per-beat consumer downstream of that master (e.g. a router
 *      output emitting one upstream resp() per beat, or a DMA back-end feeding
 *      one chunk per cycle into the destination back-end).
 *   2. It wants the consumer fed at one beat per cycle regardless of whether
 *      the immediate slave answered as sync DONE, async big-packet, or beat
 *      stream.
 *   3. It cares about cycle-accurate per-beat spacing — so a slow slave's
 *      req->latency translates naturally into beat-rate slowdown, and a
 *      bandwidth-limited upstream sees the pipeline fill correctly.
 *
 * Components that don't match all three (and therefore do NOT need an
 * adapter on their master side):
 *
 *   - Functional passthroughs (router_v2_untimed, demux_v2, remapper_v2,
 *     rw_splitter_v2, log_ico_v2): no timing model, response is just
 *     in->resp(req) inside the downstream's resp_meth.
 *   - Request-side latency annotators (router_v2_bandwidth,
 *     router_v2_backpressure, limiter_v2): timing is modeled by writing
 *     req->latency or by pacing the forward-side emission rate; the
 *     response path is a verbatim passthrough. The annotated latency is
 *     what the *next* beat-fidelity hop downstream picks up — the bandwidth
 *     model already translates into beat-rate pacing at that hop without
 *     these routers needing their own adapter (adding one would double-count
 *     the bandwidth, like the bug we fixed inside the adapter with
 *     step = max(1, latency/N)).
 *   - Terminal targets / pure initiators (memory_v3, loader_v2): the
 *     adapter has no master to sit in front of.
 *   - cache_v4: would be a candidate only if cache-line fill becomes a
 *     cycle-by-cycle modeled event. Today it uses req->latency on hits and
 *     a single async resp on miss refills, neither of which fan out into a
 *     per-beat stream.
 *
 * Today the only components that match all three criteria — and therefore the
 * only adapter users — are router_v2_beat and the iDMA's idma_be_axi back-end.
 */

#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>


class BeatResponseAdapter : public vp::Block
{
public:
    // One element of the normalised per-beat callback stream.
    struct BeatEvent
    {
        vp::IoReq *req;        // original request
        uint8_t *data;         // slice pointer into the request's data buffer
        uint64_t size;         // beat size, <= beat_width
        uint64_t offset;       // byte offset of this beat inside the burst
        int64_t  ready_cycle;  // cycle at which this beat is observed
        bool is_first;         // first beat of the burst (cumulative)
        bool is_last;          // last beat of the burst (cumulative)
        vp::IoRespStatus status;
        // Snapshot of req->burst_id at schedule_chunk time. Necessary because
        // req->burst_id is a router-local slot index that downstream cascades
        // mutate and restore in place; reading event.req->burst_id at on_beat
        // fire time can pick up the restored value of a deeper router rather
        // than this router's own slot. Consumers that want a stable per-burst
        // tag must use this captured field.
        int64_t  burst_id;
    };

    // Implemented by the owning beat-fidelity component.
    class Handler
    {
    public:
        virtual ~Handler() = default;
        virtual void on_beat(const BeatEvent &beat) = 0;
        virtual void on_retry() = 0;
    };

    BeatResponseAdapter(vp::Block *parent, std::string name,
                        int beat_width, Handler *handler);

    // The bus-facing IoMaster — bind this outward via the owning component's
    // new_master_port(...). Pass &this_adapter as the context, so the adapter's
    // static resp/retry trampolines dispatch back into this instance.
    vp::IoMaster &out() { return this->downstream; }

    // Forward `req` downstream and register it for response normalisation.
    // Returns IO_REQ_GRANTED (response will arrive as on_beat callbacks) or
    // IO_REQ_DENIED (initiator must hold the req and resubmit on on_retry).
    // Never returns IO_REQ_DONE.
    vp::IoReqStatus submit(vp::IoReq *req);

    void reset(bool active) override;

private:
    // Per-in-flight-request progress. Keyed by the IoReq* the initiator
    // submitted; lifetime is from submit() until cumulative bytes routed reach
    // total_size (which may be the same call for sync DONE or many calls later
    // for beat-stream responses).
    struct InFlight
    {
        uint64_t total_size;     // req->size as observed at submit()
        uint64_t bytes_routed;   // bytes already emitted as on_beat callbacks
    };

    static void resp_handler(vp::Block *__this, vp::IoReq *req);
    static void retry_handler(vp::Block *__this);
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    // Convert one chunk of response (a sync DONE payload or one resp()'s
    // payload) into 1..N pending BeatEvents.
    void schedule_chunk(vp::IoReq *req, uint8_t *data, uint64_t size,
                        int64_t latency_cycles);

    void reschedule_fsm();

    int beat_width;
    Handler *handler;
    vp::IoMaster downstream;
    vp::ClockEvent fsm_event;
    std::deque<BeatEvent> pending;                          // FIFO, sorted by ready_cycle
    std::unordered_map<vp::IoReq *, InFlight> in_flight;
    vp::Trace trace;
};
