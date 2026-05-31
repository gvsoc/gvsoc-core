/*
 * Copyright (C) 2026 ETH Zurich, University of Bologna and
 *                    EssilorLuxottica SAS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

/*
 * dramsys_v2.cpp — v2 (beat-protocol) sibling of dramsys.cpp.
 *
 * A single shared C++ wrapper for both protocols is not possible because
 * vp::IoReq is defined differently in vp/itf/io.hpp (v1) and
 * vp/itf/io_v2.hpp (v2) under the same namespace. The two cannot coexist
 * in one translation unit, so the v2 wrapper lives here as a separate
 * model loaded under the name 'memory.dramsys_v2'. The Python generator
 * (memory.dramsys.py) picks 'memory.dramsys' or 'memory.dramsys_v2' based
 * on its `version` kwarg.
 *
 * Protocol summary:
 *   - Read path: master sends one req with full burst size; wrapper
 *     returns IO_REQ_GRANTED, slices into access_size DRAMSys chunks,
 *     and on byte drain emits per-cycle beat resps via a ClockEvent so
 *     each beat lands on its own GVSoC cycle (the +1-cycle enqueue
 *     triggers TimeEngine::was_updated -> the systemc_driver advances
 *     GVSoC time before the beat fires).
 *   - Write path: beat-form only. Master sends N reqs each <= beat_width
 *     sharing a burst_id, last carrying is_last=true. Wrapper accumulates
 *     per-burst_id chunks; fires dram_send_req on chunk-fill or is_last.
 *     A single req with size > beat_width hits trace.fatal.
 *
 * Backpressure: read reqs returning IO_REQ_DENIED bump a counter; on
 * DRAMSys capacity regain (reqCallback) the wrapper calls retry() once
 * per pending denied req so the master re-issues.
 *
 * PIM is not supported in v2.
 */

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <systemc.h>

// GvsocMemspec: ABI-compatible duplicate of the struct in
// gvsoc/core/models/memory/include/pim.hpp. We can't include pim.hpp
// here because it transitively pulls in vp/itf/io.hpp (v1), whose
// vp::IoReq / vp::IoSlave / vp::IoMaster definitions conflict with
// the v2 versions from vp/itf/io_v2.hpp.
struct GvsocMemspec {
    uint access_size;
    uint nb_channels;
    uint nb_pseudo_channels;
    uint nb_ranks;
    uint nb_bank_groups;
    uint nb_banks;
    uint nb_rows;
    uint nb_columns;
    uint channel_stride;
    uint rank_stride;
    uint bankgroup_stride;
    uint bank_stride;
    uint row_stride;
    uint column_stride;
};

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <vector>
#include <deque>
#include <unordered_map>
#include <queue>
#include <algorithm>

typedef void *CallbackInstance_t;
typedef void (AsynCallbackResp_Meth)(CallbackInstance_t, int);
typedef void (AsynCallbackUpdateReq_Meth)(CallbackInstance_t);

class ddr_v2 : public vp::Component
{
public:
    ddr_v2(vp::ComponentConf &conf);
    ~ddr_v2();
    void reset(bool active) override;

private:
    // Per-in-flight read. Master submits ONE req with the full burst size;
    // wrapper drains DRAMSys bytes into the master's data buffer via a
    // mask queue (1 = copy, 0 = drop misalignment/slack) and emits beats
    // back to the master via beat_handler, one per GVSoC cycle.
    struct ReadInflight {
        vp::IoReq *req;
        uint8_t  *master_data;     // snapshot of req->data on submit
        uint64_t total_size;       // snapshot of req->size on submit
        uint64_t burst_addr;       // snapshot of req->addr on submit (burst start)
        int64_t  burst_id;         // snapshot of req->burst_id on submit
        std::queue<int> mask_q;    // one entry per aligned-DRAMSys byte
        uint64_t bytes_filled;     // bytes copied into master_data so far
        uint64_t bytes_emitted;    // bytes emitted as beats so far
    };

