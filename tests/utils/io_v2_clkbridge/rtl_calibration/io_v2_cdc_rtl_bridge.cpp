// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include "io_v2_cdc_rtl_bridge.hpp"

#include <dlfcn.h>
#include <cstring>


// Payload layout (must agree with how the host code packs/unpacks bytes;
// the SV side just sees an opaque 304-bit vector).
//   bytes [0..7]   addr (uint64_t little-endian)
//   bytes [8..11]  size (uint32_t little-endian)
//   byte  [12]     opcode
//   bytes [13..14] burst_id (lower 16 bits, signed)
//   byte  [15]     flags: bit0 is_write, bit1 is_first, bit2 is_last
//   bytes [16..31] data (up to 16 bytes copied verbatim)
//   bytes [32..37] pad
//
// Response layout (144 bits):
//   bytes [0..15]  data
//   byte  [16]     status (IO_RESP_OK=0, IO_RESP_INVALID=1)
//   byte  [17]     flags (reserved; carries is_last for future bursts)


// Plugin's dlsym'd setter type. Defined in cdc_bridge_exchange.h.
typedef void (*CdcAttachExchangeFn)(VlPlugin *, CdcBridgeExchange *);


IoV2CdcRtlBridge::IoV2CdcRtlBridge(vp::ComponentConf &config)
    : vp::Component(config),
      step_event(this, &IoV2CdcRtlBridge::step_handler)
{
    traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_slave_port("input", &this->in);
    this->new_master_port("output", &this->out);

    js::Config *js = this->get_js_config();
    js::Config *path_cfg = js->get("plugin_path");
    if (path_cfg == nullptr)
    {
        this->trace.fatal("io_v2_cdc_rtl_bridge: missing 'plugin_path' property\n");
    }
    this->plugin_path = path_cfg->get_str();
}


