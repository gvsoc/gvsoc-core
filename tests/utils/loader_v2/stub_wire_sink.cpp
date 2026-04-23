// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Testbench sink for the loader_v2 ``start`` (wire<bool>) and ``entry``
// (wire<uint64_t>) wires. Every sync pulse is logged with the current
// cycle and the payload so the checker can confirm the loader finalised
// correctly. A quit event also fires unconditionally after
// ``quit_after_cycles`` cycles (default 200) so the simulator always
// terminates even when the loader never reaches finalisation (e.g.
// missing_binary or a permanent downstream deny loop).

#include <vp/vp.hpp>
#include <vp/clock/clock_event.hpp>
#include <cstdio>
#include <string>

class StubWireSink : public vp::Component
{
public:
    StubWireSink(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    static void start_sync(vp::Block *__this, bool active);
    static void entry_sync(vp::Block *__this, uint64_t value);
    static void quit_handler(vp::Block *__this, vp::ClockEvent *event);

    vp::Trace                 trace;
    vp::WireSlave<bool>       start_in;
    vp::WireSlave<uint64_t>   entry_in;
    vp::ClockEvent            quit_event;
    std::string               logname;
    int64_t                   quit_after_cycles = 200;
};


StubWireSink::StubWireSink(vp::ComponentConf &config)
    : vp::Component(config),
      quit_event(this, &StubWireSink::quit_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->logname = this->get_js_config()->get_child_str("logname");
    if (this->logname.empty()) this->logname = this->get_name();

    int qac = this->get_js_config()->get_child_int("quit_after_cycles");
    if (qac > 0) this->quit_after_cycles = qac;

    this->start_in.set_sync_meth(&StubWireSink::start_sync);
    this->new_slave_port("start_in", &this->start_in);

    this->entry_in.set_sync_meth(&StubWireSink::entry_sync);
    this->new_slave_port("entry_in", &this->entry_in);
}


void StubWireSink::reset(bool active)
{
    if (!active)
    {
        this->quit_event.enqueue(this->quit_after_cycles);
    }
}


void StubWireSink::start_sync(vp::Block *__this, bool active)
{
    StubWireSink *_this = (StubWireSink *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s START value=%d\n", now, _this->logname.c_str(), active ? 1 : 0);
    fflush(stdout);
}


void StubWireSink::entry_sync(vp::Block *__this, uint64_t value)
{
    StubWireSink *_this = (StubWireSink *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s ENTRY value=0x%lx\n", now, _this->logname.c_str(), value);
    fflush(stdout);
}


void StubWireSink::quit_handler(vp::Block *__this, vp::ClockEvent *event)
{
    StubWireSink *_this = (StubWireSink *)__this;
    int64_t now = _this->clock.get_cycles();
    printf("[%ld] %s QUIT\n", now, _this->logname.c_str());
    fflush(stdout);
    _this->time.get_engine()->quit(0);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new StubWireSink(config);
}
