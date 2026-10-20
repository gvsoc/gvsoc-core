// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// io_v2 port of the ``memory_v2`` SRAM model.
//
// Functionally identical to memory_v2.cpp minus the memcheck
// bookkeeping and the bandwidth model: configurable-size
// byte-addressable store, fixed per-request latency, optional RISC-V
// atomics, preload-from-file, and a ``power_ctrl`` wire to gate the
// backing store.
//
// Differences vs memory_v2:
//
//   - Input port is now ``vp::IoSlave`` from ``vp/itf/io_v2.hpp``.
//   - Completion status is ``IO_REQ_DONE`` (never ``GRANTED`` /
//     ``DENIED`` — memory never stalls), with ``IO_RESP_OK`` /
//     ``IO_RESP_INVALID`` on the response-status sideband.
//   - Timing is a single fixed latency annotated via
//     ``req->inc_latency(cfg.latency)``, read inline by the master
//     under the IoV2Sync contract. The v2 per-byte bandwidth model
//     (``width_log2`` / ``set_duration``) is not reproduced; place a
//     shaper (e.g. interco.limiter_v2.Limiter) upstream if needed.
//   - ``req->is_debug()`` is gone; every access goes through the full
//     timing path.
//   - ``req->get_initiator()`` returns ``void *`` instead of ``int``;
//     the LR/SC reservation table is rekeyed on the pointer.
//   - **No JSON access.** Every tunable (size, latency, stim file,
//     atomics, ...) is read exclusively from the compiled
//     :class:`MemoryV3Config` struct. The model reads zero entries via
//     ``get_js_config()``.
//   - Memcheck bookkeeping is entirely dropped. v1 / v2 carried a
//     shadow buffer, an allocator wire port, and a per-request memcheck
//     sideband; none of that is reproduced here. v3 is a plain backing
//     store. Use :class:`memory.memory_v2.Memory` when memcheck
//     integration is needed.
//   - Power tables are promoted into the struct: the nested ``power``
//     config of :class:`MemoryV3Config` (one ``vp.power_config`` source
//     per field, ``read_8``..``write_32`` and ``background``, filled from
//     the ``power_model`` YAML file) feeds the per-access energy quanta
//     and the background/leakage sources; untouched fields leave their
//     source inert. The ``power_trigger`` start/stop-capture feature only
//     looks at magic payload values.

#include <stdio.h>
#include <string.h>
#include <map>
#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/stats/stats.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/itf/wire.hpp>
#include <vp/debug_mem.hpp>
#include <memory/memory_v3/memory_v3_config.hpp>
#include <vp/power/power_table_convert.hpp>

class Memory : public vp::Component, public vp::DebugMemIf
{

public:
    Memory(vp::ComponentConf &config);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    uint8_t *debug_mem_hostptr(uint64_t addr, uint64_t size) override;

    MemoryV3Config cfg;

private:

    void stop() override;
    void reset(bool active) override;

    static void power_ctrl_sync(vp::Block *__this, bool value);
    static void meminfo_sync_back(vp::Block *__this, void **value);
    static void meminfo_sync(vp::Block *__this, void *value);
    // `strb`, when not NULL, is a per-byte enable: only the bytes whose
    // strobe is non-zero are committed. NULL writes the whole range.
    vp::IoReqStatus handle_write(uint64_t addr, uint64_t size, uint8_t *data,
        uint8_t *strb = nullptr);
    vp::IoReqStatus handle_read(uint64_t addr, uint64_t size, uint8_t *data);
    vp::IoReqStatus handle_atomic(uint64_t addr, uint64_t size, uint8_t *in_data,
        uint8_t *out_data, vp::IoReqOpcode opcode, void *initiator);
    void log_access(uint64_t addr, uint64_t size, bool is_write);
#ifdef VP_MEMCHECK_ACTIVE
    // Buffer check plus shadow maintenance for one request
    void memcheck_handle_req(vp::IoReq *req, uint64_t offset, uint64_t size);
    // Mark a range initialized with no attached pointer (writes without shadow)
    void memcheck_set_range_valid(uint64_t offset, uint64_t size);
#endif

    vp::Trace trace;
    // io_v2 slave port — request callback is attached via the in-class
    // initializer; no set_req_meth() in v2.
    vp::IoSlave in{&Memory::req};

    uint64_t truncate_mask;

    uint8_t *mem_data;
    uint8_t *check_mem;

    // Memcheck shadow storage, allocated when memory checking is enabled: per-byte
    // validity of the stored data (uninitialized-value tracking) and per-4-byte-word
    // buffer ID (provenance of pointers spilled to memory)
    bool memcheck_enabled = false;
    uint8_t *memcheck_shadow = NULL;
    uint32_t *memcheck_shadow_id = NULL;

    vp::WireSlave<bool> power_ctrl_itf;
    vp::WireSlave<void *> meminfo_itf;

    bool powered_up;

    // Power sources fed from the config 'power' tables (inert when absent)
    vp::PowerSource read_8_power;
    vp::PowerSource read_16_power;
    vp::PowerSource read_32_power;
    vp::PowerSource write_8_power;
    vp::PowerSource write_16_power;
    vp::PowerSource write_32_power;
    vp::PowerSource background_power;

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

#ifdef VP_MEMCHECK_ACTIVE
    this->memcheck_enabled = this->traces.get_trace_engine()->is_memcheck_enabled();
    if (this->memcheck_enabled)
    {
        // The shadow starts all-invalid with no attached buffers
        this->memcheck_shadow = (uint8_t *)calloc(this->cfg.size, 1);
        this->memcheck_shadow_id = (uint32_t *)calloc((this->cfg.size + 3) / 4,
            sizeof(uint32_t));
        if (this->memcheck_shadow == NULL || this->memcheck_shadow_id == NULL)
        {
            throw std::bad_alloc();
        }
    }
#endif

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

