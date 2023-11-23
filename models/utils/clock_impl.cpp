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
#include <vp/itf/clock.hpp>
#include <stdio.h>
#include <string.h>

class Clock : public vp::Component
{

public:
    Clock(vp::ComponentConf &conf);

    void reset(bool active);

private:
    static void edge_handler(vp::Block *__this, vp::ClockEvent *event);
    inline void raise_edge();
    static void power_sync(vp::Block *__this, bool active);

    vp::Trace trace;
    vp::WireSlave<bool> power_itf;
    vp::ClockMaster clock_ctrl_itf;
    vp::ClockMaster clock_sync_itf;
    vp::ClockEvent *event;
    int value;
    float target_frequency;
    float frequency;
    float powerup_time;
    bool powered_on;
    int64_t start_time;
};

inline void Clock::raise_edge()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Changing clock level (level: %d)\n", value);

    if (this->target_frequency)
    {
        int64_t diff_time = this->time.get_time()- this->start_time;
        if (diff_time >= this->powerup_time)
        {
            this->clock_ctrl_itf.set_frequency(this->target_frequency);
            this->target_frequency = 0;
        }
        else
        {
            float frequency = this->target_frequency * (0.1 + 0.9 * diff_time / this->powerup_time);
            this->clock_ctrl_itf.set_frequency(frequency);
        }
    }

    this->clock_sync_itf.sync(value);
    this->value ^= 1;
}

void Clock::edge_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Clock *_this = (Clock *)__this;
    _this->raise_edge();
}


void Clock::power_sync(vp::Block *__this, bool active)
{
    Clock *_this = (Clock *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Changing clock power (is_on: %d)\n", active);

    if (active != _this->powered_on)
    {
        if (active)
        {
            _this->target_frequency = _this->frequency;
            _this->start_time = _this->time.get_time();
            if (_this->clock_sync_itf.is_bound())
            {
                _this->event->enable();
            }
        }
        else
        {
            if (_this->event->is_enqueued())
            {
                _this->event->disable();
            }
        }
    }

    _this->powered_on = active;
}


Clock::Clock(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);
    this->event = this->event_new(Clock::edge_handler);

    this->power_itf.set_sync_meth(&Clock::power_sync);
    this->new_slave_port("power", &this->power_itf);
    this->new_master_port("clock_ctrl", &this->clock_ctrl_itf);
    this->new_master_port("clock_sync", &this->clock_sync_itf);
    this->value = 0;
    this->powerup_time = this->get_js_config()->get_child_int("powerup_time");
    this->powered_on = this->get_js_config()->get("powered_on") == NULL || this->get_js_config()->get_child_bool("powered_on");

}

void Clock::reset(bool active)
{
    if (!active)
    {
        this->target_frequency = 0;
        this->frequency = this->clock.get_engine()->get_frequency();
        this->clock_sync_itf.set_frequency(this->clock.get_engine()->get_frequency() / 2);

        if (this->powered_on)
        {
            if (!this->event->is_enqueued())
            {
                if (this->clock_sync_itf.is_bound())
                {
                    this->event->enable();
                }
            }
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Clock(config);
}