    // Per-burst_id beat-form write accumulator.
    struct WriteAccum {
        uint64_t chunk_base_addr;       // aligned base of the current chunk
        std::vector<uint8_t> data;      // size = access_size
        std::vector<uint8_t> strobe;    // size = access_size
        bool     active;                // chunk has any bytes accumulated
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static void            beat_handler(vp::Block *__this, vp::ClockEvent *event);
    static void            rspCallback(void *__this, int is_write);
    static void            reqCallback(void *__this);

    void handle_read(vp::IoReq *req);
    void handle_write_beat(vp::IoReq *req);
    void flush_write_chunk(WriteAccum &accum);
    void emit_one_beat();
    // Bring the local TimeEngine + ClockEngine forward to the current SC
    // wall-clock time. Required as the first step of any SC-entry callback
    // (rspCallback / reqCallback): while systemc_driver waits in SC, neither
    // engine advances on its own, so clock.get_cycles() and event.enqueue(N)
    // would otherwise be computed against the stale "last step_until_sync"
    // cycle, scheduling things in the SC past. By construction systemc_driver
    // would have woken on any earlier GVSoC event, so nothing in the skipped
    // window needs firing — a counter-only bump (via ClockEngine::sync) is
    // sufficient.
    void sync_to_sc();

    vp::Trace      trace;
    vp::IoSlave    in_itf;
    vp::ClockEvent beat_event;

    // DRAMSys C ABI handles (same set as v1).
    void *libraryHandle = nullptr;
    int   dram_id = 0;
    GvsocMemspec memspec{};
    uint  access_size_clog = 0;
    int   beat_width = 0;       // = memspec.access_size

    int  (*add_dram)(char *, char *, GvsocMemspec *)              = nullptr;
    void (*close_dram)(int)                                       = nullptr;
    int  (*dram_can_accept_req)(int)                              = nullptr;
    int  (*dram_has_read_rsp)(int)                                = nullptr;
    int  (*dram_has_write_rsp)(int)                               = nullptr;
    int  (*dram_get_write_rsp)(int)                               = nullptr;
    void (*dram_write_buffer)(int, int, int)                      = nullptr;
    void (*dram_write_strobe)(int, int, int)                      = nullptr;
    void (*dram_send_req)(int, uint64_t, uint64_t, uint64_t, uint64_t) = nullptr;
    int  (*dram_get_read_rsp_byte)(int)                           = nullptr;
    void (*dram_register_async_callback)(int, CallbackInstance_t,
            AsynCallbackResp_Meth *, AsynCallbackUpdateReq_Meth *) = nullptr;

