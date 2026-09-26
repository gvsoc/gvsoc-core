// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#pragma once

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>

// io_v2 bridge between two clock domains driven by the same clock (one clock
// behind two gates or dividers, no synchronizer on the chip): the two domains
// have every edge in common, so nothing is resynchronized. Not for a real
// clock domain crossing, which keeps its synchronizer latency whatever the
// phase of the two clocks (see IoV2ClockBridge and its cdc kinds).
//
// The crossing is registered, as with the AXI slices of a cluster interface.
// Each direction buffers one request (or response) and sends one per cycle, at
// least one cycle after it was taken:
//
// - A request is taken if the slot is empty, or if the one in it leaves in
//   this cycle (it has waited its cycle and was not denied): the slot then
//   frees in this cycle, before or after the new one arrives depending on the
//   order of the two engines, which must not matter. A stream is thus never
//   denied as long as the other side takes everything.
// - A request the other side denies keeps the slot, so the next one is denied
//   (congestion). Once the slot frees, the retry is sent on the next cycle,
//   from the domain which was denied.
//
// Each direction is sent by an event of the domain it goes to (requests on the
// output engine, responses on the input engine), and its retry by an event of
// the domain it comes from, so that the bridge only calls into a domain from
// one of its own events, among the events of its cycle, never from the other
// engine, whose order with this one at a given time is not fixed.
class IoV2SharedClockBridge : public vp::Component, public vp::DebugMemIf
{
public:
    IoV2SharedClockBridge(vp::ComponentConf &config);
    void start() override;
    void reset(bool active) override;

    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

private:
    // Pending requests or responses of one direction, chained through
    // IoReq::next, which the bridge owns while the request is in the list. It
    // holds one of them, two only while the first one leaves in this cycle.
    struct List
    {
        vp::IoReq *head = nullptr;
        vp::IoReq *tail = nullptr;
        // First cycle where the head can be sent.
        int64_t head_cycle = 0;
        // The other side denied the head: it stays there until that side
        // asks for a retry.
        bool held = false;
        // We denied the side feeding this list: it waits for our retry.
        bool retry_owed = false;
        // Engine of the domain the list is sent to, which runs its event and
        // counts its cycles.
        vp::ClockEngine *engine = nullptr;
        vp::ClockEvent *event = nullptr;
        // Engine of the domain feeding the list, which runs its retry event.
        vp::ClockEngine *src_engine = nullptr;
        vp::ClockEvent *retry_event = nullptr;
    };

    static vp::IoReqStatus in_req_handler(vp::Block *__this, vp::IoReq *req);
    static void            in_resp_retry_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static vp::IoRespAck   out_resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            out_retry_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static void            req_event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void            resp_event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void            req_retry_event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void            resp_retry_event_handler(vp::Block *__this, vp::ClockEvent *event);

    // Set the engines and events of the two lists.
    void init_lists();
    // Tell if a list can take a new request or response now.
    bool can_take(List &list);
    // Append a request to a list, once can_take() said so.
    void push(List &list, vp::IoReq *req);
    // Remove the head of a list, given its successor read before the head was
    // sent. The next one can go on the next cycle.
    void pop(List &list, vp::IoReq *next);
    // Send the head of the request list to the output, if it can go now.
    void req_send();
    // Send the head of the response list to the input, if it can go now.
    void resp_send();
    // Wake up on the next cycle of its engine if a list still has something
    // to send.
    void check(List &list);

    vp::IoSlave  in{&IoV2SharedClockBridge::in_req_handler,
                    &IoV2SharedClockBridge::in_resp_retry_handler};
    vp::IoMaster out{&IoV2SharedClockBridge::out_retry_handler,
                     &IoV2SharedClockBridge::out_resp_handler};
    vp::Trace trace;

    vp::ClockEngine *master_engine = nullptr;
    vp::ClockEngine *slave_engine  = nullptr;

    List req_list;
    List resp_list;
    vp::ClockEvent req_event;
    vp::ClockEvent resp_event;
    vp::ClockEvent req_retry_event;
    vp::ClockEvent resp_retry_event;
};
