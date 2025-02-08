 /*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

 #pragma once

 #include "cpu/iss/include/types.hpp"
 #include <cpu/iss/include/csr.hpp>
 #include <pulp/snitch/archi/ssr_gvsoc.h>

class IssWrapper;

class SsrStreamer : public vp::Block
{
public:
    SsrStreamer(IssWrapper &top, Iss &iss, Block *parent, int id, std::string name, std::string memory_itf_name);

    void reset(bool active);
    bool cfg_access(uint64_t offset, uint8_t *value, bool is_write);
    void enable();
    void disable();
    void handle_data();
    uint64_t pop_data();
    void push_data(uint64_t data);

private:
    void wptr_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write, int dim);
    void rptr_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write, int dim);
    static constexpr int fifo_size = 4;

    Iss &iss;
    int id;
    vp::Trace trace;
    vp_regmap_ssr regmap;
    vp::IoMaster memory_itf;
    vp::IoReq in_io_req;
    uint64_t in_io_req_data;
    vp::IoReq out_io_req;
    uint64_t out_io_req_data;

    uint64_t in_fifo[fifo_size];
    int in_fifo_head;
    int in_fifo_tail;
    int in_fifo_nb_elem;

    uint64_t out_fifo[fifo_size];
    int out_fifo_head;
    int out_fifo_tail;
    int out_fifo_nb_elem;

    bool is_write;

    vp_ssr_bounds_0 *bounds;
    vp_ssr_strides_0 *strides;
    vp_ssr_rptr_0 *rptr;
    vp_ssr_wptr_0 *wptr;

    uint64_t current_bounds[4];

    uint64_t repeat_count;

    int read_dim;
    int write_dim;

    bool active;
};

class Ssr : public vp::Block
{
public:

    Ssr(IssWrapper &top, Iss &iss);

    void reset(bool active);

    void cfg_write(iss_insn_t *insn, int reg, int ssr, iss_reg_t value);
    iss_reg_t cfg_read(iss_insn_t *insn, int reg, int ssr);
    bool ssr_is_enabled() { return this->ssr_enabled; }
    inline uint64_t pop_data(int ssr) { return this->streamers[ssr].pop_data(); }
    inline void push_data(int ssr, uint64_t data) { this->streamers[ssr].push_data(data); }

private:
    static void fsm_event_handler(vp::Block *__this, vp::ClockEvent *event);
    bool ssr_access(bool is_write, iss_reg_t &value);
    void enable();
    void disable();

    IssWrapper &top;
    Iss &iss;
    std::vector<SsrStreamer> streamers;
    vp::ClockEvent fsm_event;

    vp::Trace trace;
    CsrReg csr_ssr;

    bool ssr_enabled;
};
