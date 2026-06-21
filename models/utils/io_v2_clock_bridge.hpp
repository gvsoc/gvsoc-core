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
 *     Fast pass-through. Forwards req/resp/retry unchanged, calling
 *     engine->sync() on the remote engine before each forward. No event
 *     scheduling, no queues. Matches v1's transparent freq-cross stub.
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
 *     `depth` caps total in-flight across all four stages. depth=1 is
 *     strictly serial; depth>1 lets FIFO kinds pipeline.
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

#include <deque>


class IoV2ClockBridge : public vp::Component, public vp::DebugMemIf
{
public:
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
    static void            out_resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            out_retry_handler(vp::Block *__this, vp::IoRetryChannel);

    // Parametric-path stage handlers
    static void fwd_src_done_handler(vp::Block *_this, vp::ClockEvent *ev);
    static void fwd_dst_done_handler(vp::Block *_this, vp::ClockEvent *ev);
    static void rev_src_done_handler(vp::Block *_this, vp::ClockEvent *ev);
    static void rev_dst_done_handler(vp::Block *_this, vp::ClockEvent *ev);

    void reschedule_event(vp::ClockEvent &ev, const std::deque<Txn> &queue,
                          vp::ClockEngine *engine);
    void enqueue_in(std::deque<Txn> &queue, vp::IoReq *req,
                    int64_t now_cycle, int min_spacing_cycles);

    vp::IoSlave  in{&IoV2ClockBridge::in_req_handler};
    vp::IoMaster out{&IoV2ClockBridge::out_retry_handler,
                     &IoV2ClockBridge::out_resp_handler};
    vp::Trace trace;

    int k_src_per_dir = 0;
    int k_dst_per_dir = 0;
    int depth = 1;
    bool parametric = false;

    vp::ClockEngine *master_engine = nullptr;
    vp::ClockEngine *slave_engine  = nullptr;

    // Parametric-path state (unused when k=0)
    vp::ClockEvent *fwd_src_event = nullptr;
    vp::ClockEvent *rev_dst_event = nullptr;
    vp::ClockEvent *fwd_dst_event = nullptr;
    vp::ClockEvent *rev_src_event = nullptr;
    std::deque<Txn> fwd_src_queue;
    std::deque<Txn> fwd_dst_queue;
    std::deque<Txn> rev_src_queue;
    std::deque<Txn> rev_dst_queue;
    bool retry_owed = false;
};