void IoV2CdcRtlBridge::start()
{
    // 1. dlopen the plugin.
    this->handle = dlopen(this->plugin_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (this->handle == nullptr)
    {
        this->trace.fatal("dlopen failed for '%s': %s\n",
            this->plugin_path.c_str(), dlerror());
    }

    auto get_vt = (const VlPluginVtable *(*)())
        dlsym(this->handle, "gv_verilator_plugin_get");
    if (get_vt == nullptr)
    {
        this->trace.fatal("missing 'gv_verilator_plugin_get' in '%s'\n",
            this->plugin_path.c_str());
    }
    this->vt = get_vt();

    auto attach = (CdcAttachExchangeFn)
        dlsym(this->handle, "cdc_bridge_attach_exchange");
    if (attach == nullptr)
    {
        this->trace.fatal("missing 'cdc_bridge_attach_exchange' in '%s'\n",
            this->plugin_path.c_str());
    }

    // 2. Construct the design. No plusargs needed for the headless case.
    const char *argv[] = {"gvsoc-cdc-rtl-bridge"};
    this->design = this->vt->open(1, argv);
    if (this->design == nullptr)
    {
        this->trace.fatal("plugin->open() failed\n");
    }

    // 3. Resolve both engines via the remote-context owners. The bridge's
    //    own clock IS the master engine; the slave engine is reached via
    //    out's remote owner.
    auto *master_block = static_cast<vp::Block *>(this->in.get_remote_context());
    auto *slave_block  = static_cast<vp::Block *>(this->out.get_remote_context());
    if (master_block == nullptr || slave_block == nullptr)
    {
        this->trace.fatal("bridge not fully bound (in.bound=%d, out.bound=%d)\n",
                          master_block != nullptr, slave_block != nullptr);
    }
    this->master_engine = master_block->clock.get_engine();
    this->slave_engine  = slave_block->clock.get_engine();

    int64_t src_period = this->master_engine->get_period();
    int64_t dst_period = this->slave_engine->get_period();
    this->exchange.src_half_period_ps = src_period / 2;
    this->exchange.dst_half_period_ps = dst_period / 2;
    if (this->exchange.src_half_period_ps <= 0)
        this->exchange.src_half_period_ps = 1;
    if (this->exchange.dst_half_period_ps <= 0)
        this->exchange.dst_half_period_ps = 1;
    this->exchange.reset_active = 1;

    // 4. Hand the exchange to the plugin.
    attach(this->design, &this->exchange);

    this->trace.msg(vp::Trace::LEVEL_INFO,
        "Bridge attached: plugin='%s' src_period=%ld ps dst_period=%ld ps\n",
        this->plugin_path.c_str(), src_period, dst_period);
}


void IoV2CdcRtlBridge::reset(bool active)
{
    if (!active)
    {
        this->phase = PHASE_IDLE;
        this->in_flight = nullptr;
        this->reset_cycles_left = 8;
        this->exchange.reset_active = 1;
        this->exchange.fwd_src_pending = 0;
        this->exchange.fwd_dst_observed = 0;
        this->exchange.rev_src_pending = 0;
        this->exchange.rev_dst_observed = 0;
        // Kick the first plugin step "now". The plugin will toggle clocks
        // at exchange.src/dst_half_period_ps and progress from there.
        this->schedule_step(0);
    }
    else
    {
        this->step_event.cancel();
    }
}


void IoV2CdcRtlBridge::stop()
{
    if (this->design != nullptr && this->vt != nullptr)
    {
        this->vt->close(this->design);
        this->design = nullptr;
    }
    if (this->handle != nullptr)
    {
        dlclose(this->handle);
        this->handle = nullptr;
    }
}


void IoV2CdcRtlBridge::schedule_step(int64_t delta_ps)
{
    if (delta_ps < 0) delta_ps = 0;
    if (this->step_event.is_enqueued())
    {
        this->step_event.cancel();
    }
    this->step_event.enqueue(delta_ps);
}


// ---- Marshal / unmarshal --------------------------------------------------

void IoV2CdcRtlBridge::marshal_req_into_exchange(vp::IoReq *req)
{
    uint8_t *p = this->exchange.fwd_payload;
    std::memset(p, 0, CDC_PAYLOAD_BYTES);

    uint64_t addr = req->get_addr();
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(addr >> (i * 8));

    uint32_t size = (uint32_t)req->get_size();
    for (int i = 0; i < 4; i++) p[8 + i] = (uint8_t)(size >> (i * 8));

    p[12] = (uint8_t)req->get_opcode();
    int16_t bid = (int16_t)req->burst_id;
    p[13] = (uint8_t)(bid & 0xff);
    p[14] = (uint8_t)((bid >> 8) & 0xff);

    uint8_t flags = 0;
    if (req->get_is_write())  flags |= 1 << 0;
    if (req->is_first)        flags |= 1 << 1;
    if (req->is_last)         flags |= 1 << 2;
    p[15] = flags;

    uint64_t copy_bytes = std::min<uint64_t>(size, 16);
    if (req->get_data() != nullptr && copy_bytes > 0)
    {
        std::memcpy(&p[16], req->get_data(), copy_bytes);
    }
}


void IoV2CdcRtlBridge::unmarshal_resp_from_exchange()
{
    if (this->in_flight == nullptr) return;
    const uint8_t *p = this->exchange.rev_dst_payload;

    uint64_t size = this->in_flight->get_size();
    uint64_t copy_bytes = std::min<uint64_t>(size, 16);
    if (!this->in_flight->get_is_write()
        && this->in_flight->get_data() != nullptr
        && copy_bytes > 0)
    {
        std::memcpy(this->in_flight->get_data(), p, copy_bytes);
    }
    uint8_t status = p[16];
    this->in_flight->set_resp_status(
        status == 0 ? vp::IO_RESP_OK : vp::IO_RESP_INVALID);
}


void IoV2CdcRtlBridge::marshal_resp_into_exchange()
{
    if (this->in_flight == nullptr) return;
    uint8_t *p = this->exchange.rev_payload;
    std::memset(p, 0, CDC_RESP_BYTES);

    uint64_t size = this->in_flight->get_size();
    uint64_t copy_bytes = std::min<uint64_t>(size, 16);
    if (!this->in_flight->get_is_write()
        && this->in_flight->get_data() != nullptr
        && copy_bytes > 0)
    {
        std::memcpy(p, this->in_flight->get_data(), copy_bytes);
    }
    p[16] = (this->in_flight->get_resp_status() == vp::IO_RESP_OK) ? 0 : 1;
}


// ---- v2 IO callbacks ------------------------------------------------------

vp::IoReqStatus IoV2CdcRtlBridge::in_req_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2CdcRtlBridge *self = static_cast<IoV2CdcRtlBridge *>(__this);

    if (self->phase != PHASE_IDLE)
    {
        return vp::IO_REQ_DENIED;
    }

    self->in_flight = req;
    self->marshal_req_into_exchange(req);
    self->exchange.fwd_src_pending = 1;
    self->phase = PHASE_WAIT_FWD;
    self->schedule_step(0);
    return vp::IO_REQ_GRANTED;
}


