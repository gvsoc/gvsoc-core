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


#include <vp/vp.hpp>
#include <vp/itf/i2c.hpp>

#include <map>

typedef struct {
    int scl;
    int sda;
} i2c_pair_t;


class I2c_bus : public vp::Component
{
public:
    I2c_bus(vp::ComponentConf &conf);

private:

    static void sync(vp::Block *__this, int scl, int sda, int id);

    vp::Trace trace;

    vp::I2cSlave in;

    std::map<int, i2c_pair_t> i2c_values;

    vp::reg_1 bus_scl;
    vp::reg_1 bus_sda;

    bool pending_resolve;
    bool do_resolve;
};


I2c_bus::I2c_bus(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->in.set_sync_meth_demuxed(&I2c_bus::sync);
    new_slave_port("input", &in);

    uint8_t reset_val = 1;
    this->bus_scl.init(this, "scl", &reset_val);
    this->bus_sda.init(this, "sda", &reset_val);

    this->bus_scl.set(1);
    this->bus_sda.set(1);

    this->pending_resolve = false;

}

void I2c_bus::sync(vp::Block *__this, int scl, int sda, int id)
{
    I2c_bus *_this = (I2c_bus *)__this;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, " => bus sync [id=%d]: scl=%d, sda=%d\n",
            id, scl, sda);
    /* store incoming values in maps */
    _this->i2c_values[id].scl = scl;
    _this->i2c_values[id].sda = sda;

    _this->do_resolve = true;

    if (_this->pending_resolve)
    {
        return;
    }

    _this->trace.msg(vp::Trace::LEVEL_TRACE, " => bus update\n");

    _this->pending_resolve = true;

    while (_this->do_resolve)
    {
        _this->do_resolve = false;

        /* browse all values and compute resulting SCL and SDA */
        int res_scl_value = 1;
        int res_sda_value = 1;

        for (std::pair<int, i2c_pair_t> i2c_val : _this->i2c_values)
        {
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "bus values [id=%d]: scl=%d, sda=%d\n",
                    i2c_val.first,
                    i2c_val.second.scl,
                    i2c_val.second.sda);
            if (i2c_val.second.scl == 0)
            {
                res_scl_value = 0;
            }
            if (i2c_val.second.sda == 0)
            {
                res_sda_value = 0;
            }
        }

        /* broadcast the values to all peripherals if needed */
        if (res_scl_value != _this->bus_scl.get() || res_sda_value != _this->bus_sda.get())
        {
            /* only propagate changes */
            _this->bus_scl.set(res_scl_value);
            _this->bus_sda.set(res_sda_value);
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "I2C: scl=%d, sda=%d\n",
                    _this->bus_scl.get(), _this->bus_sda.get());
            _this->in.sync(res_scl_value, res_sda_value);
        }
    }

    _this->pending_resolve = false;
    _this->trace.msg(vp::Trace::LEVEL_TRACE, " => bus update done[id=%d]\n", id);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new I2c_bus(config);
}
