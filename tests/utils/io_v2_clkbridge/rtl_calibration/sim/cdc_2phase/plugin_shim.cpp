// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Verilator plugin shim for the cdc_2phase_pair design β€” two cdc_2phase
 * instances (forward + reverse) wrapped under one DUT.
 *
 * The shim:
 *   - drives both clocks at independent periods supplied by the host via a
 *     CdcBridgeExchange struct (set after open() via the dlsym'd
 *     cdc_bridge_attach_exchange);
 *   - on every step() picks the nearest of the two next edges, advances
 *     verilated time to it, toggles that clock, evaluates the design;
 *   - on each src/dst rising edge it inspects the valid/ready handshake and
 *     updates the exchange struct (clearing *_pending on a captured src
 *     transfer; copying the data out and setting *_observed on a captured
 *     dst transfer);
 *   - returns the time-to-next half-edge as the host's wake-up delta.
 *
 * No internal state machine, no stimulus β€” the shim is purely a transport
 * layer for the bridge component's IO marshaling logic.
 *
 * Timescale is 1ps/1ps so ctx->time() and host time_to_next units match.
 */

#include "verilator_plugin_helpers.hpp"
#include "Vcdc_2phase_pair.h"
#include "cdc_bridge_exchange.h"

#include <algorithm>
#include <cstring>


namespace {

// Pack `nbytes` from `src_bytes` into a Verilator wide-word array starting
// at `dst_words`. Word stride is 32 bits; LSByte first, matching how
// Verilator stores `logic [N-1:0]` little-endian.
template <typename WordT>
inline void pack_bytes_to_words(const uint8_t *src_bytes, int nbytes,
                                WordT *dst_words, int nwords) {
    for (int i = 0; i < nwords; i++) dst_words[i] = 0;
    for (int i = 0; i < nbytes; i++) {
        dst_words[i >> 2] |= ((WordT)src_bytes[i]) << ((i & 3) * 8);
    }
}

template <typename WordT>
inline void unpack_words_to_bytes(const WordT *src_words, int nwords,
                                  uint8_t *dst_bytes, int nbytes) {
    (void)nwords;
    for (int i = 0; i < nbytes; i++) {
        dst_bytes[i] = (uint8_t)((src_words[i >> 2] >> ((i & 3) * 8)) & 0xff);
    }
}

}  // namespace


struct VlPlugin : gv_vlplugin::PluginCommon<Vcdc_2phase_pair> {
    CdcBridgeExchange *exchange = nullptr;
    int64_t next_src_edge_ps = 0;
    int64_t next_dst_edge_ps = 0;
    // Until the host attaches an exchange, we spin slowly to avoid stalling
    // the engine. 10 us is far longer than any real CDC bring-up.
    static constexpr int64_t IDLE_POLL_PS = 10'000'000;
};


// dlsym'd by the bridge component right after dlopen + open().
extern "C" void cdc_bridge_attach_exchange(VlPlugin *plugin,
                                            CdcBridgeExchange *exchange) {
    if (plugin == nullptr) return;
    plugin->exchange = exchange;
    // Seed the next-edge tracker so the first step picks an edge at the
    // first configured half-period in the future. ctx->time() is whatever
    // the engine called us with so far β€” usually 0.
    int64_t now = plugin->ctx->time();
    plugin->next_src_edge_ps = now + exchange->src_half_period_ps;
    plugin->next_dst_edge_ps = now + exchange->dst_half_period_ps;
}


static VlPlugin *vl_open(int argc, const char *const *argv) {
    auto *p = new VlPlugin();
    p->ctx.reset(new VerilatedContext());
    p->ctx->commandArgs(argc, const_cast<char **>(argv));
    p->ctx->traceEverOn(true);
    p->dut.reset(new Vcdc_2phase_pair(p->ctx.get()));

    // Defaults: clocks low, resets asserted, no traffic.
    p->dut->src_clk_i = 0;
    p->dut->dst_clk_i = 0;
    p->dut->src_rst_ni = 0;
    p->dut->dst_rst_ni = 0;
    p->dut->fwd_src_valid_i = 0;
    p->dut->fwd_dst_ready_i = 0;
    p->dut->rev_src_valid_i = 0;
    p->dut->rev_dst_ready_i = 0;
    // Wide inputs cleared by Verilator's default ctor; explicit zero anyway
    // so a deliberate-init Verilator warning never surfaces here.

    // ps_per_vcd_tick = 1 matches the --timescale 1ps/1ps in the Makefile.
    p->setup_trace(gv_vlplugin::parse_plusargs(argc, argv),
                   /*ps_per_vcd_tick=*/1, "cdc_2phase.fst");
    return p;
}


