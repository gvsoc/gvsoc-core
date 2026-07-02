// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

// Top-level Verilator wrapper that pairs two cdc_2phase instances β€” one for
// the request path (master domain -> slave domain) and one for the response
// path (slave domain -> master domain) β€” into a single design. Both halves
// share the same pair of clocks and resets but in mirrored roles, so the
// host sees one DUT with one consistent port surface.
//
// Bit widths are picked to comfortably hold the marshaled v2 IoReq payload
// (forward) and the marshaled response (backward). They are fixed because
// the simulator needs a concrete type at the top level β€” parametric
// `type T` works inside SystemVerilog but does not survive --top-module.

module cdc_2phase_pair #(
    parameter int unsigned PAYLOAD_BITS = 304,  // 64 (addr) + 32 (size) + 4 (opcode)
                                                 // + 16 (burst_id) + 8 (flags) + 128 (data) + 52 pad
    parameter int unsigned RESP_BITS    = 144   // 128 (data) + 8 (status) + 8 (flags)
)(
    // Two independent clock domains. The forward CDC's src is `src_clk_i`,
    // the reverse CDC's src is `dst_clk_i` (responses originate there).
    input  logic                    src_clk_i,
    input  logic                    dst_clk_i,
    input  logic                    src_rst_ni,
    input  logic                    dst_rst_ni,

    // Forward CDC β€” master domain (src) -> slave domain (dst).
    input  logic [PAYLOAD_BITS-1:0] fwd_src_data_i,
    input  logic                    fwd_src_valid_i,
    output logic                    fwd_src_ready_o,
    output logic [PAYLOAD_BITS-1:0] fwd_dst_data_o,
    output logic                    fwd_dst_valid_o,
    input  logic                    fwd_dst_ready_i,

    // Reverse CDC β€” slave domain (src) -> master domain (dst).
    input  logic [RESP_BITS-1:0]    rev_src_data_i,
    input  logic                    rev_src_valid_i,
    output logic                    rev_src_ready_o,
    output logic [RESP_BITS-1:0]    rev_dst_data_o,
    output logic                    rev_dst_valid_o,
    input  logic                    rev_dst_ready_i
);

    // Forward path: master clock is the source.
    cdc_2phase #(.T(logic [PAYLOAD_BITS-1:0])) i_fwd (
        .src_rst_ni  (src_rst_ni),
        .src_clk_i   (src_clk_i),
        .src_data_i  (fwd_src_data_i),
        .src_valid_i (fwd_src_valid_i),
        .src_ready_o (fwd_src_ready_o),
        .dst_rst_ni  (dst_rst_ni),
        .dst_clk_i   (dst_clk_i),
        .dst_data_o  (fwd_dst_data_o),
        .dst_valid_o (fwd_dst_valid_o),
        .dst_ready_i (fwd_dst_ready_i)
    );

    // Reverse path: slave clock is the source, master clock the destination.
    cdc_2phase #(.T(logic [RESP_BITS-1:0])) i_rev (
        .src_rst_ni  (dst_rst_ni),
        .src_clk_i   (dst_clk_i),
        .src_data_i  (rev_src_data_i),
        .src_valid_i (rev_src_valid_i),
        .src_ready_o (rev_src_ready_o),
        .dst_rst_ni  (src_rst_ni),
        .dst_clk_i   (src_clk_i),
        .dst_data_o  (rev_dst_data_o),
        .dst_valid_o (rev_dst_valid_o),
        .dst_ready_i (rev_dst_ready_i)
    );

endmodule
