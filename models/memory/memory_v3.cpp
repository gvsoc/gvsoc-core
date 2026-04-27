// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// io_v2 port of the ``memory_v2`` SRAM model.
//
// Functionally identical to memory_v2.cpp minus the memcheck
// bookkeeping: configurable-size byte-addressable store, optional
// bandwidth-based timing model, optional RISC-V atomics,
// preload-from-file, and a ``power_ctrl`` wire to gate the backing
// store.
//
// Differences vs memory_v2:
//
//   - Input port is now ``vp::IoSlave`` from ``vp/itf/io_v2.hpp``.
//   - Completion status is ``IO_REQ_DONE`` (never ``GRANTED`` /
//     ``DENIED`` — memory never stalls), with ``IO_RESP_OK`` /
//     ``IO_RESP_INVALID`` on the response-status sideband.
//   - Timing is annotated purely via ``req->inc_latency()``. The v1
//     ``set_duration()`` field does not exist in io_v2, so the
//     bandwidth model folds the burst duration into ``latency``
//     upfront (same total observable latency as v1's
//     ``latency + duration`` sum).
//   - ``req->is_debug()`` is gone; every access goes through the full
//     timing path.
//   - ``req->get_initiator()`` returns ``void *`` instead of ``int``;
//     the LR/SC reservation table is rekeyed on the pointer.
//   - **No JSON access.** Every tunable (size, bandwidth, stim file,
//     atomics, ...) is read exclusively from the compiled
//     :class:`MemoryV3Config` struct. The model reads zero entries via
//     ``get_js_config()``.
//   - Memcheck bookkeeping is entirely dropped. v1 / v2 carried a
//     shadow buffer, an allocator wire port, and a per-request memcheck
//     sideband; none of that is reproduced here. v3 is a plain backing
//     store. Use :class:`memory.memory_v2.Memory` when memcheck
//     integration is needed.
//   - Power-source instantiation is dropped. The v2 model pulled
//     per-access energy tables out of the JSON tree
//     (``**/read_8`` etc.); in the config-only port those energy
//     tables would have to be promoted into the struct too, which is
//     out of scope. The ``power_trigger`` start/stop-capture feature
//     still works because it only looks at magic payload values.

#include <stdio.h>
#include <string.h>
#include <map>
#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/stats/stats.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/itf/wire.hpp>
#include <memory/memory_v3/memory_v3_config.hpp>

class Memory : public vp::Component
{

public:
    Memory(vp::ComponentConf &config);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    MemoryV3Config cfg;

private:

    void stop() override;
    void reset(bool active) override;

    static void power_ctrl_sync(vp::Block *__this, bool value);
    static void meminfo_sync_back(vp::Block *__this, void **value);
    static void meminfo_sync(vp::Block *__this, void *value);
    vp::IoReqStatus handle_write(uint64_t addr, uint64_t size, uint8_t *data);
    vp::IoReqStatus handle_read(uint64_t addr, uint64_t size, uint8_t *data);
    vp::IoReqStatus handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data,
        uint8_t *out_data, vp::IoReqOpcode opcode, void *initiator);
    void log_access(uint64_t addr, uint64_t size, bool is_write);

    vp::Trace trace;
    // io_v2 slave port — request callback is attached via the in-class
    // initializer; no set_req_meth() in v2.
    vp::IoSlave in{&Memory::req};

    uint64_t truncate_mask;

    uint8_t *mem_data;
    uint8_t *check_mem;

    int64_t next_packet_start;

    vp::WireSlave<bool> power_ctrl_itf;
    vp::WireSlave<void *> meminfo_itf;

    bool powered_up;

    // LR/SC reservation table. Keyed on ``req->initiator`` (void* in v2).
    std::map<void *, uint64_t> res_table;

    bool free_mem = false;
    vp::Signal<uint64_t> log_addr;
    vp::Signal<uint64_t> log_size;
    vp::Signal<bool> log_is_write;
    int64_t last_logged_access = -1;
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

    this->truncate_mask = this->cfg.truncate ? this->cfg.size - 1 : -1;

    trace.msg("Building Memory (size: 0x%llx, check: %d)\n",
              (unsigned long long)this->cfg.size, this->cfg.check);

    if (this->cfg.align)
    {
        mem_data = (uint8_t *)aligned_alloc(this->cfg.align, this->cfg.size);
    }
    else
    {
        mem_data = (uint8_t *)calloc(this->cfg.size, 1);
        if (mem_data == NULL) throw std::bad_alloc();
    }
    this->free_mem = true;

    if (this->cfg.check)
    {
        check_mem = new uint8_t[(this->cfg.size + 7) / 8];
    }
    else
    {
        check_mem = NULL;
    }

    if (this->cfg.init && this->cfg.size < (2<<24))
    {
        memset(mem_data, 0x57, this->cfg.size);
    }

    if (this->cfg.stim_file != nullptr && this->cfg.stim_file[0] != '\0')
    {
        trace.msg("Preloading Memory with stimuli file (path: %s)\n", this->cfg.stim_file);

        FILE *file = fopen(this->cfg.stim_file, "rb");
        if (file == NULL)
        {
            this->trace.fatal("Unable to open stim file: %s, %s\n",
                               this->cfg.stim_file, strerror(errno));
            return;
        }
        if (fread(this->mem_data, 1, this->cfg.size, file) == 0)
        {
            this->trace.fatal("Failed to read stim file: %s, %s\n",
                               this->cfg.stim_file, strerror(errno));
            return;
        }
    }
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

    _this->trace.msg("Memory access (addr: 0x%llx, offset: 0x%llx, size: 0x%llx, is_write: %d, op: %d)\n",
        (unsigned long long)req->get_addr(), (unsigned long long)offset,
        (unsigned long long)size, req->get_is_write(), req->get_opcode());

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

    // Timing annotation. io_v2 has no ``duration`` field, so the
    // bandwidth contribution is folded into ``latency`` upfront.
    //
    //   v1: full_latency = max(cfg.latency, diff) + duration
    //   v2: inc_latency(max(cfg.latency, diff) + duration)
    //
    // Same observable timing to the master.
    int64_t cycles = _this->clock.get_cycles();
    int64_t base = (int64_t)_this->cfg.latency;
    int duration = 0;
    if (_this->cfg.width_log2 != -1)
    {
        duration = ((size - 1) >> _this->cfg.width_log2) + 1;
        int64_t diff = _this->next_packet_start - cycles;
        if (diff > base) base = diff;
        _this->next_packet_start =
            (_this->next_packet_start > cycles ? _this->next_packet_start : cycles) + duration;
    }
    req->inc_latency(base + duration);

