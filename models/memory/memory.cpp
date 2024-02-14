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

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <stdio.h>
#include <string.h>


class Memory : public vp::Component
{

public:
    Memory(vp::ComponentConf &config);

    void reset(bool active);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

private:
    static void power_ctrl_sync(vp::Block *__this, bool value);
    static void meminfo_sync_back(vp::Block *__this, void **value);
    static void meminfo_sync(vp::Block *__this, void *value);
    vp::IoReqStatus handle_write(uint64_t addr, uint64_t size, uint8_t *data);
    vp::IoReqStatus handle_read(uint64_t addr, uint64_t size, uint8_t *data);
    vp::IoReqStatus handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data, uint8_t *out_data,
        vp::IoReqOpcode opcode, int initiator);

    vp::Trace trace;
    vp::IoSlave in;

    uint64_t size = 0;
    bool check = false;
    int width_bits = 0;
    int latency;

    uint8_t *mem_data;
    uint8_t *check_mem;

    int64_t next_packet_start;

    vp::WireSlave<bool> power_ctrl_itf;
    vp::WireSlave<void *> meminfo_itf;

    bool power_trigger;
    bool powered_up;

    vp::PowerSource read_8_power;
    vp::PowerSource read_16_power;
    vp::PowerSource read_32_power;
    vp::PowerSource write_8_power;
    vp::PowerSource write_16_power;
    vp::PowerSource write_32_power;
    vp::PowerSource background_power;

    vp::ClockEvent *power_event;
    int64_t last_access_timestamp;

    // Load-reserved reservation table
    std::map<uint64_t, uint64_t> res_table;
};



Memory::Memory(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);
    in.set_req_meth(&Memory::req);
    new_slave_port("input", &in);

    this->power_ctrl_itf.set_sync_meth(&Memory::power_ctrl_sync);
    new_slave_port("power_ctrl", &this->power_ctrl_itf);

    this->meminfo_itf.set_sync_back_meth(&Memory::meminfo_sync_back);
    this->meminfo_itf.set_sync_meth(&Memory::meminfo_sync);
    new_slave_port("meminfo", &this->meminfo_itf);

    js::Config *js_config = get_js_config()->get("power_trigger");
    this->power_trigger = js_config != NULL && js_config->get_bool();

    power.new_power_source("leakage", &background_power, this->get_js_config()->get("**/background"));
    power.new_power_source("read_8", &read_8_power, this->get_js_config()->get("**/read_8"));
    power.new_power_source("read_16", &read_16_power, this->get_js_config()->get("**/read_16"));
    power.new_power_source("read_32", &read_32_power, this->get_js_config()->get("**/read_32"));
    power.new_power_source("write_8", &write_8_power, this->get_js_config()->get("**/write_8"));
    power.new_power_source("write_16", &write_16_power, this->get_js_config()->get("**/write_16"));
    power.new_power_source("write_32", &write_32_power, this->get_js_config()->get("**/write_32"));

    this->size = this->get_js_config()->get("size")->get_int();
    this->check = get_js_config()->get_child_bool("check");
    this->width_bits = get_js_config()->get_child_int("width_bits");
    this->latency = get_js_config()->get_child_int("latency");
    int align = get_js_config()->get_child_int("align");

    trace.msg("Building Memory (size: 0x%x, check: %d)\n", size, check);

    if (align)
    {
        mem_data = (uint8_t *)aligned_alloc(align, size);
    }
    else
    {
        mem_data = (uint8_t *)calloc(size, 1);
        if (mem_data == NULL) throw std::bad_alloc();
    }

    // Special option to check for uninitialized accesses
    if (check)
    {
        check_mem = new uint8_t[(size + 7) / 8];
    }
    else
    {
        check_mem = NULL;
    }

    // Initialize the Memory with a special value to detect uninitialized
    // variables
#ifndef CONFIG_GVSOC_ISS_SNITCH
    memset(mem_data, 0x57, size);
#endif
#ifdef CONFIG_GVSOC_ISS_SNITCH
    memset(mem_data, 0x0, size);
