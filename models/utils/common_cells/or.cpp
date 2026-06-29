/*
 * Copyright (C) 2026 ETH Zurich and University of Bologna
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
 * Description: This file implements a GVSoC OR common cell.
 * Author: Yinrong Li (ETH Zurich) (yinrli@student.ethz.ch)
 */

#include <cstdint>

#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>


class Or : public vp::Component
{

public:
    Or(vp::ComponentConf &config);

    void reset(bool active);

private:
    static void sync(vp::Block *__this, bool value, int id);

    vp::Trace trace;

    std::vector<vp::WireSlave<bool>> input_itfs;
    vp::WireMaster<bool> output_itf;

    std::vector<uint64_t> values;
    int nb_input;
    int nb_values;
};



Or::Or(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->nb_input = this->get_js_config()->get_child_int("nb_input");
    this->input_itfs.resize(this->nb_input);

    for (int i=0; i<this->nb_input; i++)
    {
        this->input_itfs[i].set_sync_meth_muxed(&Or::sync, i);
        this->new_slave_port("input_" + std::to_string(i), &this->input_itfs[i]);
    }

    this->new_master_port("output", &this->output_itf);

    this->nb_values = (this->nb_input + 63) / 64;
    this->values.resize(this->nb_values);
}



void Or::reset(bool active)
{
    if (active)
    {
        for (int i=0; i<this->nb_values; i++)
        {
            this->values[i] = 0;
        }
    }
}



void Or::sync(vp::Block *__this, bool value, int id)
{
    Or *_this = (Or *)__this;

    int value_word = id / 64;
    uint64_t value_mask = uint64_t(1) << (id % 64);

    if (value)
    {
        _this->values[value_word] |= value_mask;
    }
    else
    {
        _this->values[value_word] &= ~value_mask;
    }

    bool output_value = false;
    for (int i=0; i<_this->nb_values; i++)
    {
        if (_this->values[i] != 0)
        {
            output_value = true;
            break;
        }
    }

    _this->output_itf.sync(output_value);
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Or(config);
}