#ifdef VP_TRACE_ACTIVE
    if (_this->cfg.power_trigger)
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
                fprintf(stderr, "@power.measure_%d@%f@\n", measure_index++,
                        _this->power.get_engine()->get_average_power(dynamic_power, static_power));
            }
        }
    }
#endif

    if (offset + size > (uint64_t)_this->cfg.size)
    {
        _this->trace.force_warning_no_error(
            "Received out-of-bound request (reqAddr: 0x%llx, reqSize: 0x%llx, memSize: 0x%llx)\n",
            (unsigned long long)offset, (unsigned long long)size,
            (unsigned long long)_this->cfg.size);
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
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
        return _this->handle_atomic(offset, size, data, req->get_second_data(),
                                     req->get_opcode(), req->initiator);
#else
        _this->trace.force_warning("Received unsupported atomic operation\n");
        req->set_resp_status(vp::IO_RESP_INVALID);
        return vp::IO_REQ_DONE;
#endif
    }
}



vp::IoReqStatus Memory::handle_write(uint64_t offset, uint64_t size, uint8_t *data)
{
    if (!this->powered_up)
    {
        return vp::IO_REQ_DONE;
    }

    if (data)
    {
        memcpy((void *)&this->mem_data[offset], (void *)data, size);
    }

    return vp::IO_REQ_DONE;
}


vp::IoReqStatus Memory::handle_read(uint64_t offset, uint64_t size, uint8_t *data)
{
    if (!this->powered_up)
    {
        if (data) memset((void *)data, 0, size);
        return vp::IO_REQ_DONE;
    }

    if (data)
    {
        memcpy((void *)data, (void *)&this->mem_data[offset], size);
    }

    return vp::IO_REQ_DONE;
}


static inline int64_t get_signed_value(int64_t val, int bits)
{
    return ((int64_t)val) << (64 - bits) >> (64 - bits);
}


vp::IoReqStatus Memory::handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data,
    uint8_t *out_data, vp::IoReqOpcode opcode, void *initiator)
{
    int64_t operand = 0;
    int64_t prev_val = 0;
    int64_t result = 0;
    bool is_write = true;

    memcpy((uint8_t *)&operand, in_data, size);

    this->handle_read(addr, size, (uint8_t *)&prev_val);

    if (size < 8)
    {
        operand = get_signed_value(operand, size * 8);
        prev_val = get_signed_value(prev_val, size * 8);
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
                for (auto &e : this->res_table) {
                    if (e.second >= addr && e.second < addr + size)
                    {
                        e.second = -1;
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
        case vp::IoReqOpcode::SWAP: result = operand;                           break;
        case vp::IoReqOpcode::ADD:  result = prev_val + operand;                 break;
        case vp::IoReqOpcode::XOR:  result = prev_val ^ operand;                 break;
        case vp::IoReqOpcode::AND:  result = prev_val & operand;                 break;
        case vp::IoReqOpcode::OR:   result = prev_val | operand;                 break;
        case vp::IoReqOpcode::MIN:  result = prev_val < operand ? prev_val : operand; break;
        case vp::IoReqOpcode::MAX:  result = prev_val > operand ? prev_val : operand; break;
        case vp::IoReqOpcode::MINU:
            result = (uint64_t) prev_val < (uint64_t) operand ? prev_val : operand; break;
        case vp::IoReqOpcode::MAXU:
            result = (uint64_t) prev_val > (uint64_t) operand ? prev_val : operand; break;
        default:
            return vp::IO_REQ_DONE;
    }

    if (out_data != nullptr)
    {
        memcpy(out_data, (uint8_t *)&prev_val, size);
    }
    if (is_write)
    {
        this->handle_write(addr, size, (uint8_t *)&result);
    }

    return vp::IO_REQ_DONE;
}


void Memory::stop()
{
    if (this->free_mem)
    {
        free(this->mem_data);
        this->free_mem = false;
    }
    if (this->cfg.check)
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
