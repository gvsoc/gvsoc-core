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
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

/*
 * AP Memory APS256XXN octal DDR PSRAM (Xccela, x8), driven edge by edge through the hyper
 * interface: cs_sync at each chip select change, then one sync_cycle per useful CK edge (both
 * edges, the command being repeated on the falling edge of its clock). The read data go back
 * with a sync_cycle on the edges carrying data; the write data carry their mask (DM high) in
 * bit 8.
 *
 * Behaviour of the datasheet (APS256XXN-OBx9):
 *   - edge 0: command, edges 2-5: address A3..A0 (byte address, A[0] ignored);
 *   - reads (00h, 20h, 40h): data from the rising edge of clock 2 + latency (edge
 *     2 * (2 + latency)), latency the MR0 read latency code, doubled with fixed latency; mode
 *     register reads give the register then the next readable one, repeated;
 *   - writes (80h, A0h): data from edge 2 * (2 + write latency of MR4); mode register writes
 *     (C0h): the byte of edge 6;
 *   - bursts wrap inside their 2 KB row (no row boundary crossing unless the rbx option and
 *     MR8[3]), sync bursts wrap following MR8;
 *   - global reset (FFh) puts the mode registers back to their default.
 * Not modelled: the variable latency refresh pushout, x16, half sleep, deep power down.
 *
 * In legacy mode (for the OctoSPI interface of gvsoc/gap), the data come one clock later, the
 * start address is taken as is and the bursts do not wrap.
 */

#include <vp/vp.hpp>
#include <string.h>
#include <vector>

#include <vp/itf/hyper.hpp>
#include <devices/ram/aps256xxn/aps256xxn_config.hpp>


#define ROW_BYTES 2048


class Aps256xxn : public vp::Component
{
public:
    Aps256xxn(vp::ComponentConf &config);
    void reset(bool active) override;

private:
    static void sync_cycle_stub(vp::Block *__this, int data);
    static void cs_sync_stub(vp::Block *__this, int cs, int active);
    // Handle a useful edge of the OctoSPI clock, data being the byte on the bus (and the mask
    // in bit 8 for write data)
    void edge(int data);
    void cs_sync(int active);
    int read_latency();
    int write_latency();
    int mr_read(int index, bool *defined);
    int mr_next(int index);
    // Address in the array of byte j of the current burst, -1 if out of the RAM
    int64_t byte_addr(int j);
    void mr_reset();

    Aps256xxnConfig cfg;
    vp::Trace trace;
    vp::HyperSlave in_itf;

    // RAM array, filled with a special value to help detecting reads of uninitialized data
    std::vector<uint8_t> mem;

    // Mode registers
    uint8_t mr0, mr4, mr8;

    // Transaction state
    bool selected;
    // Index of the current edge since the chip select went active
    int edge_idx;
    uint8_t cmd;
    uint32_t addr;
    bool is_read, is_write, is_reg, is_sync, is_reset;
    // Edge of the first data, -1 until the command and address are received
    int data_edge;
};


Aps256xxn::Aps256xxn(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->in_itf.set_cs_sync_meth(&Aps256xxn::cs_sync_stub);
    this->in_itf.set_sync_cycle_meth(&Aps256xxn::sync_cycle_stub);
    this->new_slave_port("input", &this->in_itf);

    this->mem.resize(this->cfg.size, 0x57);
    this->mr_reset();
    this->selected = false;
}


void Aps256xxn::mr_reset()
{
    this->mr0 = 0x08;      // variable latency, read latency code 5
    this->mr4 = 0x40;      // write latency code 5, 4x refresh
    this->mr8 = 0x05;      // hybrid wrap 32
}


void Aps256xxn::reset(bool active)
{
    if (active)
    {
        this->mr_reset();
        this->selected = false;
    }
}


int Aps256xxn::read_latency()
{
    static const int lc[] = { 3, 4, 5, 6, 7, 9, 10, 10 };
    static const int fl[] = { 6, 8, 10, 12, 14, 16, 18, 18 };
    int code = (this->mr0 >> 2) & 7;
    return (this->mr0 >> 5) & 1 ? fl[code] : lc[code];
}


int Aps256xxn::write_latency()
{
    // MR4[7:5] codes: 000:3 100:4 010:5 110:6 001:7 101:8 011:9
    static const int wl[] = { 3, 7, 5, 9, 4, 8, 6, 9 };
    return wl[(this->mr4 >> 5) & 7];
}


int Aps256xxn::mr_read(int index, bool *defined)
{
    *defined = true;
    switch (index)
    {
        case 0: return this->mr0;
        case 1: return 0xad;    // ULP, vendor APM
        case 2: return 0xdf;    // good die, generation 4, 256 Mb
        case 3:
        {
            // RBX support, self refresh flag from the MR4 refresh frequency
            int srf = !(this->mr4 & (1 << 3)) ? 2 : (this->mr4 & (1 << 4)) ? 1 : 0;
            return (this->cfg.rbx << 7) | (srf << 4);
        }
        case 4: return this->mr4;
        case 8: return this->mr8;
        default: *defined = false; return 0;
    }
}


int Aps256xxn::mr_next(int index)
{
    switch (index)
    {
        case 0: return 1;
        case 1: return 2;
        case 2: return 3;
        case 3: return 4;
        case 4: case 5: case 6: case 7: return 8;
        default: return 0;
    }
}


