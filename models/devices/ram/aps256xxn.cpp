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
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vp/itf/hyper.hpp>
#include <vp/itf/wire.hpp>

#define REGS_AREA_SIZE 1024

typedef enum
{
    APS256XXN_STATE_CMD,
    APS256XXN_STATE_ADDR,
    APS256XXN_STATE_DATA
} Aps256xxn_state_e;

class Aps256xxn : public vp::component
{
public:
    Aps256xxn(js::config *config);

    void handle_access(bool is_register_access, int address, bool is_write, uint8_t data);

    int build();

    static void sync_cycle(void *_this, int data);
    static void cs_sync(void *__this, bool value);

protected:
    vp::trace trace;
    vp::hyper_slave in_itf;
    vp::wire_slave<bool> cs_itf;

private:
    int size;
    uint8_t *data;
    uint8_t *reg_data;

    uint32_t cmd;
    uint32_t addr;
    int cmd_count;
    int addr_count;
    int current_address;
    bool is_register_access;
    bool is_write;

    Aps256xxn_state_e state;
};

void Aps256xxn::handle_access(bool is_register_access, int address, bool is_write, uint8_t data)
{
    if (is_register_access)
    {
        this->trace.msg(vp::trace::LEVEL_DEBUG, "Accessing ram register (addr: %d, is_write: %d, data: 0x%x)\n",
            address, is_write, data);

        // Since registers have no impact, we ignore any access
        if (!is_write)
        {
            this->in_itf.sync_cycle(0);
        }
    }
    else
    {
        if (address >= this->size)
        {
            this->warning.force_warning("Received out-of-bound request (addr: 0x%x, ram_size: 0x%x)\n", address, this->size);
        }
        else
        {
            if (!is_write)
            {
                uint8_t data = this->data[address];
                this->trace.msg(vp::trace::LEVEL_TRACE, "Sending data byte (value: 0x%x)\n", data);
                this->in_itf.sync_cycle(data);
            }
            else
            {
                this->trace.msg(vp::trace::LEVEL_TRACE, "Received data byte (value: 0x%x)\n", data);
                this->data[address] = data;
            }
        }
    }
}

Aps256xxn::Aps256xxn(js::config *config)
    : vp::component(config)
{
}

void Aps256xxn::sync_cycle(void *__this, int data)
{
    Aps256xxn *_this = (Aps256xxn *)__this;

    if (_this->state == APS256XXN_STATE_CMD)
    {
        _this->trace.msg(vp::trace::LEVEL_TRACE, "Received command byte (value: 0x%x)\n", data);

        _this->cmd_count--;
        _this->cmd = (_this->cmd >> 8) | (data << 8);

        if (_this->cmd_count == 0)
        {
            _this->state = APS256XXN_STATE_ADDR;
            _this->addr_count = 4;

            if (_this->cmd == 0x40)
            {
                // Register read
                _this->is_write = false;
                _this->is_register_access = true;
            }
            else if (_this->cmd == 0xc0)
            {
                // Register write
                _this->is_write = true;
                _this->is_register_access = true;
            }
            else if (_this->cmd == 0x20)
            {
                // Burst read
                _this->is_write = false;
                _this->is_register_access = false;
            }
            else if (_this->cmd == 0xA0)
            {
                // Burst write
                _this->is_write = true;
                _this->is_register_access = false;
            }
            else
            {
                _this->trace.fatal("Received unknown RAM command (cmd: 0x%x)\n", _this->cmd);
            }
        }
    }
    else if (_this->state == APS256XXN_STATE_ADDR)
    {
        _this->addr_count--;
        _this->addr = (_this->addr << 8) | data;
        _this->current_address = _this->addr;

        if (_this->addr_count == 0)
        {
            _this->state = APS256XXN_STATE_DATA;
        }
    }
    else if (_this->state == APS256XXN_STATE_DATA)
    {
        _this->handle_access(_this->is_register_access, _this->current_address, _this->is_write, data);
        _this->current_address++;
    }
}

void Aps256xxn::cs_sync(void *__this, bool value)
{
    Aps256xxn *_this = (Aps256xxn *)__this;
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Received CS sync (value: %d)\n", value);

    _this->state = APS256XXN_STATE_CMD;
    _this->cmd_count = 2;
}

int Aps256xxn::build()
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    in_itf.set_sync_cycle_meth(&Aps256xxn::sync_cycle);
    new_slave_port("input", &in_itf);

    cs_itf.set_sync_meth(&Aps256xxn::cs_sync);
    new_slave_port("cs", &cs_itf);

    js::config *conf = this->get_js_config();

    this->size = conf->get("size")->get_int();

    this->data = new uint8_t[this->size];
    memset(this->data, 0xff, this->size);

    this->reg_data = new uint8_t[REGS_AREA_SIZE];
    memset(this->reg_data, 0x57, REGS_AREA_SIZE);
    ((uint16_t *)this->reg_data)[0] = 0x8F1F;

    return 0;
}

extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Aps256xxn(config);
}
