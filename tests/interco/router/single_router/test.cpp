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

#include <climits>
#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include "test.hpp"
#include "vp/clock/clock_event.hpp"

Testbench::Testbench(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->target_bw = this->get_js_config()->get_int("target_bw");
    this->use_memory = this->get_js_config()->get_child_bool("use_memory");
    this->nb_router_in = this->get_js_config()->get_int("nb_router_in");
    this->nb_router_out = this->get_js_config()->get_int("nb_router_out");
    this->nb_gen_per_router_in = this->get_js_config()->get_int("nb_gen_per_router_in");
    this->shared_rw = this->get_js_config()->get("shared_rw")->get_bool();
    this->access_type = this->get_js_config()->get("access_type")->get_str();
    this->bandwidth = this->get_js_config()->get("bandwidth")->get_int();
    this->packet_size = this->get_js_config()->get("packet_size")->get_int();
    this->input_fifo_size = this->get_js_config()->get("input_fifo_size")->get_int();

    printf("Starting tests (nb_router_in: %d, nb_router_out: %d, nb_gen_per_router_in: %d, "
        "shared_rw: %d, access_type: %s, bandwidth: %d, packet_size: %d, input_fifo_size: %d)\n",
        this->nb_router_in, this->nb_router_out, this->nb_gen_per_router_in,
        this->shared_rw, this->access_type.c_str(), this->bandwidth, this->packet_size,
        this->input_fifo_size);

    this->generator_control_itf.resize(this->nb_router_in * this->nb_gen_per_router_in * 2);

    for (int x=0; x<this->nb_router_in; x++)
    {
        for (int y=0; y<this->nb_gen_per_router_in; y++)
        {
            for (int z=0; z<2; z++)
            {
                this->new_master_port(
                    "generator_control_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z),
                    &this->generator_control_itf[(x*this->nb_gen_per_router_in + y) * 2 + z]);
            }
        }
    }

    if (!this->use_memory)
    {
        this->receiver_control_itf.resize(this->nb_router_out);
        for (int x=0; x<this->nb_router_out; x++)
        {
            this->new_master_port(
                "receiver_control_" + std::to_string(x),
                &this->receiver_control_itf[x]);
        }
    }

    this->tests.push_back(new Test0(this));
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

TrafficReceiverConfigMaster *Testbench::get_receiver(int x)
{
    return &this->receiver_control_itf[x];
}

int Testbench::check_cycles(int64_t result, int64_t expected, float expected_error)
{
    float error = ((float)std::abs(result - expected)) / expected;

    if (error > expected_error)
    {
        printf("Too high error rate (error: %f, expected: %f)\n", error, expected_error);
    }

    return error > expected_error;
}

void Testbench::start(size_t size, int nb_inputs, int nb_gens_per_input, int nb_targets, vp::ClockEvent *event)
{
    bool do_read = this->access_type == "r" || this->access_type == "rw";
    bool do_write = this->access_type == "w" || this->access_type == "rw";
    size_t bw0 = this->packet_size;
    this->sync.event = event;

    uint64_t target_id = 0;

    this->sync.init();

    for (int i=0; i<nb_inputs; i++)
    {
        for (int j=0; j<nb_gens_per_input; j++)
        {
            uint64_t offset = 0x10000000 + 0x10000 * (i * nb_gens_per_input + j) * 2 +
                0x100000 * target_id;
            target_id++;
            if (target_id == nb_targets)
            {
                target_id = 0;
            }

            if (do_read)
            {
                this->get_generator(i, j, 0)->start(offset, size, bw0, &this->sync, false);
            }
            if (do_write)
            {
                this->get_generator(i, j, 1)->start(offset + 0x10000, size, bw0, &this->sync, true);
            }
        }
    }

    if (!this->use_memory)
    {
        for (int i=0; i<this->nb_router_out; i++)
        {
            this->get_receiver(i)->start(this->target_bw);
        }
    }

    this->sync.start();
}

int64_t Testbench::get_expected(int size, int nb_inputs, int nb_gens_per_input, int nb_targets)
{
    bool do_read = this->access_type == "r" || this->access_type == "rw";
    bool do_write = this->access_type == "w" || this->access_type == "rw";
    size_t target_bw = INT_MAX;
    if (this->target_bw != 0)
    {
        target_bw = std::min(this->target_bw, this->packet_size);
        target_bw = target_bw * std::min(nb_inputs*nb_gens_per_input, nb_targets);
    }
    else if (!this->use_memory)
    {
        // Receiver only accept one packet per cycle, even when bandwidth is 0
        target_bw = this->packet_size;
        target_bw = target_bw * std::min(nb_inputs*nb_gens_per_input, nb_targets);
    }

    size_t router_bw = std::min(this->bandwidth, this->packet_size) * std::min(nb_inputs, nb_targets);
    if (do_read && do_write && !this->shared_rw)
    {
        router_bw *= 2;
    }

    size_t input_bw = std::min(this->bandwidth, this->packet_size) * nb_inputs;
    if (do_read && do_write && !this->shared_rw)
    {
        input_bw *= 2;
    }

    size_t bw = std::min(input_bw, std::min(router_bw, target_bw));

    size_t total_size = size * nb_inputs * nb_gens_per_input;
    if (do_read && do_write)
    {
        total_size *= 2;
    }

    int64_t expected = total_size / bw;
    return expected;
}

TestCommon::TestCommon(Block *parent, std::string name)
: Block(parent, name)
{

}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Testbench(config);
}