static VlStepResult vl_step(VlPlugin *p) {
    CdcBridgeExchange *ex = p->exchange;
    if (ex == nullptr) {
        // Host hasn't attached yet β€” idle a long time, the engine will call
        // us again. The bridge attaches before reset deassertion, so this
        // typically only fires once.
        return VlStepResult{-1, VlPlugin::IDLE_POLL_PS};
    }

    // Pick the nearest edge and advance time to it. Both halves may coincide
    // (e.g. equal-frequency case), in which case we toggle both clocks in
    // the same eval().
    int64_t now_ps = p->ctx->time();
    int64_t edge_at = std::min(p->next_src_edge_ps, p->next_dst_edge_ps);
    int64_t delta = edge_at - now_ps;
    if (delta < 0) delta = 0;
    if (delta > 0) p->ctx->timeInc(delta);

    bool src_edge = (p->next_src_edge_ps == edge_at);
    bool dst_edge = (p->next_dst_edge_ps == edge_at);

    // Drive non-clock inputs from the exchange BEFORE toggling clocks so
    // setup is met on the rising edge.
    p->dut->src_rst_ni = ex->reset_active ? 0 : 1;
    p->dut->dst_rst_ni = ex->reset_active ? 0 : 1;

    p->dut->fwd_src_valid_i = ex->fwd_src_pending ? 1 : 0;
    pack_bytes_to_words(ex->fwd_payload, CDC_PAYLOAD_BYTES,
                        &p->dut->fwd_src_data_i[0],
                        (CDC_PAYLOAD_BITS + 31) / 32);
    // dst_ready high while there's no pending observation backed up.
    p->dut->fwd_dst_ready_i = ex->fwd_dst_observed ? 0 : 1;

    p->dut->rev_src_valid_i = ex->rev_src_pending ? 1 : 0;
    pack_bytes_to_words(ex->rev_payload, CDC_RESP_BYTES,
                        &p->dut->rev_src_data_i[0],
                        (CDC_RESP_BITS + 31) / 32);
    p->dut->rev_dst_ready_i = ex->rev_dst_observed ? 0 : 1;

    // Toggle whichever clock(s) edge fired now.
    if (src_edge) {
        p->dut->src_clk_i ^= 1;
        p->next_src_edge_ps += ex->src_half_period_ps;
    }
    if (dst_edge) {
        p->dut->dst_clk_i ^= 1;
        p->next_dst_edge_ps += ex->dst_half_period_ps;
    }

    p->dut->eval();

    // Sample handshakes after the edge. Only act on RISING edges β€” that's
    // where valid/ready captures happen. Gate every capture on the
    // corresponding rst_ni being high, because cdc_2phase's ready_o defaults
    // to 1 under reset and we would otherwise spuriously clear pending bits
    // before the RTL has actually latched anything.
    if (src_edge && p->dut->src_clk_i) {
        // Master-domain captures: forward request accepted, reverse response
        // delivered into the master domain.
        if (p->dut->src_rst_ni
            && p->dut->fwd_src_valid_i && p->dut->fwd_src_ready_o) {
            ex->fwd_src_pending = 0;
        }
        if (p->dut->src_rst_ni
            && p->dut->rev_dst_ready_i && p->dut->rev_dst_valid_o
            && !ex->rev_dst_observed) {
            unpack_words_to_bytes(&p->dut->rev_dst_data_o[0],
                                  (CDC_RESP_BITS + 31) / 32,
                                  ex->rev_dst_payload, CDC_RESP_BYTES);
            ex->rev_dst_observed = 1;
            ex->rev_completions++;
        }
    }
    if (dst_edge && p->dut->dst_clk_i) {
        // Slave-domain captures: forward request delivered, reverse response
        // accepted.
        if (p->dut->dst_rst_ni
            && p->dut->rev_src_valid_i && p->dut->rev_src_ready_o) {
            ex->rev_src_pending = 0;
        }
        if (p->dut->dst_rst_ni
            && p->dut->fwd_dst_ready_i && p->dut->fwd_dst_valid_o
            && !ex->fwd_dst_observed) {
            unpack_words_to_bytes(&p->dut->fwd_dst_data_o[0],
                                  (CDC_PAYLOAD_BITS + 31) / 32,
                                  ex->fwd_dst_payload, CDC_PAYLOAD_BYTES);
            ex->fwd_dst_observed = 1;
            ex->fwd_completions++;
        }
    }

    if (p->tfp != nullptr) p->tfp->dump(p->ctx->time());

    int64_t next_at = std::min(p->next_src_edge_ps, p->next_dst_edge_ps);
    int64_t next_delta = next_at - p->ctx->time();
    if (next_delta < 1) next_delta = 1;
    return VlStepResult{-1, next_delta};
}


GV_VERILATOR_PLUGIN_DEFINE(vl_open, vl_step)