    if (_this->power.is_enabled())
    {
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

    // Timing annotation. memory_v3 only models a fixed per-request
    // latency; it is read inline by the master under the IoV2Sync
    // contract (no async response). Bandwidth / per-byte duration is
    // deliberately not modelled here — put a shaper (e.g.
    // interco.limiter_v2.Limiter) upstream if that is needed.
    req->inc_latency((int64_t)(req->get_opcode() == vp::WRITE && _this->cfg.write_latency >= 0 ?
        _this->cfg.write_latency : _this->cfg.latency));

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

#ifdef VP_MEMCHECK_ACTIVE
    if (_this->memcheck_enabled)
    {
        _this->memcheck_handle_req(req, offset, size);
    }
#endif

    if (req->get_opcode() == vp::IoReqOpcode::READ)
    {
        return _this->handle_read(offset, size, data);
    }
    else if (req->get_opcode() == vp::IoReqOpcode::WRITE)
    {
        return _this->handle_write(offset, size, data, req->get_strb());
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



uint8_t *Memory::debug_mem_hostptr(uint64_t addr, uint64_t size)
{
    // Direct accesses bypass integrity checking and the memcheck
    // shadow; refuse the window when either is active so those
    // features keep seeing every access.
    if (this->check_mem != NULL)
    {
        return NULL;
    }
#ifdef VP_MEMCHECK_ACTIVE
    if (this->memcheck_enabled)
    {
        return NULL;
    }
#endif

    if (addr + size > (uint64_t)this->cfg.size)
    {
        return NULL;
    }

    return this->mem_data + addr;
}

int Memory::debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size, bool is_write)
{
    uint64_t offset = addr & this->truncate_mask;

    if (offset + size > (uint64_t)this->cfg.size)
    {
        return -1;
    }

    if (is_write)
    {
#ifdef VP_MEMCHECK_ACTIVE
        // Backdoor writes (loader, debugger) initialize the memory
        if (this->memcheck_enabled)
        {
            this->memcheck_set_range_valid(offset, size);
        }
#endif
        this->handle_write(offset, size, data);
    }
    else
    {
        this->handle_read(offset, size, data);
    }

    return 0;
}

#ifdef VP_MEMCHECK_ACTIVE

void Memory::memcheck_set_range_valid(uint64_t offset, uint64_t size)
{
    memset(&this->memcheck_shadow[offset], -1, size);
    for (uint64_t slot = offset >> 2; slot <= (offset + size - 1) >> 2; slot++)
    {
        this->memcheck_shadow_id[slot] = 0;
    }
}

void Memory::memcheck_handle_req(vp::IoReq *req, uint64_t offset, uint64_t size)
{
    // Shadow maintenance: keep the per-byte validity and per-word provenance of
    // the stored data in sync with the request sideband. The buffer checks
    // themselves happen in the core models at issue time.
    if (req->get_opcode() == vp::IoReqOpcode::WRITE)
    {
        if (req->get_memcheck_data() != NULL)
        {
            memcpy(&this->memcheck_shadow[offset], req->get_memcheck_data(), size);
        }
        else
        {
            // The initiator has no shadow support, consider the data initialized
            // to avoid false positives
            memset(&this->memcheck_shadow[offset], -1, size);
        }

        if (size == 4 && (offset & 3) == 0)
        {
            // Word store: record the provenance of the stored value so pointers
            // spilled to memory keep naming their buffer
            this->memcheck_shadow_id[offset >> 2] = req->get_memcheck_data_id();
        }
        else
        {
            // Partial overwrite, the covered slots do not hold a whole pointer
            // anymore
            for (uint64_t slot = offset >> 2; slot <= (offset + size - 1) >> 2; slot++)
            {
                this->memcheck_shadow_id[slot] = 0;
            }
        }
    }
    else if (req->get_opcode() == vp::IoReqOpcode::READ)
    {
        if (req->get_memcheck_data() != NULL)
        {
            memcpy(req->get_memcheck_data(), &this->memcheck_shadow[offset], size);
        }
        if (size == 4 && (offset & 3) == 0)
        {
            req->set_memcheck_data_id(this->memcheck_shadow_id[offset >> 2]);
        }
    }
    else
    {
        // Atomics read-modify-write the location without shadow transport yet:
        // consider the result initialized and drop any stored provenance
        this->memcheck_set_range_valid(offset, size);
    }
}

#endif


vp::IoReqStatus Memory::handle_write(uint64_t offset, uint64_t size, uint8_t *data,
    uint8_t *strb)
{
    if (!this->powered_up)
    {
        return vp::IO_REQ_DONE;
    }

    if (data)
    {
        if (strb)
        {
            // Partial write: commit only the bytes the initiator enabled.
            // Used by masked vector stores, where the inactive elements of a
            // request must leave memory untouched.
            for (uint64_t i = 0; i < size; i++)
            {
                if (strb[i])
                {
                    this->mem_data[offset + i] = data[i];
                }
            }
        }
        else
        {
            memcpy((void *)&this->mem_data[offset], (void *)data, size);
        }
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
#ifdef VP_MEMCHECK_ACTIVE
    free(this->memcheck_shadow);
    free(this->memcheck_shadow_id);
#endif
}

void Memory::reset(bool active)
{
    if (active)
    {
        this->powered_up = true;

        this->background_power.leakage_power_start();
        this->background_power.dynamic_power_start();
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
