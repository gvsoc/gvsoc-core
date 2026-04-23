// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Testbench control-wire driver for cache_v4.
 *
 * Reads a schedule of (cycle, signal, value) entries and pulses the matching
 * wire port at each cycle. Supports the four cache control wires:
 *   signal = "enable"              -> bool   (cache enable/disable)
 *   signal = "flush"               -> bool   (whole-cache flush pulse)
 *   signal = "flush_line"          -> bool   (single-line flush pulse)
 *   signal = "flush_line_addr"     -> uint32 (latches the line addr)
 *
 * Also exposes a slave port `flush_ack` that simply logs the ack pulses the
 * cache emits after a flush.
 */

#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>
#include <cstdio>
#include <string>
#include <vector>

class StubWires : public vp::Component
{
public:
    StubWires(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    struct Entry {
        int64_t  cycle;
        std::string signal;
        uint64_t value;
    };

    static void fire_handler(vp::Block *__this, vp::ClockEvent *event);
    static void flush_ack_sync(vp::Block *__this, bool value);

    vp::WireMaster<bool>     enable_itf;
    vp::WireMaster<bool>     flush_itf;
    vp::WireMaster<bool>     flush_line_itf;
    vp::WireMaster<uint32_t> flush_line_addr_itf;
    vp::WireSlave<bool>      flush_ack_itf;

    vp::ClockEvent fire_event;
    vp::Trace      trace;
    std::vector<Entry> schedule;
    size_t         next_idx = 0;
    std::string    logname;
};

StubWires::StubWires(vp::ComponentConf &config)
    : vp::Component(config),
      fire_event(this, &StubWires::fire_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_master_port("enable",          &this->enable_itf);
    this->new_master_port("flush",           &this->flush_itf);
    this->new_master_port("flush_line",      &this->flush_line_itf);
    this->new_master_port("flush_line_addr", &this->flush_line_addr_itf);

    this->flush_ack_itf.set_sync_meth(&StubWires::flush_ack_sync);
    this->new_slave_port("flush_ack", &this->flush_ack_itf);

    this->logname = this->get_js_config()->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();

    js::Config *schedule_cfg = this->get_js_config()->get("schedule");
    if (schedule_cfg != NULL)
    {
        for (auto &item : schedule_cfg->get_elems())
        {
            Entry e;
            e.cycle  = item->get_int("cycle");
            e.signal = item->get_child_str("signal");
            e.value  = (uint64_t)item->get_int("value");
            this->schedule.push_back(e);
        }
    }
}

void StubWires::reset(bool active)
{
    if (!active && !this->schedule.empty() && this->next_idx == 0)
    {
        int64_t first = this->schedule[0].cycle;
        if (first <= 0) first = 1;
        this->fire_event.enqueue(first);
    }
}

void StubWires::fire_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubWires *_this = (StubWires *)__this;
    if (_this->next_idx >= _this->schedule.size()) return;

    int64_t now = _this->clock.get_cycles();

    // Fire all entries scheduled for this cycle.
    while (_this->next_idx < _this->schedule.size()
            && _this->schedule[_this->next_idx].cycle <= now)
    {
        Entry &e = _this->schedule[_this->next_idx++];
        printf("[%ld] %s FIRE signal=%s value=%lu\n",
            now, _this->logname.c_str(), e.signal.c_str(), e.value);

        if (e.signal == "enable")
        {
            _this->enable_itf.sync((bool)e.value);
        }
        else if (e.signal == "flush")
        {
            _this->flush_itf.sync((bool)e.value);
        }
        else if (e.signal == "flush_line")
        {
            _this->flush_line_itf.sync((bool)e.value);
        }
        else if (e.signal == "flush_line_addr")
        {
            _this->flush_line_addr_itf.sync((uint32_t)e.value);
        }
        else
        {
            _this->trace.force_warning("Unknown signal '%s'\n", e.signal.c_str());
        }
    }

    if (_this->next_idx < _this->schedule.size())
    {
        int64_t delta = _this->schedule[_this->next_idx].cycle - now;
        if (delta <= 0) delta = 1;
        _this->fire_event.enqueue(delta);
    }
}

void StubWires::flush_ack_sync(vp::Block *__this, bool value)
{
    StubWires *_this = (StubWires *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s FLUSH_ACK value=%d\n", now, _this->logname.c_str(), value ? 1 : 0);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubWires(config);
}
