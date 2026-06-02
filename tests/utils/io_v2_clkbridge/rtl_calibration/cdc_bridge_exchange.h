// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Shared POD struct between the io_v2_cdc_2phase_rtl GVSoC bridge component
 * (host side) and the Verilator plugin shim (RTL side). The bridge writes
 * the *_pending fields and reads the *_observed fields; the plugin shim
 * reads pending and writes observed. No locks β€” the host runs strictly
 * before / strictly after each plugin->step() call. Both sides see the
 * same memory layout because this header is included by both.
 *
 * The plugin exposes a dlsym'd setter `cdc_bridge_attach_exchange(plugin,
 * exchange*)` that the host calls right after open(). The vtable itself is
 * unchanged.
 */

#ifndef CDC_BRIDGE_EXCHANGE_H
#define CDC_BRIDGE_EXCHANGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Must match cdc_2phase_pair.sv parameters. Widths are in *bits*; we pack
 * into byte-aligned arrays on both sides. */
#define CDC_PAYLOAD_BITS 304
#define CDC_RESP_BITS    144
#define CDC_PAYLOAD_BYTES ((CDC_PAYLOAD_BITS + 7) / 8)   /* 38 */
#define CDC_RESP_BYTES    ((CDC_RESP_BITS    + 7) / 8)   /* 18 */

typedef struct {
    /* Clock configuration β€” set by the host before reset is released. The
     * plugin uses these to schedule its two clock edges. Half periods in
     * picoseconds. */
    int64_t src_half_period_ps;
    int64_t dst_half_period_ps;

    /* Reset β€” the host can pulse this and the plugin will run a few cycles
     * with rst_ni deasserted. */
    uint8_t  reset_active;

    /* ---- Forward direction: host pushes request, plugin ferries to dst ---- */

    /* Host -> plugin: a payload is waiting at fwd_payload, present as
     *   fwd_src_valid_i to the RTL. The plugin must clear fwd_src_pending
     *   only after it observes the corresponding src_ready handshake. */
    uint8_t  fwd_src_pending;
    uint8_t  fwd_payload[CDC_PAYLOAD_BYTES];

    /* Plugin -> host: a payload arrived at the dst side. The host reads
     *   fwd_dst_payload and clears fwd_dst_observed (which makes the plugin
     *   stop asserting fwd_dst_ready_i, completing the receive handshake). */
    uint8_t  fwd_dst_observed;
    uint8_t  fwd_dst_payload[CDC_PAYLOAD_BYTES];

    /* ---- Reverse direction: host pushes response, plugin ferries back ---- */

    uint8_t  rev_src_pending;
    uint8_t  rev_payload[CDC_RESP_BYTES];

    uint8_t  rev_dst_observed;
    uint8_t  rev_dst_payload[CDC_RESP_BYTES];

    /* Sanity/debug counters β€” updated by the plugin once per accepted
     *   request / completed response. Visible to the host's bench checker. */
    uint64_t fwd_completions;   /* # of times fwd_dst_valid_o fired */
    uint64_t rev_completions;   /* # of times rev_dst_valid_o fired */

} CdcBridgeExchange;

/* Exported by the plugin .so. The host dlsym's it after open(). */
typedef struct VlPlugin VlPlugin;  /* opaque, defined by the shim */
void cdc_bridge_attach_exchange(VlPlugin *plugin, CdcBridgeExchange *exchange);

#ifdef __cplusplus
}
#endif

#endif /* CDC_BRIDGE_EXCHANGE_H */
