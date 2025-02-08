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

#include <functional>
#include <cpu/iss/include/cores/snitch_fast/ssr.hpp>
#include "cpu/iss/include/iss.hpp"
#include ISS_CORE_INC(class.hpp)

using namespace std::placeholders;

SsrStreamer::SsrStreamer(IssWrapper &top, Iss &iss, Block *parent, int id, std::string name,
    std::string memory_itf_name)
: vp::Block(parent, name), iss(iss), regmap(*this, "regmap")
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    top.new_master_port(memory_itf_name, &this->memory_itf, (vp::Block *)this);

    this->id = id;

    this->in_io_req.init();
    this->in_io_req.set_size(8);
    this->in_io_req.set_data((uint8_t *)&this->in_io_req_data);
    this->in_io_req.set_is_write(false);

    this->out_io_req.init();
    this->out_io_req.set_size(8);
    this->out_io_req.set_data((uint8_t *)&this->out_io_req_data);
    this->out_io_req.set_is_write(true);

    this->bounds = (vp_ssr_bounds_0 *)&this->regmap.bounds_0;
    this->strides = (vp_ssr_strides_0 *)&this->regmap.strides_0;
    this->rptr = (vp_ssr_rptr_0 *)&this->regmap.rptr_0;
    this->wptr = (vp_ssr_wptr_0 *)&this->regmap.wptr_0;

    for (int i=0; i<4; i++)
    {
        this->wptr[i].register_callback(std::bind(&SsrStreamer::wptr_req, this, _1, _2, _3, _4, i));
        this->rptr[i].register_callback(std::bind(&SsrStreamer::rptr_req, this, _1, _2, _3, _4, i));
    }
}

void SsrStreamer::wptr_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write, int dim)
{
    this->write_dim = dim;
    this->active = true;
    this->wptr[dim].update(reg_offset, size, value, is_write);

    if (is_write)
    {
        this->is_write = true;
    }
}

void SsrStreamer::rptr_req(uint64_t reg_offset, int size, uint8_t *value, bool is_write, int dim)
{
    this->read_dim = dim;
    this->active = true;
    this->rptr[dim].update(reg_offset, size, value, is_write);

    if (is_write)
    {
        this->is_write = false;
    }
}

void SsrStreamer::reset(bool active)
{
    if (active)
    {
        this->in_fifo_head = 0;
        this->in_fifo_tail = 0;
        this->in_fifo_nb_elem = 0;
        this->repeat_count = 0;
        for (int i=0; i<4; i++)
        {
            this->current_bounds[i] = 0;
        }
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

void SsrStreamer::push_data(uint64_t data)
{
    if (this->out_fifo_nb_elem < SsrStreamer::fifo_size)
    {
#ifdef VP_TRACE_ACTIVE
        // Also set FPU register for tracing.
        this->iss.regfile.fregs[this->id] = data;
#endif
        this->out_fifo[this->out_fifo_head] = data;
        this->out_fifo_head++;
        if (this->out_fifo_head == SsrStreamer::fifo_size)
        {
            this->out_fifo_head = 0;
        }
        this->out_fifo_nb_elem++;
    }
    else
    {
        printf("UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
    }
}

uint64_t SsrStreamer::pop_data()
{
    if (this->in_fifo_nb_elem > 0)
    {
        uint64_t value = this->in_fifo[this->in_fifo_head];

        this->repeat_count++;
        if (this->repeat_count > this->regmap.repeat.get())
        {
            this->repeat_count = 0;
            this->in_fifo_head++;
            if (this->in_fifo_head == SsrStreamer::fifo_size)
            {
                this->in_fifo_head = 0;
            }
            this->in_fifo_nb_elem--;

    #ifdef VP_TRACE_ACTIVE
            // If another value is present in the value, setup now the FPU register since
            // the fetch was not able to set it for tracing
            if (this->in_fifo_nb_elem > 0)
            {
                this->iss.regfile.fregs[this->id] = this->in_fifo[this->in_fifo_head];
            }
    #endif
        }

        return value;
    }
    else
    {
        printf("UNIMPLEMENTED AT %s %d\n", __FILE__, __LINE__);
        return 0;
    }
}

void SsrStreamer::handle_data()
{
    if (this->is_write)
    {
        if (this->out_fifo_nb_elem > 0)
        {
            vp::IoReq *req = &this->out_io_req;
            uint64_t addr = this->wptr[this->write_dim].get();

            req->prepare();
            req->set_addr(addr);

            this->out_io_req_data = this->out_fifo[this->out_fifo_tail];

            this->memory_itf.req(req);

            this->out_fifo_tail++;
            if (this->out_fifo_tail == SsrStreamer::fifo_size)
            {
                this->out_fifo_tail = 0;
            }
            this->out_fifo_nb_elem--;

            for (int i=0; i<this->write_dim + 1; i++)
            {
                this->wptr[this->write_dim].set(addr + this->strides[i].get());

                if (this->current_bounds[i] == this->bounds[i].get())
                {
                    this->current_bounds[i] = 0;
                }
                else
                {
                    this->current_bounds[i]++;
                    break;
                }
            }
        }
    }
    else
    {
        if (this->in_fifo_nb_elem < SsrStreamer::fifo_size && this->active)
        {
            vp::IoReq *req = &this->in_io_req;
            uint64_t addr = this->rptr[this->read_dim].get();

            req->prepare();
            req->set_addr(addr);

            this->memory_itf.req(req);

#ifndef VP_TRACE_ACTIVE
            // The case the FIFO is empty, also setup the FPU register so that instruction trace
            // can see the register content.
            if (this->in_fifo_nb_elem == 0)
            {
                this->iss.regfile.fregs[this->id] = this->in_io_req_data;
            }
#endif

            this->in_fifo[this->in_fifo_tail] = this->in_io_req_data;
            this->in_fifo_tail++;
            if (this->in_fifo_tail == SsrStreamer::fifo_size)
            {
                this->in_fifo_tail = 0;
            }
            this->in_fifo_nb_elem++;

            for (int i=0; i<this->read_dim + 1; i++)
            {
                this->rptr[this->read_dim].set(addr + this->strides[i].get());

                if (this->current_bounds[i] == this->bounds[i].get())
                {
                    this->current_bounds[i] = 0;

                    if (i == this->read_dim)
                    {
                        this->active = false;
                    }
                }
                else
                {
                    this->current_bounds[i]++;
                    break;
                }
            }
        }
    }
}

Ssr::Ssr(IssWrapper &top, Iss &iss)
: vp::Block(&top, "ssr"), top(top), iss(iss), fsm_event(this, &Ssr::fsm_event_handler)
{
    top.traces.new_trace("ssr", &this->trace, vp::DEBUG);

    this->streamers.reserve(3);
    for (int i=0; i<3; i++)
    {
        this->streamers.emplace_back(top, iss, &top, i, "ssr_streamer" + std::to_string(i),
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
        _this->streamers[i].handle_data();
    }
}

bool Ssr::ssr_access(bool is_write, iss_reg_t &value)
{
    this->ssr_enabled = value & 1;

    if (is_write)
    {
        if (this->ssr_enabled)
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