vp::IoRespAck IoV2CdcRtlBridge::out_resp_handler(vp::Block *__this, vp::IoReq *req)
{
    IoV2CdcRtlBridge *self = static_cast<IoV2CdcRtlBridge *>(__this);
    self->marshal_resp_into_exchange();
    self->exchange.rev_src_pending = 1;
    self->phase = PHASE_WAIT_REV;
    self->schedule_step(0);
    return vp::IO_RESP_ACCEPTED;
}


void IoV2CdcRtlBridge::out_retry_handler(vp::Block *, vp::IoRetryChannel)
{
    // Downstream became ready after a DENIED. Our adapter currently doesn't
    // re-issue (the downstream slave in the v2 IO ports we connect to is
    // always sync-DONE). If we ever bind to a slave that denies, this is
    // where we'd retry the out.req() call.
}


// ---- Step driver ----------------------------------------------------------

void IoV2CdcRtlBridge::step_handler(vp::Block *_this, vp::TimeEvent *)
{
    IoV2CdcRtlBridge *self = static_cast<IoV2CdcRtlBridge *>(_this);
    VlStepResult r = self->vt->step(self->design);

    if (self->reset_cycles_left > 0)
    {
        if (--self->reset_cycles_left == 0)
        {
            self->exchange.reset_active = 0;
        }
    }

    if (self->phase == PHASE_WAIT_FWD)
    {
        if (self->exchange.fwd_dst_observed)
        {
            // The forward CDC delivered our payload to the slave domain.
            // Build a downstream IoReq from the latched fields and shoot it.
            self->phase = PHASE_WAIT_DOWNSTREAM;
            self->exchange.fwd_dst_observed = 0;

            vp::IoReq *req = self->in_flight;
            req->prepare();
            vp::IoReqStatus st = self->out.req(req);
            if (st == vp::IO_REQ_DONE)
            {
                self->marshal_resp_into_exchange();
                self->exchange.rev_src_pending = 1;
                self->phase = PHASE_WAIT_REV;
            }
            // IO_REQ_GRANTED: wait for resp callback.
            // IO_REQ_DENIED: stays in WAIT_DOWNSTREAM until retry re-issues.
        }
    }
    else if (self->phase == PHASE_WAIT_REV)
    {
        if (self->exchange.rev_dst_observed)
        {
            self->unmarshal_resp_from_exchange();
            self->exchange.rev_dst_observed = 0;

            vp::IoReq *req = self->in_flight;
            self->in_flight = nullptr;
            self->phase = PHASE_IDLE;
            self->in.resp(req);
            self->in.retry();
        }
    }

    int64_t delta = r.time_to_next;
    if (delta < 1) delta = 1;
    self->step_event.enqueue(delta);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new IoV2CdcRtlBridge(config);
}
