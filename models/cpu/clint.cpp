// Copyright (c) 2010-2017, The Regents of the University of California
// (Regents).  All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the Regents nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
// OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
// HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
// MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>

/* 0000 msip hart 0
 * 0004 msip hart 1
 * 4000 mtimecmp hart 0 lo
 * 4004 mtimecmp hart 0 hi
 * 4008 mtimecmp hart 1 lo
 * 400c mtimecmp hart 1 hi
 * bff8 mtime lo
 * bffc mtime hi
 */

#define RESOLUTION 1000000

#define MSIP_BASE 0x0
#define MTIMECMP_BASE 0x4000
#define MTIME_BASE 0xbff8
#define CLINT_SIZE         0x000c0000


class Clint : public vp::Component
{
public:
    Clint(vp::ComponentConf &conf);

    void reset(bool active);

private:
    static void event_handler(vp::Block *_this, vp::ClockEvent *event);
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void time_sync_back(void *__this, uint64_t *value);
    bool load(unsigned int addr, size_t len, uint8_t* bytes);
    bool store(unsigned int addr, size_t len, const uint8_t* bytes);
    uint64_t get_mtime();
    void check_mtime();
    void check_event();

    vp::IoSlave input_itf;

    vp::Trace trace;
    typedef uint64_t mtime_t;
    typedef uint64_t mtimecmp_t;
    typedef uint32_t msip_t;
    std::vector<mtimecmp_t> mtimecmp;

    std::vector<vp::WireMaster<bool>> sw_irq_itf;
    std::vector<vp::WireMaster<bool>> timer_irq_itf;
    vp::WireSlave<uint64_t> time_itf;

    int nb_cores;
    std::vector<msip_t> msip;
    int64_t start_time;

    std::vector<vp::ClockEvent *> event;
};

void Clint::reset(bool active)
{
    if (active)
    {
        this->start_time = this->time.get_time();
        for (int i=0; i<this->nb_cores; i++)
        {
            this->msip[i] = 0;
            this->mtimecmp[i] = 0;
        }
    }
}

void Clint::event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Clint *_this = (Clint *)__this;
    _this->check_mtime();
    _this->check_event();
}


void Clint::time_sync_back(void *__this, uint64_t *value)
{
    Clint *_this = (Clint *)__this;
    *value = _this->get_mtime();
}



vp::IoReqStatus Clint::req(vp::Block *__this, vp::IoReq *req)
{
    Clint *_this = (Clint *)__this;
    bool status;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received request (offset: 0x%x, size: 0x%d, is_write: %d)\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    if (req->get_is_write())
    {
        status = _this->store(req->get_addr(), req->get_size(), req->get_data());
    }
    else
    {
        status = _this->load(req->get_addr(), req->get_size(), req->get_data());
    }
    return status ? vp::IO_REQ_OK : vp::IO_REQ_INVALID;
}

uint64_t Clint::get_mtime()
{
    uint64_t value = (this->time.get_time() - this->start_time) / RESOLUTION;
    return value;
}

void Clint::check_mtime()
{
    uint64_t mtime = this->get_mtime();
    for (int i=0; i<this->nb_cores; i++)
    {
        if (this->timer_irq_itf[i].is_bound())
        {
            bool value = mtime >= this->mtimecmp[i];
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Sync interrupt (value: 0x%d)\n", value);
            this->timer_irq_itf[i].sync(value);
        }
    }
}

void Clint::check_event()
{
    for (int i=0; i<this->nb_cores; i++)
    {
        uint64_t diff = (this->mtimecmp[i] - this->get_mtime()) * RESOLUTION / this->clock.get_period();

        if (diff > 0)
        {
            this->event[i]->enqueue(diff);
        }
    }
}

bool Clint::load(unsigned int addr, size_t len, uint8_t *bytes)
{
    if (addr >= MSIP_BASE && addr + len <= MSIP_BASE + this->nb_cores * sizeof(msip_t))
    {
        memcpy(bytes, (uint8_t *)&msip[0] + addr - MSIP_BASE, len);
    }
    else if (addr >= MTIMECMP_BASE && addr + len <= MTIMECMP_BASE + this->nb_cores * sizeof(mtimecmp_t))
    {
        memcpy(bytes, (uint8_t *)&mtimecmp[0] + addr - MTIMECMP_BASE, len);
    }
    else if (addr >= MTIME_BASE && addr + len <= MTIME_BASE + sizeof(mtime_t))
    {
        uint64_t mtime = this->get_mtime();
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Returning mtime (value: 0x%lx)\n", mtime);

        memcpy(bytes, (uint8_t *)&mtime + addr - MTIME_BASE, len);
    }
    else if (addr + len <= CLINT_SIZE)
    {
        memset(bytes, 0, len);
    }
    else
    {
        return false;
    }
    return true;
}

bool Clint::store(unsigned int addr, size_t len, const uint8_t *bytes)
{
    if (addr >= MSIP_BASE && addr + len <= MSIP_BASE + this->nb_cores * sizeof(msip_t))
    {
        memcpy((uint8_t *)&msip[0] + addr - MSIP_BASE, bytes, len);
        for (size_t i = 0; i < this->nb_cores; ++i)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Updating SW interrupt (hart: %d, value: %d)\n",
                i, msip[i] & 1);
            if (this->sw_irq_itf[i].is_bound())
            {
                this->sw_irq_itf[i].sync(msip[i] & 1);
            }
        }
    }
    else if (addr >= MTIMECMP_BASE && addr + len <= MTIMECMP_BASE + this->nb_cores * sizeof(mtimecmp_t))
    {
        memcpy((uint8_t *)&mtimecmp[0] + addr - MTIMECMP_BASE, bytes, len);
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Updating mtimecmp (hart: %d, value: %d)\n",
            addr - MTIMECMP_BASE, mtimecmp[addr - MTIMECMP_BASE]);
        this->check_mtime();
        this->check_event();
    }
    else if (addr >= MTIME_BASE && addr + len <= MTIME_BASE + sizeof(mtime_t))
    {
        uint64_t mtime = this->get_mtime();
        memcpy((uint8_t *)&mtime + addr - MTIME_BASE, bytes, len);
        this->start_time = this->time.get_time() - mtime;
        this->check_mtime();
        this->check_event();
    }
    else if (addr + len <= CLINT_SIZE)
    {
        // Do nothing
    }
    else
    {
        return false;
    }
    return true;
}

Clint::Clint(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->time_itf.set_sync_back_meth(&Clint::time_sync_back);
    new_slave_port("time", &this->time_itf);

    this->input_itf.set_req_meth(&Clint::req);
    new_slave_port("input", &this->input_itf);

    this->nb_cores = this->get_js_config()->get_child_int("nb_cores");

    this->event.resize(this->nb_cores);
    this->mtimecmp.resize(this->nb_cores);
    this->msip.resize(this->nb_cores);
    this->sw_irq_itf.resize(this->nb_cores);
    this->timer_irq_itf.resize(this->nb_cores);
    for (int i=0; i<this->nb_cores; i++)
    {
        this->new_master_port("sw_irq_" + std::to_string(i), &this->sw_irq_itf[i]);
        this->new_master_port("timer_irq_" + std::to_string(i), &this->timer_irq_itf[i]);
        this->event[i] = event_new(Clint::event_handler);
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Clint(config);
}
