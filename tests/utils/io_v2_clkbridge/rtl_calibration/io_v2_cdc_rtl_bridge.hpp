// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2CdcRtlBridge β€” generic v2 IO clock-domain bridge backed by a
 * Verilator plugin wrapping a real CDC SystemVerilog block.
 *
 * Acts as a drop-in replacement for `utils.io_v2_clock_bridge`. The same
 * C++ component is reused across multiple bridge "kinds" (cdc_2phase,
 * cdc_fifo_gray, cdc_fifo_2phase, ...) by pointing `plugin_path` at
 * different Verilator-built .so files. All plugins honour the same
 * CdcBridgeExchange ABI (defined in `cdc_bridge_exchange.h`), so the C++
 * side only sees opaque payload bytes flowing in / completion bits flowing
 * out β€” it does not know or care which CDC IP is doing the ferrying.
 *
 * Single in-flight transaction for now β€” the bridge denies any concurrent
 * request and fires retry() when it returns to idle.
 */

#pragma once

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/time/time_event.hpp>
#include "verilator_plugin.h"

// The exchange header lives next to the SV / shim under hw/ so both sides
// share one source of truth for the layout. Add an `-I` for it in CMake.
#include "cdc_bridge_exchange.h"


class IoV2CdcRtlBridge : public vp::Component
{
public:
    IoV2CdcRtlBridge(vp::ComponentConf &config);
    void start() override;
    void reset(bool active) override;
    void stop() override;

private:
    enum Phase
    {
        PHASE_IDLE,             // No request in flight
        PHASE_WAIT_FWD,         // Req marshaled, RTL ferrying request to slave
        PHASE_WAIT_DOWNSTREAM,  // Downstream slave processing (async GRANTED)
        PHASE_WAIT_REV,         // Resp marshaled, RTL ferrying response to master
    };

    // v2 IO callbacks
    static vp::IoReqStatus in_req_handler(vp::Block *__this, vp::IoReq *req);
    static void            out_resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            out_retry_handler(vp::Block *__this);
    static void            step_handler(vp::Block *_this, vp::TimeEvent *event);

    void marshal_req_into_exchange(vp::IoReq *req);
    void unmarshal_resp_from_exchange();
    void marshal_resp_into_exchange();
    void schedule_step(int64_t delta_ps);

    vp::IoSlave  in{&IoV2CdcRtlBridge::in_req_handler};
    vp::IoMaster out{&IoV2CdcRtlBridge::out_retry_handler,
                     &IoV2CdcRtlBridge::out_resp_handler};
    vp::TimeEvent step_event;
    vp::Trace     trace;

    // Plugin .so + vtable
    std::string plugin_path;
    void *handle = nullptr;
    const VlPluginVtable *vt = nullptr;
    VlPlugin *design = nullptr;

    // Shared exchange struct (host owns, plugin reads/writes via shared ptr)
    CdcBridgeExchange exchange{};

    // Cached engine pointers (so we know each side's period in ps)
    vp::ClockEngine *master_engine = nullptr;
    vp::ClockEngine *slave_engine  = nullptr;

    // Per-transaction state
    Phase phase = PHASE_IDLE;
    vp::IoReq *in_flight = nullptr;
    int reset_cycles_left = 8;
    // Stash of the data buffer we use to ship payload bytes when forwarding
    // a downstream req β€” keeps the original IoReq's data buf untouched.
    uint8_t downstream_data[CDC_PAYLOAD_BYTES];
};