    // State.
    std::deque<ReadInflight *> read_inflight;
    std::unordered_map<int64_t, WriteAccum> write_inflight;
    int denied_count = 0;
};


ddr_v2::ddr_v2(vp::ComponentConf &config)
    : vp::Component(config),
      in_itf(&ddr_v2::req_handler),
      beat_event(this, &ddr_v2::beat_handler)
{
    traces.new_trace("trace", &this->trace, vp::DEBUG);
    new_slave_port("input", &this->in_itf);

    libraryHandle = dlopen("libDRAMSys_Simulator.so", RTLD_LAZY);
    if (!libraryHandle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        abort();
    }

    add_dram                     = (int  (*)(char *, char *, GvsocMemspec *))dlsym(libraryHandle, "add_dram");
    close_dram                   = (void (*)(int))dlsym(libraryHandle, "close_dram");
    dram_can_accept_req          = (int  (*)(int))dlsym(libraryHandle, "dram_can_accept_req");
    dram_has_read_rsp            = (int  (*)(int))dlsym(libraryHandle, "dram_has_read_rsp");
    dram_has_write_rsp           = (int  (*)(int))dlsym(libraryHandle, "dram_has_write_rsp");
    dram_get_write_rsp           = (int  (*)(int))dlsym(libraryHandle, "dram_get_write_rsp");
    dram_write_buffer            = (void (*)(int, int, int))dlsym(libraryHandle, "dram_write_buffer");
    dram_write_strobe            = (void (*)(int, int, int))dlsym(libraryHandle, "dram_write_strobe");
    dram_send_req                = (void (*)(int, uint64_t, uint64_t, uint64_t, uint64_t))dlsym(libraryHandle, "dram_send_req");
    dram_get_read_rsp_byte       = (int  (*)(int))dlsym(libraryHandle, "dram_get_read_rsp_byte");
    dram_register_async_callback = (void (*)(int, CallbackInstance_t, AsynCallbackResp_Meth *, AsynCallbackUpdateReq_Meth *))dlsym(libraryHandle, "dram_register_async_callback");

    // dramsys_configs/ parent dir: env var override, fall back to compile-time default.
    const char *env_path = std::getenv("DRAMSYS_PATH");
    std::string current_path = env_path ? env_path : DRAMSYS_PATH;
    std::cout << "DRAMSYS_PATH: " << current_path
              << (env_path ? " (from env)" : " (from compile-time default)") << std::endl;

    std::string resources_path = current_path + "/dramsys_configs";
    std::string dram_type = get_js_config()->get("dram-type")->get_str();
    if (dram_type.empty()) dram_type = "hbm2-example.json";
    std::string simulationJson_path = resources_path + "/" + dram_type;

    dram_id = add_dram((char *)resources_path.c_str(), (char *)simulationJson_path.c_str(), &memspec);
    access_size_clog = log2(memspec.access_size);
    beat_width = memspec.access_size;
    dram_register_async_callback(dram_id, (CallbackInstance_t)this,
        (AsynCallbackResp_Meth *)&ddr_v2::rspCallback,
        (AsynCallbackUpdateReq_Meth *)&ddr_v2::reqCallback);

    trace.msg("DRAMSys v2 model: type=%s, access_size (beat_width)=%d, nb_channels=%d\n",
              dram_type.c_str(), memspec.access_size, memspec.nb_channels);
}


ddr_v2::~ddr_v2()
{
    for (auto *infl : read_inflight) delete infl;
    if (close_dram) close_dram(dram_id);
}


void ddr_v2::reset(bool active)
{
    if (active) {
        for (auto *infl : read_inflight) delete infl;
        read_inflight.clear();
        write_inflight.clear();
        denied_count = 0;
        if (beat_event.is_enqueued()) beat_event.cancel();
    }
}


vp::IoReqStatus ddr_v2::req_handler(vp::Block *__this, vp::IoReq *req)
{
    ddr_v2 *_this = (ddr_v2 *)__this;
    if (req->get_is_write()) {
        _this->handle_write_beat(req);
        return vp::IO_REQ_DONE;
    }
    // Read: needs DRAMSys capacity at submit time.
    if (!_this->dram_can_accept_req(_this->dram_id)) {
        _this->denied_count++;
        _this->trace.msg("Read DENIED (no DRAMSys capacity), pending=%d\n", _this->denied_count);
        return vp::IO_REQ_DENIED;
    }
    _this->handle_read(req);
    return vp::IO_REQ_GRANTED;
}


void ddr_v2::handle_read(vp::IoReq *req)
{
    uint64_t addr  = req->get_addr();
    uint64_t size  = req->get_size();
    uint8_t *data  = req->get_data();
    int64_t  bid   = req->burst_id;

    uint64_t aligned_start = (addr >> access_size_clog) << access_size_clog;
    uint64_t num_chunks    = 1 + (((addr + size - 1) - aligned_start) >> access_size_clog);
    uint64_t aligned_total = num_chunks * memspec.access_size;

    auto *infl = new ReadInflight{
        /*req=*/req,
        /*master_data=*/data,
        /*total_size=*/size,
        /*burst_addr=*/addr,
        /*burst_id=*/bid,
        /*mask_q=*/{},
        /*bytes_filled=*/0,
        /*bytes_emitted=*/0,
    };
    // Per-byte mask: 1 in [skip, skip+size), 0 elsewhere (leading
    // misalignment + trailing aligned-overread slack).
    uint64_t skip = addr - aligned_start;
    for (uint64_t i = 0; i < aligned_total; i++) {
        infl->mask_q.push((i >= skip && i < skip + size) ? 1 : 0);
    }
    read_inflight.push_back(infl);

    trace.msg("Read SUBMIT addr=0x%lx size=%lu (aligned 0x%lx, %lu chunks)\n",
              addr, size, aligned_start, num_chunks);

    for (uint64_t i = 0; i < num_chunks; i++) {
        dram_send_req(dram_id, aligned_start + i * memspec.access_size,
                      memspec.access_size, /*is_write=*/0, /*strobe_enable=*/0);
    }
}


void ddr_v2::handle_write_beat(vp::IoReq *req)
{
    uint64_t addr  = req->get_addr();
    uint64_t size  = req->get_size();
    uint8_t *data  = req->get_data();
    int64_t  bid   = req->burst_id;
    bool     last  = req->is_last;

    if (size > (uint64_t)beat_width) {
        trace.fatal("v2 wrapper: write size=%lu > beat_width=%d. Big-packet writes are not "
                    "supported; split into beat-form writes (one req per beat, sharing burst_id).\n",
                    size, beat_width);
    }
    if (bid < 0) {
        trace.fatal("v2 wrapper: beat-form write requires burst_id >= 0 "
                    "(got %ld). burst_id == -1 would alias all in-flight bursts.\n", bid);
    }

    WriteAccum &accum = write_inflight[bid];
    if (!accum.active) {
        accum.chunk_base_addr = addr & ~((uint64_t)memspec.access_size - 1);
        accum.data.assign(memspec.access_size, 0);
        accum.strobe.assign(memspec.access_size, 0);
        accum.active = true;
    }

    uint64_t aligned_beat = addr & ~((uint64_t)memspec.access_size - 1);
    if (aligned_beat != accum.chunk_base_addr) {
        // Beat crosses out of the current chunk; flush and start a fresh one.
        flush_write_chunk(accum);
        accum.chunk_base_addr = aligned_beat;
        accum.data.assign(memspec.access_size, 0);
        accum.strobe.assign(memspec.access_size, 0);
        accum.active = true;
    }

    uint64_t pos = addr - accum.chunk_base_addr;
    for (uint64_t i = 0; i < size; i++) {
        accum.data[pos + i]   = data[i];
        accum.strobe[pos + i] = 1;
    }

    trace.msg("Write beat addr=0x%lx size=%lu burst_id=%ld is_last=%d (accum chunk_base=0x%lx)\n",
              addr, size, bid, last, accum.chunk_base_addr);

    // Flush on chunk-fill or on burst end.
    if (last || (pos + size) == (uint64_t)memspec.access_size) {
        flush_write_chunk(accum);
        if (last) write_inflight.erase(bid);
    }
}


void ddr_v2::sync_to_sc()
{
    int64_t sc_ps = (int64_t)sc_core::sc_time_stamp().to_double();
    // (1) Catch the TimeEngine up to SC. update() is monotonic — it only
    //     moves forward, never back.
    this->time.get_engine()->update(sc_ps);
    // (2) Now bump ClockEngine::cycles to match the new TimeEngine time.
    //     ClockEngine::sync() is a no-op if the engine has a permanent_first
    //     event (CPU-style every-cycle event) — the dramsys wrapper has none,
    //     so it always runs ClockEngine::update().
    this->clock.get_engine()->sync();
}


void ddr_v2::flush_write_chunk(WriteAccum &accum)
{
    for (int i = 0; i < memspec.access_size; i++) {
        dram_write_buffer(dram_id, accum.data[i], i);
        dram_write_strobe(dram_id, accum.strobe[i], i);
    }
    dram_send_req(dram_id, accum.chunk_base_addr, memspec.access_size,
                  /*is_write=*/1, /*strobe_enable=*/1);
    trace.msg("Write chunk FLUSH to DRAMSys: addr=0x%lx\n", accum.chunk_base_addr);
    accum.active = false;
}


void ddr_v2::rspCallback(void *__this_v, int is_write)
{
    ddr_v2 *_this = (ddr_v2 *)__this_v;
    // SC kernel may have advanced past the last step_until_sync; without
    // this both clock.get_cycles() and beat_event.enqueue() would compute
    // against the stale GVSoC cycle and land beats in the SC past.
    _this->sync_to_sc();
    if (is_write) {
        while (_this->dram_has_write_rsp(_this->dram_id)) {
            (void)_this->dram_get_write_rsp(_this->dram_id);
        }
        return;
    }
    // Read: drain DRAMSys bytes into the head in-flight req. Schedule the
    // beat_event whenever a beat boundary is reached (i.e. enough bytes
    // ready to fill the next beat).
    bool need_beat_event = false;
    while (_this->dram_has_read_rsp(_this->dram_id) && !_this->read_inflight.empty()) {
        ReadInflight *infl = _this->read_inflight.front();
        if (infl->mask_q.empty()) {
            // Already fully drained for this in-flight; let beat_handler retire it.
            break;
        }
        int byte = _this->dram_get_read_rsp_byte(_this->dram_id);
        int mask = infl->mask_q.front();
        infl->mask_q.pop();
        if (mask) {
            infl->master_data[infl->bytes_filled++] = (uint8_t)byte;
            bool boundary = (infl->bytes_filled % (uint64_t)_this->beat_width == 0) ||
                            (infl->bytes_filled == infl->total_size);
            if (boundary) need_beat_event = true;
        }
    }
    if (need_beat_event && !_this->beat_event.is_enqueued()) {
        _this->beat_event.enqueue(1);
    }
}


void ddr_v2::beat_handler(vp::Block *__this, vp::ClockEvent *event)
{
    ddr_v2 *_this = (ddr_v2 *)__this;
    _this->emit_one_beat();
}


void ddr_v2::emit_one_beat()
{
    if (read_inflight.empty()) return;
    ReadInflight *infl = read_inflight.front();

    uint64_t avail = infl->bytes_filled - infl->bytes_emitted;
    if (avail == 0) return;
    uint64_t left = infl->total_size - infl->bytes_emitted;
    uint64_t beat_size = std::min({(uint64_t)beat_width, avail, left});

    bool is_first = (infl->bytes_emitted == 0);
    bool is_last  = (infl->bytes_emitted + beat_size == infl->total_size);

    vp::IoReq *req = infl->req;
    // Mutate addr to this beat's start address (= burst_addr +
    // cumulative emitted bytes). The v2 contract does not strictly
    // require the slave to update addr per beat (AXI doesn't either),
    // but doing so is a cheap convenience for tracing/debugging and
    // matches the dramsys_v2 convention.
    req->addr     = infl->burst_addr + infl->bytes_emitted;
    req->data     = infl->master_data + infl->bytes_emitted;
    req->size     = beat_size;
    req->burst_id = infl->burst_id;
    req->is_first = is_first;
    req->is_last  = is_last;
    req->status   = vp::IO_RESP_OK;

    trace.msg("Beat RESP addr=0x%lx burst_id=%ld size=%lu is_first=%d is_last=%d (emitted %lu of %lu)\n",
              req->addr, infl->burst_id, beat_size, is_first, is_last,
              infl->bytes_emitted + beat_size, infl->total_size);
    in_itf.resp(req);
    infl->bytes_emitted += beat_size;

    if (is_last) {
        read_inflight.pop_front();
        delete infl;
    }

    // Reschedule if any in-flight still has emitted < filled (more beats ready).
    bool more = false;
    if (!read_inflight.empty()) {
        ReadInflight *next = read_inflight.front();
        if (next->bytes_filled > next->bytes_emitted) more = true;
    }
    if (more && !beat_event.is_enqueued()) {
        beat_event.enqueue(1);
    }
}


void ddr_v2::reqCallback(void *__this_v)
{
    ddr_v2 *_this = (ddr_v2 *)__this_v;
    // Same SC→GVSoC time-skew issue as rspCallback: in_itf.retry() will
    // re-enter the master and may enqueue a fresh GVSoC event whose
    // cyclestamp must reflect the current SC time, not the stale one.
    _this->sync_to_sc();
    while (_this->dram_can_accept_req(_this->dram_id) && _this->denied_count > 0) {
        _this->denied_count--;
        _this->trace.msg("Capacity regained; signalling retry\n");
        _this->in_itf.retry();
    }
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new ddr_v2(config);
}
