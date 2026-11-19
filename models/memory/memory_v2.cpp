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

#include <stdio.h>
#include <string.h>
#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/stats/stats.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <memory/memory_config/memory_config.hpp>
#include <vp/power/power_table_convert.hpp>

class Memory : public vp::Component
{

public:
    Memory(vp::ComponentConf &config);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    MemoryConfig cfg;

private:

    void stop() override;
    void reset(bool active) override;

    static void power_ctrl_sync(vp::Block *__this, bool value);
    static void meminfo_sync_back(vp::Block *__this, void **value);
    static void meminfo_sync(vp::Block *__this, void *value);
    vp::IoReqStatus handle_write(uint64_t addr, uint64_t size, uint8_t *data, uint8_t *memcheck_data);
    vp::IoReqStatus handle_read(uint64_t addr, uint64_t size, uint8_t *data, uint8_t *memcheck_data);
    vp::IoReqStatus handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data, uint8_t *out_data,
        vp::IoReqOpcode opcode, int initiator, uint8_t *in_memcheck_data, uint8_t *out_memcheck_data);
    void log_access(uint64_t addr, uint64_t size, bool is_write);

    vp::Trace trace;
    vp::IoSlave in;


    bool check = false;
    int width_bits = -1;

    uint64_t truncate_mask;

    uint8_t *mem_data;
    // Per-byte validity shadow of the memory content, used for uninitialized-value
    // tracking when memcheck is enabled
    uint8_t *memcheck_data = NULL;
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

    bool free_mem = false;
    vp::Signal<uint64_t> log_addr;
    vp::Signal<uint64_t> log_size;
    vp::Signal<bool> log_is_write;
    // Last cycle where an access was logged. Used to delay a bit in the trace the requests
    // which arrives in the same cycle
    int64_t last_logged_access = -1;
    // Number of requests logged in the same cycle. Used to delay a bit in the trace the requests
    // which arrives in the same cycle
    int nb_logged_access_in_same_cycle = 0;

    // Statistics
    vp::StatScalar stat_reads;
    vp::StatScalar stat_writes;
    vp::StatScalar stat_bytes_read;
    vp::StatScalar stat_bytes_written;
    vp::StatBw stat_read_bw;
    vp::StatBw stat_write_bw;
};



