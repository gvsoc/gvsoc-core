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
#include <vp/itf/i2s.hpp>
#include <vp/itf/clock.hpp>


class I2s_clock : public vp::Component
{
    friend class Stim_txt;

public:
    I2s_clock(vp::ComponentConf &conf);

    void start();

protected:

    static void handler(vp::Block *__this, vp::ClockEvent *event);

    vp::I2sMaster i2s_itf;
    vp::ClockMaster clock_cfg;
    vp::ClockEvent *event;

    int sck;
    bool is_ws;
};


I2s_clock::I2s_clock(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->new_master_port("i2s", &this->i2s_itf);
    this->new_master_port("clock_cfg", &this->clock_cfg);

    this->event = this->event_new(I2s_clock::handler);
    this->sck = 0;
    this->is_ws = this->get_js_config()->get_child_bool("is_ws");

}


void I2s_clock::start()
{
    int frequency = this->get_js_config()->get_child_int("frequency");

    if (frequency != 0)
    {
        this->clock_cfg.set_frequency(frequency);
        this->event_enqueue(this->event, 1);
    }
}


void I2s_clock::handler(vp::Block *__this, vp::ClockEvent *event)
{
    I2s_clock *_this = (I2s_clock *)__this;

    if (_this->is_ws)
    {
        _this->i2s_itf.sync(2, _this->sck, 2);
    }
    else
    {
        _this->i2s_itf.sync(_this->sck, 2, 2);
    }

    _this->sck ^= 1;

    _this->event_enqueue(_this->event, 1);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new I2s_clock(config);
}