int64_t Aps256xxn::byte_addr(int j)
{
    if (this->cfg.legacy)
    {
        int64_t addr = (int64_t)this->addr + j;
        return addr < (int64_t)this->mem.size() ? addr : -1;
    }

    uint32_t base = this->addr & (this->cfg.size - 1) & ~1u;
    uint32_t row = base & ~(ROW_BYTES - 1);

    if (this->is_sync && (this->mr8 & 3) != 3)
    {
        uint32_t bl = 16 << (this->mr8 & 3);
        uint32_t block = base & ~(bl - 1);
        if (!(this->mr8 & (1 << 2)) || (uint32_t)j < bl)
        {
            return block | ((base + j) & (bl - 1));
        }
        return row | ((block + j) & (ROW_BYTES - 1));
    }
    if (this->is_read && this->cfg.rbx && (this->mr8 & (1 << 3)))
    {
        return (base + j) & (this->cfg.size - 1);
    }
    return row | ((base + j) & (ROW_BYTES - 1));
}


void Aps256xxn::edge(int data)
{
    if (!this->selected)
    {
        return;
    }

    int e = this->edge_idx++;

    if (e == 0)
    {
        this->cmd = data & 0xff;
    }
    else if (e >= 2 && e <= 5)
    {
        this->addr = (this->addr << 8) | (data & 0xff);
        if (e == 5)
        {
            uint8_t c = this->cmd;
            this->is_read = c == 0x00 || c == 0x20 || c == 0x40;
            this->is_write = c == 0x80 || c == 0xa0 || c == 0xc0;
            this->is_reg = c == 0x40 || c == 0xc0;
            this->is_sync = c == 0x00 || c == 0x80;
            this->is_reset = c == 0xff;
            if (this->is_read)
                this->data_edge = 2 * (2 + this->read_latency());
            else if (this->is_reg)
                this->data_edge = 2 * (2 + 1);
            else if (this->is_write)
                this->data_edge = 2 * (2 + this->write_latency());
            else if (!this->is_reset)
                this->trace.force_warning("Unknown command (cmd: 0x%x)\n", c);

            if (this->data_edge >= 0 && this->cfg.legacy)
            {
                this->data_edge += 2;
            }

            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Command (cmd: 0x%x, addr: 0x%x)\n",
                c, this->addr);
        }
    }

    if (this->data_edge >= 0 && e >= this->data_edge)
    {
        int j = e - this->data_edge;
        if (this->is_read)
        {
            int value = 0;
            if (this->is_reg)
            {
                bool defined;
                int reg = this->addr & 0xff;
                value = this->mr_read(reg, &defined);
                if (defined && (j & 1))
                {
                    value = this->mr_read(this->mr_next(reg), &defined);
                }
            }
            else
            {
                int64_t addr = this->byte_addr(j);
                if (addr < 0)
                {
                    this->trace.force_warning("Out-of-bound read (addr: 0x%x, size: 0x%x)\n",
                        this->addr + j, this->cfg.size);
                }
                else
                {
                    value = this->mem[addr];
                }
            }
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending data byte (value: 0x%x)\n", value);
            this->in_itf.sync_cycle(value);
        }
        else if (this->is_write && !(data & 0x100))
        {
            if (this->is_reg)
            {
                if (j == 0)
                {
                    int reg = this->addr & 0xff;
                    this->trace.msg(vp::Trace::LEVEL_INFO,
                        "Mode register write (reg: %d, value: 0x%x)\n", reg, data & 0xff);
                    if (reg == 0) this->mr0 = data & 0x3f;
                    else if (reg == 4) this->mr4 = data & 0xff;
                    else if (reg == 8)
                    {
                        this->mr8 = data & 0x7f;
                        if (this->mr8 & (1 << 6))
                        {
                            this->trace.force_warning("x16 mode is not modelled\n");
                        }
                    }
                }
            }
            else
            {
                int64_t addr = this->byte_addr(j);
                this->trace.msg(vp::Trace::LEVEL_TRACE, "Received data byte (value: 0x%x)\n",
                    data & 0xff);
                if (addr < 0)
                {
                    this->trace.force_warning("Out-of-bound write (addr: 0x%x, size: 0x%x)\n",
                        this->addr + j, this->cfg.size);
                }
                else
                {
                    this->mem[addr] = data & 0xff;
                }
            }
        }
    }
}


void Aps256xxn::cs_sync(int active)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Received CS sync (active: %d)\n", active);

    if (active)
    {
        this->selected = true;
        this->edge_idx = 0;
        this->cmd = 0;
        this->addr = 0;
        this->is_read = this->is_write = this->is_reg = this->is_sync = this->is_reset = false;
        this->data_edge = -1;
    }
    else
    {
        if (this->selected && this->is_reset)
        {
            this->mr_reset();
        }
        this->selected = false;
    }
}


void Aps256xxn::sync_cycle_stub(vp::Block *__this, int data)
{
    ((Aps256xxn *)__this)->edge(data);
}


void Aps256xxn::cs_sync_stub(vp::Block *__this, int cs, int active)
{
    ((Aps256xxn *)__this)->cs_sync(active);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Aps256xxn(config);
}
