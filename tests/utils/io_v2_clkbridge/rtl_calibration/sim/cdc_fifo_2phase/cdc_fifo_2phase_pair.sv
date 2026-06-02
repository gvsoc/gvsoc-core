// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

// Top-level Verilator wrapper pairing two cdc_fifo_2phase instances β€”
// forward and reverse. cdc_fifo_2phase combines a small FIFO with a pair
// of cdc_2phase handshakes for the read/write pointers; it's the
// middle-ground IP between cdc_2phase (single beat, single in-flight) and
// cdc_fifo_gray (gray-pointer async FIFO).

module cdc_fifo_2phase_pair #(
    parameter int unsigned PAYLOAD_BITS = 304,
    parameter int unsigned RESP_BITS    = 144,
    parameter int LOG_DEPTH = 1
)(
    input  logic                    src_clk_i,
    input  logic                    dst_clk_i,
    input  logic                    src_rst_ni,
    input  logic                    dst_rst_ni,

    input  logic [PAYLOAD_BITS-1:0] fwd_src_data_i,
    input  logic                    fwd_src_valid_i,
    output logic                    fwd_src_ready_o,
    output logic [PAYLOAD_BITS-1:0] fwd_dst_data_o,
    output logic                    fwd_dst_valid_o,
    input  logic                    fwd_dst_ready_i,

    input  logic [RESP_BITS-1:0]    rev_src_data_i,
    input  logic                    rev_src_valid_i,
    output logic                    rev_src_ready_o,
    output logic [RESP_BITS-1:0]    rev_dst_data_o,
    output logic                    rev_dst_valid_o,
    input  logic                    rev_dst_ready_i
);

    cdc_fifo_2phase #(
        .T         (logic [PAYLOAD_BITS-1:0]),
        .LOG_DEPTH (LOG_DEPTH)
    ) i_fwd (
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

    cdc_fifo_2phase #(
        .T         (logic [RESP_BITS-1:0]),
        .LOG_DEPTH (LOG_DEPTH)
    ) i_rev (
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
