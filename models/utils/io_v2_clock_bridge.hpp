// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2ClockBridge β€” v2 IO clock-domain bridge auto-inserted by the systree
 * on bindings whose two endpoints resolve to different clock sources.
 *
 * Two modes selected by (k_src_per_dir, k_dst_per_dir):
 *
 *   k_src = k_dst = 0 (DEFAULT, "sync_only"):
 *     Zero modeled latency, but still re-synchronizes each crossing onto
 *     the destination clock edge β€” like a CDC synchronizer sampling on the
 *     next edge. The forward req is forwarded inline (the downstream slave
 *     re-aligns when it enqueues on its own clock); the async resp is
 *     scheduled on the master engine's next edge so the cluster sees it on
 *     a cluster cycle, not on the SoC edge the resp happened to land on.
 *
 *   k_src > 0 or k_dst > 0 (parametric, "cdc_*_beh"):
 *     Models a CDC IP as four cycle-counted stages:
 *
 *       fwd_src (master_clk, k_src cycles) β†’ fwd_dst (slave_clk, k_dst)
 *       out.req β€” downstream slave β€” resp
 *       rev_src (slave_clk, k_src) β†’ rev_dst (master_clk, k_dst) β†’ in.resp
 *
 *     Each stage has its own FIFO of pending transactions ordered by
 *     absolute cycle deadline in its clock engine, and its own
 *     vp::ClockEvent always scheduled to the head's deadline. The slave-
 *     domain events are enqueued directly on the slave engine, so
 *     frequency changes are tracked structurally β€” the engine maps cycles
 *     to wall time, not the bridge.
 *
 *     `depth` caps the total in-flight across the four stages. depth=1 is
 *     strictly serial; depth>1 lets FIFO kinds pipeline.
 *
 *     With `axi_channels`, the bridge is an AXI CDC, with one FIFO per
 *     channel: reads (requests and their responses) and writes (write beats
 *     and their acknowledgements) have their own four stages, and `depth`
 *     caps the requests of each crossing at a time (its two forward stages),
 *     the responses crossing in their own FIFO, at the pace they come. A
 *     write is then never held behind a read, nor a read behind a write.
 *
 *     A request the downstream denies, or a response the upstream master
 *     denies, stays at the head of its channel until the retry.
 *
 * Ownership (io_v2 per-burst write-acknowledgement contract, see io_v2.hpp
 * "Write acknowledgement"):
 *
 *   The bridge is a mid-chain FORWARDER: it passes the SAME request object
 *   through in both directions and never copies a payload — the buffer
 *   behind a write beat's data is valid exactly as long as the beat is
 *   unfreed, so beats parked in the stage queues keep their payload alive
 *   by construction, no copy needed. Once the downstream GRANTs a forwarded
 *   write beat the bridge never touches it again: the target consumes and
 *   frees it, and non-last write beats produce no response at all. The
 *   burst's single ack flows back through the rev path as an ordinary
 *   response, relayed 1:1 (the bridge is not the initiator and never frees
 *   it). The one case where the bridge manufactures anything is an inline
 *   DONE on the burst's LAST write beat: the beat was granted to the bridge
 *   by the upstream master, so the bridge still owns it and recycles it in
 *   place as the data-less burst ack, delivered through the rev path so the
 *   ack keeps the CDC delay shape. Reads and atomics keep the classic
 *   round-trip through the rev queues, unchanged.
 *
 *   Because granted-and-consumed write beats never come back, the upstream
 *   accept window (the `depth` gate) must re-open from the FORWARD-path
 *   drain, not only from the response-path drain — see maybe_retry() /
 *   schedule_retry().
 *
 * Python wrappers (io_v2_clock_bridge.py) set sensible defaults per kind:
 *
 *   IoV2ClockBridge       k_src=0 k_dst=0 depth=1   (sync_only default)
 *   IoV2Cdc2PhaseBeh      k_src=1 k_dst=2 depth=1
 *   IoV2CdcFifoGrayBeh    k_src=1 k_dst=2 depth=2
 *   IoV2CdcFifo2PhaseBeh  k_src=3 k_dst=3 depth=2
 */

#pragma once

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>

#include <cstdint>
#include <deque>


class IoV2ClockBridge : public vp::Component, public vp::DebugMemIf
{
public:
    // Parametric-path channels: write beats and their acknowledgements, and
    // the rest (reads and their response beats).
    enum { CH_READ = 0, CH_WRITE = 1, NB_CHANNELS = 2 };

    IoV2ClockBridge(vp::ComponentConf &config);
    void start() override;
    void reset(bool active) override;

    // Backdoor debug access (vp/debug_mem.hpp): pure pass-through into the
    // output. Debug accesses are zero-time, so the CDC timing model does not
    // apply to them.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

private:
    struct Txn
    {
        vp::IoReq *req;
        int64_t   deadline_cycle;
    };