#endif

    // Preload the Memory
    js::Config *stim_file_conf = this->get_js_config()->get("stim_file");
    if (stim_file_conf != NULL)
    {
        string path = stim_file_conf->get_str();
        if (path != "")
        {
            trace.msg("Preloading Memory with stimuli file (path: %s)\n", path.c_str());

            FILE *file = fopen(path.c_str(), "rb");
            if (file == NULL)
            {
                this->trace.fatal("Unable to open stim file: %s, %s\n", path.c_str(), strerror(errno));
                return;
            }
            if (fread(this->mem_data, 1, size, file) == 0)
            {
                this->trace.fatal("Failed to read stim file: %s, %s\n", path.c_str(), strerror(errno));
                return;
            }
        }
    }

    this->background_power.leakage_power_start();
    this->background_power.dynamic_power_start();
    this->last_access_timestamp = -1;
}



vp::IoReqStatus Memory::req(vp::Block *__this, vp::IoReq *req)
{
    Memory *_this = (Memory *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    if (!_this->powered_up)
    {
        _this->trace.force_warning("Accessing Memory while it is down (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());
        return vp::IO_REQ_INVALID;
    }

    _this->trace.msg("Memory access (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());

    // _this->trace.msg(vp::Trace::LEVEL_TRACE, "IO req latency before entering router %d\n", req->get_latency());
    req->inc_latency(_this->latency);
    // _this->trace.msg(vp::Trace::LEVEL_TRACE, "IO req latency after entering router %d\n", req->get_latency());

    // Impact the Memory bandwith on the packet
    if (_this->width_bits != 0)
    {
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
        int duration = MAX(size >> _this->width_bits, 1);
        req->set_duration(duration);
        int64_t cycles = _this->clock.get_cycles();
        int64_t diff = _this->next_packet_start - cycles;
        if (diff > 0)
        {
            _this->trace.msg("Delayed packet (latency: %ld)\n", diff);
            req->inc_latency(diff);
        }
        // _this->trace.msg(vp::Trace::LEVEL_TRACE, "IO req latency after sending packet %d, duration %d\n", req->get_latency(), duration);
        #ifndef CONFIG_TCDM
        _this->next_packet_start = MAX(_this->next_packet_start, cycles) + duration;
        #endif
        #ifdef CONFIG_TCDM
        _this->next_packet_start = cycles + duration;
        #endif
        // _this->trace.msg(vp::Trace::LEVEL_TRACE, "Delayed packet (next_packet_start: %ld)\n", _this->next_packet_start);
    }

    if (_this->power.get_power_trace()->get_active())
    {
        _this->last_access_timestamp = _this->time.get_time();

        if (req->get_is_write())
        {
            if (size == 1)
                _this->write_8_power.account_energy_quantum();
            else if (size == 2)
                _this->write_16_power.account_energy_quantum();
            else if (size == 4)
                _this->write_32_power.account_energy_quantum();
        }
        else
        {
            if (size == 1)
                _this->read_8_power.account_energy_quantum();
            else if (size == 2)
                _this->read_16_power.account_energy_quantum();
            else if (size == 4)
                _this->read_32_power.account_energy_quantum();
        }
    }

#ifdef VP_TRACE_ACTIVE
    if (_this->power_trigger)
    {
        if (req->get_is_write() && size == 4 && offset == 0)
        {
            if (*(uint32_t *)data == 0xabbaabba)
            {
                _this->power.get_engine()->start_capture();
            }
            else if (*(uint32_t *)data == 0xdeadcaca)
            {
                static int measure_index = 0;
                _this->power.get_engine()->stop_capture();
                double dynamic_power, static_power;
                fprintf(stderr, "@power.measure_%d@%f@\n", measure_index++, _this->power.get_engine()->get_average_power(dynamic_power, static_power));
            }
        }
    }
#endif

    if (offset + size > _this->size)
    {
        _this->trace.force_warning("Received out-of-bound request (reqAddr: 0x%x, reqSize: 0x%x, memSize: 0x%x)\n", offset, size, _this->size);
        return vp::IO_REQ_INVALID;
    }

    if (req->get_opcode() == vp::IoReqOpcode::READ)
    {
        return _this->handle_read(offset, size, data);
    }
    else if (req->get_opcode() == vp::IoReqOpcode::WRITE)
    {
        return _this->handle_write(offset, size, data);
    }
    else
    {
#ifdef CONFIG_ATOMICS
        return _this->handle_atomic(offset, size, data, req->get_second_data(), req->get_opcode(),
            req->get_initiator());
#else
        _this->trace.force_warning("Received unsupported atomic operation\n");
        return vp::IO_REQ_INVALID;
#endif
    }
}



vp::IoReqStatus Memory::handle_write(uint64_t offset, uint64_t size, uint8_t *data)
{
    if (this->check_mem)
    {
        for (unsigned int i = 0; i < size; i++)
        {
            this->check_mem[(offset + i) / 8] |= 1 << ((offset + i) % 8);
        }
    }
    if (data)
    {
        memcpy((void *)&this->mem_data[offset], (void *)data, size);
    }

    return vp::IO_REQ_OK;
}



vp::IoReqStatus Memory::handle_read(uint64_t offset, uint64_t size, uint8_t *data)
{
    if (this->check_mem)
    {
        for (unsigned int i = 0; i < size; i++)
        {
            int access = (this->check_mem[(offset + i) / 8] >> ((offset + i) % 8)) & 1;
            if (!access)
            {
                // trace.msg("Unitialized access (offset: 0x%x, size: 0x%x, isRead: %d)\n", offset, size, isRead);
                return vp::IO_REQ_INVALID;
            }
        }
    }
    if (data)
    {
        memcpy((void *)data, (void *)&this->mem_data[offset], size);
    }

    return vp::IO_REQ_OK;
}


static inline int64_t get_signed_value(int64_t val, int bits)
{
    return ((int64_t)val) << (64 - bits) >> (64 - bits);
}


vp::IoReqStatus Memory::handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data,
    uint8_t *out_data, vp::IoReqOpcode opcode, int initiator)
{
    int64_t operand = 0;
    int64_t prev_val = 0;
    int64_t result;
    bool is_write = true;

    memcpy((uint8_t *)&operand, in_data, size);

    this->handle_read(addr, size, (uint8_t *)&prev_val);

    if (size < 8)
    {
        operand = get_signed_value(operand, size*8);
        prev_val = get_signed_value(prev_val, size*8);
    }

    switch (opcode)
    {
        case vp::IoReqOpcode::LR:
            this->res_table[initiator] = addr;
            is_write = false;
            break;
        case vp::IoReqOpcode::SC:
            if (this->res_table[initiator] == addr)
            {
                // Valid reservation --> clear all others as we are going to write
                for (auto &it : this->res_table) {
                    if (it.second >= addr && it.second < addr+size)
                    {
                        it.second = -1;
                    }
                }
                result   = operand;
                prev_val = 0;
            }
            else
            {
                is_write = false;
                prev_val = 1;
            }
            break;
        case vp::IoReqOpcode::SWAP:
            result = operand;
            break;
        case vp::IoReqOpcode::ADD:
            result = prev_val + operand;
            break;
        case vp::IoReqOpcode::XOR:
            result = prev_val ^ operand;
            break;
        case vp::IoReqOpcode::AND:
            result = prev_val & operand;
            break;
        case vp::IoReqOpcode::OR:
            result = prev_val | operand;
            break;
        case vp::IoReqOpcode::MIN:
            result = prev_val < operand ? prev_val : operand;
            break;
        case vp::IoReqOpcode::MAX:
            result = prev_val > operand ? prev_val : operand;
            break;
        case vp::IoReqOpcode::MINU:
            result = (uint64_t) prev_val < (uint64_t) operand ? prev_val : operand;
            break;
        case vp::IoReqOpcode::MAXU:
            result = (uint64_t) prev_val > (uint64_t) operand ? prev_val : operand;
            break;
        default:
            return vp::IO_REQ_INVALID;
    }

    memcpy(out_data, (uint8_t *)&prev_val, size);
    if (is_write)
    {
        this->handle_write(addr, size, (uint8_t *)&result);
    }

    return vp::IO_REQ_OK;
}



void Memory::reset(bool active)
{
    if (active)
    {
        this->next_packet_start = 0;
        this->powered_up = true;
    }
}



void Memory::power_ctrl_sync(vp::Block *__this, bool value)
{
    Memory *_this = (Memory *)__this;
    _this->powered_up = value;
}



void Memory::meminfo_sync_back(vp::Block *__this, void **value)
{
    Memory *_this = (Memory *)__this;
    *value = _this->mem_data;
}



void Memory::meminfo_sync(vp::Block *__this, void *value)
{
    Memory *_this = (Memory *)__this;
    _this->mem_data = (uint8_t *)value;
}




extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Memory(config);
}
