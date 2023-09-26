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


class memory : public vp::component
{

public:
    memory(js::config *config);

    int build();
    void start();
    void reset(bool active);

    static vp::io_req_status_e req(void *__this, vp::io_req *req);

private:
    static void power_ctrl_sync(void *__this, bool value);
    static void meminfo_sync_back(void *__this, void **value);
    static void meminfo_sync(void *__this, void *value);
    vp::io_req_status_e handle_write(uint64_t addr, uint64_t size, uint8_t *data);
    vp::io_req_status_e handle_read(uint64_t addr, uint64_t size, uint8_t *data);
    vp::io_req_status_e handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data, uint8_t *out_data,
        vp::io_req_opcode_e opcode, int initiator);

    vp::trace trace;
    vp::io_slave in;

    uint64_t size = 0;
    bool check = false;
    int width_bits = 0;

    uint8_t *mem_data;
    uint8_t *check_mem;

    int64_t next_packet_start;

    vp::wire_slave<bool> power_ctrl_itf;
    vp::wire_slave<void *> meminfo_itf;

    bool power_trigger;
    bool powered_up;

    vp::power::power_source read_8_power;
    vp::power::power_source read_16_power;
    vp::power::power_source read_32_power;
    vp::power::power_source write_8_power;
    vp::power::power_source write_16_power;
    vp::power::power_source write_32_power;
    vp::power::power_source background_power;

    vp::clock_event *power_event;
    int64_t last_access_timestamp;

    // Load-reserved reservation table
    std::map<uint64_t, uint64_t> res_table;
};



memory::memory(js::config *config)
    : vp::component(config)
{
}



vp::io_req_status_e memory::req(void *__this, vp::io_req *req)
{
    memory *_this = (memory *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    if (!_this->powered_up)
    {
        _this->trace.force_warning("Accessing memory while it is down (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());
        return vp::IO_REQ_INVALID;
    }

    _this->trace.msg("Memory access (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());

    // Impact the memory bandwith on the packet
    if (_this->width_bits != 0)
    {
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
        int duration = MAX(size >> _this->width_bits, 1);
        req->set_duration(duration);
        int64_t cycles = _this->get_cycles();
        int64_t diff = _this->next_packet_start - cycles;
        if (diff > 0)
        {
            _this->trace.msg("Delayed packet (latency: %ld)\n", diff);
            req->inc_latency(diff);
        }
        _this->next_packet_start = MAX(_this->next_packet_start, cycles) + duration;
    }

    if (_this->power.get_power_trace()->get_active())
    {
        _this->last_access_timestamp = _this->get_time();

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
                fprintf(stderr, "@power.measure_%d@%f@\n", measure_index++, _this->power.get_engine()->get_power());
            }
        }
    }
#endif

    if (offset + size > _this->size)
    {
        _this->trace.force_warning("Received out-of-bound request (reqAddr: 0x%x, reqSize: 0x%x, memSize: 0x%x)\n", offset, size, _this->size);
        return vp::IO_REQ_INVALID;
    }

    if (req->get_opcode() == vp::io_req_opcode_e::READ)
    {
        return _this->handle_read(offset, size, data);
    }
    else if (req->get_opcode() == vp::io_req_opcode_e::WRITE)
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



vp::io_req_status_e memory::handle_write(uint64_t offset, uint64_t size, uint8_t *data)
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



vp::io_req_status_e memory::handle_read(uint64_t offset, uint64_t size, uint8_t *data)
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


vp::io_req_status_e memory::handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data,
    uint8_t *out_data, vp::io_req_opcode_e opcode, int initiator)
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
        case vp::io_req_opcode_e::LR:
            this->res_table[initiator] = addr;
            is_write = false;
            break;
        case vp::io_req_opcode_e::SC:
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
        case vp::io_req_opcode_e::SWAP:
            result = operand;
            break;
        case vp::io_req_opcode_e::ADD:
            result = prev_val + operand;
            break;
        case vp::io_req_opcode_e::XOR:
            result = prev_val ^ operand;
            break;
        case vp::io_req_opcode_e::AND:
            result = prev_val & operand;
            break;
        case vp::io_req_opcode_e::OR:
            result = prev_val | operand;
            break;
        case vp::io_req_opcode_e::MIN:
            result = prev_val < operand ? prev_val : operand;
            break;
        case vp::io_req_opcode_e::MAX:
            result = prev_val > operand ? prev_val : operand;
            break;
        case vp::io_req_opcode_e::MINU:
            result = (uint64_t) prev_val < (uint64_t) operand ? prev_val : operand;
            break;
        case vp::io_req_opcode_e::MAXU:
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



void memory::reset(bool active)
{
    if (active)
    {
        this->next_packet_start = 0;
        this->powered_up = true;
    }
}



void memory::power_ctrl_sync(void *__this, bool value)
{
    memory *_this = (memory *)__this;
    _this->powered_up = value;
}



void memory::meminfo_sync_back(void *__this, void **value)
{
    memory *_this = (memory *)__this;
    *value = _this->mem_data;
}



void memory::meminfo_sync(void *__this, void *value)
{
    memory *_this = (memory *)__this;
    _this->mem_data = (uint8_t *)value;
}



int memory::build()
{
    traces.new_trace("trace", &trace, vp::DEBUG);
    in.set_req_meth(&memory::req);
    new_slave_port("input", &in);

    this->power_ctrl_itf.set_sync_meth(&memory::power_ctrl_sync);
    new_slave_port("power_ctrl", &this->power_ctrl_itf);

    this->meminfo_itf.set_sync_back_meth(&memory::meminfo_sync_back);
    this->meminfo_itf.set_sync_meth(&memory::meminfo_sync);
    new_slave_port("meminfo", &this->meminfo_itf);

    js::config *config = get_js_config()->get("power_trigger");
    this->power_trigger = config != NULL && config->get_bool();

    power.new_power_source("leakage", &background_power, this->get_js_config()->get("**/background"));
    power.new_power_source("read_8", &read_8_power, this->get_js_config()->get("**/read_8"));
    power.new_power_source("read_16", &read_16_power, this->get_js_config()->get("**/read_16"));
    power.new_power_source("read_32", &read_32_power, this->get_js_config()->get("**/read_32"));
    power.new_power_source("write_8", &write_8_power, this->get_js_config()->get("**/write_8"));
    power.new_power_source("write_16", &write_16_power, this->get_js_config()->get("**/write_16"));
    power.new_power_source("write_32", &write_32_power, this->get_js_config()->get("**/write_32"));

    return 0;
}



void memory::start()
{
    size = this->get_js_config()->get("size")->get_int();
    check = get_config_bool("check");
    width_bits = get_config_int("width_bits");
    int align = get_config_int("align");

    trace.msg("Building memory (size: 0x%x, check: %d)\n", size, check);

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

    // Initialize the memory with a special value to detect uninitialized
    // variables
    memset(mem_data, 0x57, size);

    // Preload the memory
    js::config *stim_file_conf = this->get_js_config()->get("stim_file");
    if (stim_file_conf != NULL)
    {
        string path = stim_file_conf->get_str();
        if (path != "")
        {
            trace.msg("Preloading memory with stimuli file (path: %s)\n", path.c_str());

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



extern "C" vp::component *vp_constructor(js::config *config)
{
    return new memory(config);
}