    // Single set of v2 IO callbacks; the implementation branches on
    // `this->parametric` to the fast or modeled path.
    static vp::IoReqStatus in_req_handler(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   out_resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            in_resp_retry_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static void            out_retry_handler(vp::Block *__this, vp::IoRetryChannel);

    // sync_only-path response delivery, aligned on the master clock edge
    static void resp_event_handler(vp::Block *_this, vp::ClockEvent *ev);

    // Parametric-path stage handlers
    static void fwd_src_done_handler(vp::Block *_this, vp::ClockEvent *ev);
    static void fwd_dst_done_handler(vp::Block *_this, vp::ClockEvent *ev);
    static void rev_src_done_handler(vp::Block *_this, vp::ClockEvent *ev);
    static void rev_dst_done_handler(vp::Block *_this, vp::ClockEvent *ev);
    // Master-engine event servicing an owed upstream retry (see
    // schedule_retry()).
    static void retry_event_handler(vp::Block *_this, vp::ClockEvent *ev);

    void enqueue_in(std::deque<Txn> &queue, vp::IoReq *req,
                    int64_t now_cycle, int min_spacing_cycles);

    // Parametric-path state of a channel. Reads (requests and their
    // responses) and writes (write beats and their acknowledgements) cross
    // apart, as the AXI channels of a CDC have their own FIFOs: a write is
    // not held behind a read, nor a read behind a write.
    struct Channel
    {
        std::deque<Txn> fwd_src;
        std::deque<Txn> fwd_dst;
        std::deque<Txn> rev_src;
        std::deque<Txn> rev_dst;
        bool retry_owed = false;
        // The downstream denied the request at the head of fwd_dst: it stays
        // there until the downstream calls retry(), which sends it again.
        bool fwd_out_denied = false;
        // The upstream master denied the response at the head of rev_dst: it
        // stays there until the master calls resp_retry(), which sends it
        // again.
        bool rev_in_denied = false;
    };

    // Reschedule a stage event on the earliest head of the channels, skipping
    // the denied ones (`denied` null for a stage which is never denied).
    void reschedule_stage(vp::ClockEvent &ev, std::deque<Txn> Channel::*queue,
                          bool Channel::*denied, vp::ClockEngine *engine);
    void reschedule_fwd_src();
    void reschedule_fwd_dst();
    void reschedule_rev_src();
    void reschedule_rev_dst();
    // Send the forward (resp. reverse) heads of a channel whose deadline is
    // reached, until one is denied.
    void fwd_dst_send(int ch);
    void rev_dst_send(int ch);

    // What the `depth` admission gate of a channel compares against: its
    // requests crossing with axi_channels, its requests and responses
    // without.
    int occupancy(int ch) const;
    // Channel of a request or response.
    int channel_of(vp::IoReq *req) const;
    // Master-engine context ONLY (in.retry() must fire on the upstream
    // domain): if a retry is owed and a slot is free, service it now.
    void maybe_retry();
    // Slave-engine-safe variant: cross the retry back into the master
    // domain via retry_event (mirrors the rev path's CDC re-alignment).
    void schedule_retry();

    vp::IoSlave  in{&IoV2ClockBridge::in_req_handler,
                    &IoV2ClockBridge::in_resp_retry_handler};
    vp::IoMaster out{&IoV2ClockBridge::out_retry_handler,
                     &IoV2ClockBridge::out_resp_handler};
    vp::Trace trace;

    int k_src_per_dir = 0;
    int k_dst_per_dir = 0;
    int depth = 1;
    // Reads and writes cross apart, each with its `depth` of requests (see
    // the class comment). Off: a single FIFO for both, requests and
    // responses alike.
    bool axi_channels = false;
    bool parametric = false;

    vp::ClockEngine *master_engine = nullptr;
    vp::ClockEngine *slave_engine  = nullptr;

    // sync_only-path state: responses pending delivery on the next master edge
    vp::ClockEvent *resp_event = nullptr;
    std::deque<vp::IoReq *> resp_queue;
    // The upstream master denied the response at the head of resp_queue: it
    // stays there until the master calls resp_retry().
    bool resp_held = false;
    // We denied a downstream response (the upstream master had denied it):
    // the downstream holds it and waits for our resp_retry().
    bool resp_retry_owed = false;
    // Deliver the queued responses upstream, until one is denied.
    void deliver_resps();

    // Parametric-path state (unused when k=0)
    vp::ClockEvent *fwd_src_event = nullptr;
    vp::ClockEvent *rev_dst_event = nullptr;
    vp::ClockEvent *fwd_dst_event = nullptr;
    vp::ClockEvent *rev_src_event = nullptr;
    // Always enqueued on the master engine: services an owed upstream retry
    // when occupancy was freed on the slave-domain (forward) side, where
    // in.retry() must not be called directly.
    vp::ClockEvent *retry_event = nullptr;
    Channel channels[NB_CHANNELS];
};
