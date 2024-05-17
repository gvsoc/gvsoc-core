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
#include <vp/itf/wire.hpp>


class And : public vp::Component
{

public:
    And(vp::ComponentConf &config);

    void reset(bool active);

private:
    static void sync(vp::Block *__this, bool value, int id);

    vp::Trace trace;

    std::vector<vp::WireSlave<bool>> input_itfs;
    vp::WireMaster<bool> output_itf;

    std::vector<uint64_t> values;
    int nb_values;
    uint64_t last_value_mask;
};



And::And(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    int nb_input = this->get_js_config()->get_child_int("nb_input");
    this->input_itfs.resize(nb_input);

    for (int i=0; i<nb_input; i++)
    {
        this->input_itfs[i].set_sync_meth_muxed(&And::sync, i);
        this->new_slave_port("input_" + std::to_string(i), &this->input_itfs[i]);
    }

    this->new_master_port("output", &this->output_itf);

    this->nb_values = (nb_input + 63) / 64;

    this->last_value_mask = ~((1 << (nb_input % 64)) - 1);

    this->values.resize(this->nb_values);
}



void And::reset(bool active)
{
    if (active)
    {
        for (int i=0; i<this->nb_values; i++)
        {
            this->values[i] = 0;
        }

        this->values[this->nb_values - 1] = this->last_value_mask;
    }
}



void And::sync(vp::Block *__this, bool value, int id)
{
    And *_this = (And *)__this;

    int value_byte = id / 64;
    int value_bit = id % 64;

    if (value)
    {
        _this->values[value_byte] |= 1 << value_bit;
    }
    else
    {
        _this->values[value_byte] &= ~(1 << value_bit);
    }

    if (_this->values[value_byte] == -1)
    {
        for (int i=0; i<_this->nb_values; i++)
        {
            if (_this->values[i] != -1)
            {
                _this->output_itf.sync(false);
                return;
            }
        }

        _this->output_itf.sync(true);
    }
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new And(config);
}
