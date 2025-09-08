/*
 * Copyright (C) 2025 ETH Zurich and University of Bologna
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
#include <vp/itf/io.hpp>
#include "test.hpp"
#include "vp/clock/clock_event.hpp"

#define CYCLES_ERROR 0.05f

Testbench::Testbench(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->nb_router_in = this->get_js_config()->get_int("nb_router_in");
    this->nb_router_out = this->get_js_config()->get_int("nb_router_out");
    this->nb_gen_per_router_in = this->get_js_config()->get_int("nb_gen_per_router_in");
    this->shared_rw = this->get_js_config()->get("shared_rw")->get_bool();
    this->access_type = this->get_js_config()->get("access_type")->get_str();
    this->bandwidth = this->get_js_config()->get("bandwidth")->get_int();
    this->packet_size = this->get_js_config()->get("packet_size")->get_int();
    this->input_fifo_size = this->get_js_config()->get("input_fifo_size")->get_int();

    this->generator_control_itf.resize(this->nb_router_in * this->nb_gen_per_router_in * 2);

    for (int x=0; x<this->nb_router_in; x++)
    {
        for (int y=0; y<this->nb_gen_per_router_in; y++)
        {
            for (int z=0; z<this->nb_gen_per_router_in; z++)
            {
                this->new_master_port(
                    "generator_control_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z),
                    &this->generator_control_itf[(x*this->nb_gen_per_router_in + y) * 2 + z]);
            }
        }
    }

    this->tests.push_back(new Test0(this));
    this->tests.push_back(new Test1(this));
    this->tests.push_back(new Test2(this));
    this->tests.push_back(new Test3(this));
    this->tests.push_back(new Test4(this));
    this->tests.push_back(new Test5(this));
}

bool Testbench::is_finished()
{
    for (int i=0; i<this->nb_router_in; i++)
    {
        for (int j=0; j<this->nb_gen_per_router_in; j++)
        {
            for (int k=0; k<2; k++)
            {
                if (!this->get_generator(i, j, k)->is_finished())
                {
                    return false;
                }
            }
        }
    }

    return true;
}

void Testbench::reset(bool active)
{
    if (!active)
    {
        this->current_test = 0;
        this->current_test_step = 0;
        this->exec_next_test();
    }
}

void Testbench::exec_next_test()
{
    if (this->current_test == this->tests.size())
    {
        this->time.get_engine()->quit(0);
    }
    else
    {
        this->tests[this->current_test++]->exec_test();
    }
}

void Testbench::test_end(int status)
{
    if (status != 0)
    {
        this->time.get_engine()->quit(status);
    }
    else
    {
        this->exec_next_test();
    }
}

TrafficGeneratorConfigMaster *Testbench::get_generator(int x, int y, bool is_write)
{
    return &this->generator_control_itf[(x*this->nb_gen_per_router_in + y) * 2 + is_write];
}

int Testbench::check_cycles(int64_t result, int64_t expected)
{
    float error = ((float)std::abs(result - expected)) / expected;

    if (error > CYCLES_ERROR)
    {
        printf("Too high error rate (error: %f, expected: %f)\n", error, CYCLES_ERROR);
    }

    return error > CYCLES_ERROR;
}

TestCommon::TestCommon(Block *parent, std::string name)
: Block(parent, name)
{

}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Testbench(config);
}
