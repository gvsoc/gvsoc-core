// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <string>
#include <vp/vp.hpp>
#include <vp/itf/clock.hpp>
#include <utils/clock_mux_config.hpp>

class ClockMux : public vp::Component {

public:
    ClockMux(vp::ComponentConf &conf);

private:
    static void clock_sync(vp::Block *__this, bool value, int id);
    static void ctrl_sync(vp::Block *__this, int value);

    ClockMuxConfig cfg;
    vp::Trace trace;
    std::vector<vp::ClockSlave> input_itf;
    vp::ClockMaster output_itf;
    vp::WireSlave<int> ctrl_itf;
};

ClockMux::ClockMux(vp::ComponentConf &config)
    : vp::Component(config, this->cfg) {
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->ctrl_itf.set_sync_meth(&ClockMux::ctrl_sync);
    this->new_slave_port("clock_ctrl", &this->ctrl_itf);

    this->input_itf.resize(this->cfg.nb_clocks);
    for (int i=0; i<this->cfg.nb_clocks; i++)
    {
        this->input_itf[i].set_sync_meth_muxed(&ClockMux::clock_sync, i);
        this->new_slave_port("clock_in_" + std::to_string(i), &this->input_itf[i]);
    }

    this->new_master_port("clock_out", &this->output_itf);
}


void ClockMux::clock_sync(vp::Block *__this, bool active, int id) {
    ClockMux *_this = (ClockMux *)__this;
    if (id == _this->cfg.selected_clock) {
        _this->output_itf.sync(active);
    }
}

void ClockMux::ctrl_sync(vp::Block *__this, int value) {
    ClockMux *_this = (ClockMux *)__this;
    _this->cfg.selected_clock = value;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new ClockMux(config); }
