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

#include <cpu/iss/include/cores/snitch_fast/ssr.hpp>
#include "cpu/iss/include/iss.hpp"
#include ISS_CORE_INC(class.hpp)


SsrStreamer::SsrStreamer(IssWrapper &top, Iss &iss, Block *parent, std::string name,
    std::string memory_itf_name)
: vp::Block(parent, name), regmap(*this, "regmap")
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    top.new_master_port(memory_itf_name, &this->memory_itf, (vp::Block *)this);

    this->io_req.init();
    this->io_req.set_size(8);
    this->io_req.set_data((uint8_t *)this->io_req_data);
}

void SsrStreamer::reset(bool active)
{
    if (active)
    {
        this->fifo_head = 0;
        this->fifo_tail = 0;
        this->fifo_nb_elem = 0;
    }
}

bool SsrStreamer::cfg_access(uint64_t offset, uint8_t *value, bool is_write)
{
    return this->regmap.access(offset, 4, value, is_write);
}

void SsrStreamer::enable()
{
}

void SsrStreamer::disable()
{

}

void SsrStreamer::fetch_data()
{
    if (this->fifo_nb_elem < SsrStreamer::fifo_size && this->regmap.bounds_0.get() != 0)
    {
        vp::IoReq *req = &this->io_req;
        uint64_t addr = this->regmap.rptr_0.get();

        req->prepare();
        req->set_addr(addr);
        req->set_is_write(false);

        this->memory_itf.req(req);

        this->fifo[this->fifo_tail] = this->io_req_data;
        this->fifo_tail++;
        if (this->fifo_tail == SsrStreamer::fifo_size)
        {
            this->fifo_tail = 0;
        }
        this->fifo_nb_elem++;

        this->regmap.bounds_0.set(this->regmap.bounds_0.get() - 1);
        this->regmap.rptr_0.set(addr + 8);
    }
}

Ssr::Ssr(IssWrapper &top, Iss &iss)
: vp::Block(&top, "ssr"), top(top), iss(iss), fsm_event(this, &Ssr::fsm_event_handler)
{
    top.traces.new_trace("ssr", &this->trace, vp::DEBUG);

    this->streamers.reserve(3);
    for (int i=0; i<3; i++)
    {
        this->streamers.emplace_back(top, iss, &top, "ssr_streamer" + std::to_string(i),
            "ssr_dm" + std::to_string(i));
    }

    iss.csr.declare_csr(&this->csr_ssr, "ssr", 0x7C0);
    this->csr_ssr.register_callback(std::bind(&Ssr::ssr_access, this, std::placeholders::_1,
        std::placeholders::_2));
}


void Ssr::reset(bool active)
{
    for (int i=0; i<3; i++)
    {
        this->streamers[i].reset(active);
    }
}

void Ssr::fsm_event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Ssr *_this = (Ssr *)__this;

    for (int i=0; i<3; i++)
    {
        _this->streamers[i].fetch_data();
    }
}

bool Ssr::ssr_access(bool is_write, iss_reg_t &value)
{
    if (is_write)
    {
        if (value & 1)
        {
            this->enable();
        }
        else
        {
            this->disable();
        }
    }
    return true;
}

void Ssr::enable()
{
    for (int i=0; i<3; i++)
    {
        this->streamers[i].enable();
    }

    this->fsm_event.enable();
}

void Ssr::disable()
{
    for (int i=0; i<3; i++)
    {
        this->streamers[i].disable();
    }

    this->fsm_event.disable();
}

iss_reg_t Ssr::cfg_read(iss_insn_t *insn, int reg, int ssr)
{
    iss_reg_t value;
    this->streamers[ssr].cfg_access(reg * 4, (uint8_t *)&value, false);
    return value;
}

void Ssr::cfg_write(iss_insn_t *insn, int reg, int ssr, iss_reg_t value)
{
    if (ssr == 31)
    {
        for (int i=0; i<3; i++)
        {
            this->streamers[i].cfg_access(reg * 4, (uint8_t *)&value, true);
        }
    }
    else if (ssr < 3)
    {
        this->streamers[ssr].cfg_access(reg * 4, (uint8_t *)&value, true);
    }
}
