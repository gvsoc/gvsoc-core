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
#include <vp/register.hpp>
#include <inttypes.h>


uint64_t vp::reg::get_field(int offset, int width)
{
    uint64_t value = 0;
    this->read(0, (offset + width + 7)/8, (uint8_t *)&value);
    return (value >> offset) & ((1UL<<width)-1);
}


void vp::regmap::reset(bool active)
{
    for (auto x: this->get_registers())
    {
        x->reset(active);
    }
}

bool vp::regmap::access(uint64_t offset, int size, uint8_t *value, bool is_write)
{
    for (auto x: this->get_registers())
    {
        if (offset >= x->offset && offset + size <= x->offset + (x->width+7)/8)
        {
            vp::reg *aliased_reg = x;

            if (x->alias)
            {
                x = x->alias();
            }

            x->access((offset - aliased_reg->offset), size, value, is_write);

            if (aliased_reg->trace.get_active(vp::trace::LEVEL_DEBUG))
            {
                std::string regfields_values = "";

                if (aliased_reg->regfields.size() != 0)
                {
                    for (auto y: aliased_reg->regfields)
                    {
                        char buff[256];
                        snprintf(buff, 256, "0x%" PRId64, x->get_field(y->bit, y->width));

                        if (regfields_values != "")
                            regfields_values += ", ";

                        regfields_values += y->name + "=" + std::string(buff);
                    }

                    regfields_values = "{ " + regfields_values + " }";
                }
                else
                {
                    char buff[256];
                    snprintf(buff, 256, "0x%" PRId64, x->get_field(0, aliased_reg->width));
                    regfields_values = std::string(buff);
                }

                aliased_reg->trace.msg(vp::trace::LEVEL_DEBUG,
                    "Register access (name: %s, offset: 0x%" PRId64 ", size: 0x%x, is_write: 0x%x, value: %s)\n",
                    aliased_reg->get_name().c_str(), offset, size, is_write, regfields_values.c_str()
                );
            }

            return false;
        }
    }

    vp_warning_always(this->trace, "Accessing invalid register (offset: 0x%" PRId64 ", size: 0x%x, is_write: %d)\n", offset, size, is_write);
    return true;
}



void vp::regmap::build(vp::component *comp, vp::trace *trace, std::string name)
{
    this->comp = comp;
    this->trace = trace;

    for (auto x: this->get_registers())
    {
        std::string reg_name = name;
        if (reg_name == "")
            reg_name = x->get_hw_name();
        else
            reg_name = reg_name + "/" + x->get_hw_name();

        x->build(comp, reg_name);
    }
}


bool vp::reg::access_callback(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    if (this->callback != NULL)
        this->callback(reg_offset, size, value, is_write);

    return this->callback != NULL;
}

void vp::reg::init(vp::component *top, std::string name, int bits, uint8_t *value, uint8_t *reset_value)
{
    this->top = top;
    this->nb_bytes = (bits + 7) / 8;
    this->bits = bits;
    if (reset_value)
        this->reset_value_bytes = new uint8_t[this->nb_bytes];
    else
        this->reset_value_bytes = NULL;
    if (value)
        this->value_bytes = value;
    else
        this->value_bytes = new uint8_t[this->nb_bytes];
    this->name = name;
    if (reset_value)
        memcpy((void *)this->reset_value_bytes, (void *)reset_value, this->nb_bytes);
    top->traces.new_trace(name + "/trace", &this->trace, vp::TRACE);
    top->traces.new_trace_event(name, &this->reg_event, bits);
}

void vp::reg::reset(bool active)
{
    if (active)
    {
        this->trace.msg("Resetting register\n");

        uint64_t zero = 0;
        uint8_t *data = this->reset_value_bytes ? this->reset_value_bytes : (uint8_t *)&zero;
        if (this->exec_callback_on_reset)
        {
            this->access(0, this->nb_bytes, data, true);
        }
        else
        {
            this->update(0, this->nb_bytes, data, true);
        }
    }
}

void vp::reg_1::init(vp::component *top, std::string name, uint8_t *reset_val)
{
    reg::init(top, name, 1, (uint8_t *)&this->value, reset_val);
}

void vp::reg_8::init(vp::component *top, std::string name, uint8_t *reset_val)
{
    reg::init(top, name, 8, (uint8_t *)&this->value, reset_val);
}

void vp::reg_16::init(vp::component *top, std::string name, uint8_t *reset_val)
{
    reg::init(top, name, 16, (uint8_t *)&this->value, reset_val);
}

void vp::reg_32::init(vp::component *top, std::string name, uint8_t *reset_val)
{
    reg::init(top, name, 32, (uint8_t *)&this->value, reset_val);
}

void vp::reg_64::init(vp::component *top, std::string name, uint8_t *reset_val)
{
    reg::init(top, name, 64, (uint8_t *)&this->value, reset_val);
}

void vp::reg_1::build(vp::component *top, std::string name)
{
    top->new_reg(name, this, this->reset_val, this->do_reset);
}

void vp::reg_8::build(vp::component *top, std::string name)
{
    top->new_reg(name, this, this->reset_val, this->do_reset);
}

void vp::reg_16::build(vp::component *top, std::string name)
{
    top->new_reg(name, this, this->reset_val, this->do_reset);
}

void vp::reg_32::build(vp::component *top, std::string name)
{
    top->new_reg(name, this, this->reset_val, this->do_reset);
}

void vp::reg_32::write(int reg_offset, int size, uint8_t *value)
{
    uint8_t *dest = this->value_bytes+reg_offset;
    uint8_t *src = value;
    uint8_t *mask = ((uint8_t *)&this->write_mask) + reg_offset;

    for (int i=0; i<size; i++)
    {
        dest[i] = (dest[i] & ~mask[i]) | (src[i] & mask[i]);
    }

    this->dump_after_write();
    if (this->reg_event.get_event_active())
    {
        this->reg_event.event((uint8_t *)this->value_bytes);
    }
}

void vp::reg_64::build(vp::component *top, std::string name)
{
    top->new_reg(name, this, this->reset_val, this->do_reset);
}