Memory::Memory(vp::ComponentConf &config)
: vp::Component(config, this->cfg),
log_addr(*this, "req_addr", 64, vp::SignalCommon::ResetKind::HighZ),
log_size(*this, "req_size", 64, vp::SignalCommon::ResetKind::HighZ),
log_is_write(*this, "req_is_write", 1, vp::SignalCommon::ResetKind::HighZ)
{

    traces.new_trace("trace", &trace, vp::DEBUG);
    in.set_req_meth(&Memory::req);
    new_slave_port("input", &in);

    // Register statistics
    this->stats.register_stat(&this->stat_reads, "reads", "Number of read accesses");
    this->stats.register_stat(&this->stat_writes, "writes", "Number of write accesses");
    this->stats.register_stat(&this->stat_bytes_read, "bytes_read", "Total bytes read");
    this->stats.register_stat(&this->stat_bytes_written, "bytes_written", "Total bytes written");
    this->stats.register_stat(&this->stat_read_bw, "read_bandwidth", "Average read bandwidth");
    this->stat_read_bw.set_source(&this->stat_bytes_read);
    this->stats.register_stat(&this->stat_write_bw, "write_bandwidth", "Average write bandwidth");
    this->stat_write_bw.set_source(&this->stat_bytes_written);

    this->power_ctrl_itf.set_sync_meth(&Memory::power_ctrl_sync);
    new_slave_port("power_ctrl", &this->power_ctrl_itf);

    this->meminfo_itf.set_sync_back_meth(&Memory::meminfo_sync_back);
    this->meminfo_itf.set_sync_meth(&Memory::meminfo_sync);
    new_slave_port("meminfo", &this->meminfo_itf);

    this->power_trigger = this->cfg.power_trigger;

    // Power sources from the config power tables; a source whose tables
    // were left empty (no entry in the power model file) stays inert.
    vp::new_power_source_from_config(power, "leakage", &background_power,
        this->cfg.power.background);
    vp::new_power_source_from_config(power, "read_8", &read_8_power,
        this->cfg.power.read_8);
    vp::new_power_source_from_config(power, "read_16", &read_16_power,
        this->cfg.power.read_16);
    vp::new_power_source_from_config(power, "read_32", &read_32_power,
        this->cfg.power.read_32);
    vp::new_power_source_from_config(power, "write_8", &write_8_power,
        this->cfg.power.write_8);
    vp::new_power_source_from_config(power, "write_16", &write_16_power,
        this->cfg.power.write_16);
    vp::new_power_source_from_config(power, "write_32", &write_32_power,
        this->cfg.power.write_32);

    this->check = this->cfg.check;
    this->width_bits = this->cfg.width_bits;
    int align = this->cfg.align;

    this->truncate_mask = this->cfg.truncate ? this->cfg.size - 1 : -1;

    trace.msg("Building Memory (size: 0x%x, check: %d)\n", this->cfg.size, check);

    if (align)
    {
        mem_data = (uint8_t *)aligned_alloc(align, this->cfg.size);
    }
    else
    {
        mem_data = (uint8_t *)calloc(this->cfg.size, 1);
        if (mem_data == NULL) throw std::bad_alloc();
    }
    this->free_mem = true;

    // Special option to check for uninitialized accesses
    if (check)
    {
        check_mem = new uint8_t[(this->cfg.size + 7) / 8];
    }
    else
    {
        check_mem = NULL;
    }

    if (this->traces.get_trace_engine()->is_memcheck_enabled())
    {
        this->memcheck_data = (uint8_t *)calloc(this->cfg.size, 1);
        if (this->memcheck_data == NULL) throw std::bad_alloc();
    }

    // Initialize the Memory with a special value to detect uninitialized
    // variables.
    // Only do it for small memories to not slow down too much simulation init.
    if (this->cfg.init && this->cfg.size < (2<<24))
    {
        memset(mem_data, 0x57, this->cfg.size);
    }

    // Preload the Memory
    if (this->cfg.stim_file != NULL)
    {
        string path = this->cfg.stim_file;
        if (path != "")
        {
            trace.msg("Preloading Memory with stimuli file (path: %s)\n", path.c_str());

            FILE *file = fopen(path.c_str(), "rb");
            if (file == NULL)
            {
                this->trace.fatal("Unable to open stim file: %s, %s\n", path.c_str(), strerror(errno));
                return;
            }
            if (fread(this->mem_data, 1, this->cfg.size, file) == 0)
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


void Memory::log_access(uint64_t addr, uint64_t size, bool is_write)
{
    int64_t cycles = this->clock.get_cycles();

    if (cycles > this->last_logged_access)
    {
        this->nb_logged_access_in_same_cycle = 0;
    }

    int64_t delay = 0;
    if (this->nb_logged_access_in_same_cycle > 0)
    {
        int64_t period = this->clock.get_period();
        delay = period - (period >> this->nb_logged_access_in_same_cycle);
    }
    this->log_addr.set_and_release(addr, 0, delay);
    this->log_size.set_and_release(size, 0, delay);
    this->log_is_write.set_and_release(is_write, 0, delay);
    this->nb_logged_access_in_same_cycle++;
    this->last_logged_access = cycles;
}


vp::IoReqStatus Memory::req(vp::Block *__this, vp::IoReq *req)
{
    Memory *_this = (Memory *)__this;

    uint64_t offset = req->get_addr() & _this->truncate_mask;
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    _this->trace.msg("Memory access (addr: 0x%x, offset: 0x%x, size: 0x%x, is_write: %d, op: %d)\n",
        req->get_addr(), offset, size, req->get_is_write(), req->get_opcode());

    // Count stats
    if (req->get_is_write())
    {
        _this->stat_writes++;
        _this->stat_bytes_written += size;
    }
    else
    {
        _this->stat_reads++;
        _this->stat_bytes_read += size;
    }

    _this->log_access(offset, size, req->get_is_write());

    req->inc_latency(_this->cfg.latency);

    if (!req->is_debug())
    {
        // Impact the Memory bandwith on the packet
        if (_this->width_bits != -1)
        {
    #define MAX(a, b) (((a) > (b)) ? (a) : (b))
            int duration = ((size - 1) >> _this->width_bits) + 1;
            req->set_duration(duration);
            int64_t cycles = _this->clock.get_cycles();
            int64_t diff = _this->next_packet_start - cycles;
            if (diff > 0)
            {
                _this->trace.msg("Delayed packet (latency: %ld)\n", diff);
                // Set the latency, this will only set if it is higher than current latency, which
                // is what we want, as the already existing latency already meets our cyclestamp
                req->set_latency(diff);
            }
            _this->next_packet_start = MAX(_this->next_packet_start, cycles) + duration;
        }

        if (_this->power.is_enabled())
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

    if (offset + size > _this->cfg.size)
    {
        _this->trace.force_warning_no_error("Received out-of-bound request (reqAddr: 0x%x, reqSize: 0x%x, memSize: 0x%x)\n", offset, size, _this->cfg.size);
        return vp::IO_REQ_INVALID;
    }

    if (req->get_opcode() == vp::IoReqOpcode::READ)
    {
#ifdef VP_MEMCHECK_ACTIVE
        return _this->handle_read(offset, size, data, req->get_memcheck_data());
#else
        return _this->handle_read(offset, size, data, NULL);
#endif
    }
    else if (req->get_opcode() == vp::IoReqOpcode::WRITE)
    {
#ifdef VP_MEMCHECK_ACTIVE
        return _this->handle_write(offset, size, data, req->get_memcheck_data());
#else
        return _this->handle_write(offset, size, data, NULL);
#endif
    }
    else
    {
#ifdef CONFIG_ATOMICS
#ifdef VP_MEMCHECK_ACTIVE
        return _this->handle_atomic(offset, size, data, req->get_second_data(), req->get_opcode(),
            req->get_initiator(), req->get_memcheck_data(), req->get_second_memcheck_data());
#else
        return _this->handle_atomic(offset, size, data, req->get_second_data(), req->get_opcode(),
            req->get_initiator(), NULL, NULL);
#endif
#else
        _this->trace.force_warning("Received unsupported atomic operation\n");
        return vp::IO_REQ_INVALID;
#endif
    }
}



vp::IoReqStatus Memory::handle_write(uint64_t offset, uint64_t size, uint8_t *data, uint8_t *req_memcheck_data)
{
    // Writes on powered-down memory are silently ignored
    if (!this->powered_up)
    {
        return vp::IO_REQ_OK;
    }

#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_data != NULL)
    {
        if (req_memcheck_data != NULL)
        {
            memcpy((void *)&this->memcheck_data[offset], (void *)req_memcheck_data, size);
        }
        else
        {
            // If there is no memcheck data, it means the model which is writting does not have
            // support to it. Set all data to valid to avoid false negative
            memset((void *)&this->memcheck_data[offset], -1, size);
        }
    }
#endif

    if (data)
    {
        memcpy((void *)&this->mem_data[offset], (void *)data, size);
    }

    return vp::IO_REQ_OK;
}


vp::IoReqStatus Memory::handle_read(uint64_t offset, uint64_t size, uint8_t *data, uint8_t *req_memcheck_data)
{
    // Reads on powered-down memory return 0
    if (!this->powered_up)
    {
        memset((void *)data, 0, size);
        return vp::IO_REQ_OK;
    }

#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_data != NULL)
    {
        if (req_memcheck_data != NULL)
        {
            memcpy((void *)req_memcheck_data, (void *)&this->memcheck_data[offset], size);
        }
    }
#endif

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
    uint8_t *out_data, vp::IoReqOpcode opcode, int initiator, uint8_t *in_memcheck_data,
    uint8_t *out_memcheck_data)
{
    int64_t operand = 0;
    int64_t prev_val = 0;
    int64_t result;
    bool is_write = true;

    memcpy((uint8_t *)&operand, in_data, size);

    this->handle_read(addr, size, (uint8_t *)&prev_val, out_memcheck_data);

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
        {
            auto it = this->res_table.find(initiator);
            if (it != this->res_table.end() && it->second == addr)
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
        }
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
        this->handle_write(addr, size, (uint8_t *)&result, in_memcheck_data);
    }

    return vp::IO_REQ_OK;
}


void Memory::stop()
{
    if (this->free_mem)
    {
        free(this->mem_data);
        this->free_mem = false;
    }
    if (this->check)
    {
        delete[] this->check_mem;
    }
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
    _this->free_mem = false;
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Memory(config);
}
