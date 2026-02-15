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

#pragma once

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include "interco/traffic/generator.hpp"
#include "interco/traffic/receiver.hpp"

class TestCommon : public vp::Block
{
public:
    TestCommon(Block *parent, std::string name);
    virtual void exec_test() = 0;
};


class Testbench : public vp::Component
{
public:
    Testbench(vp::ComponentConf &config);

    void reset(bool active);

    TrafficGeneratorConfigMaster *get_generator(int x, int y, bool is_write);
    TrafficReceiverConfigMaster *get_receiver(int x);
    void test_end(int status);
    int check_cycles(int64_t result, int64_t expected, float expected_error);
    int64_t get_expected(int size, int nb_inputs, int nb_gens_per_input, int nb_targets);
    void start(size_t size, int nb_inputs, int nb_gens_per_input, int nb_targets, vp::ClockEvent *event);

    int nb_router_in;
    int nb_router_out;
    int nb_gen_per_router_in;
    std::string access_type;
    bool shared_rw;
    int bandwidth;
    int packet_size;
    int input_fifo_size;
    bool use_memory;
    int target_bw;

private:
    void exec_next_test();

    vp::Trace trace;
    std::vector<TestCommon *>tests;
    int current_test;
    int current_test_step;
    std::vector<TrafficGeneratorConfigMaster> generator_control_itf;
    std::vector<TrafficReceiverConfigMaster> receiver_control_itf;
    TrafficGeneratorSync sync;
};

class Test0 : public TestCommon
{
public:
    Test0(Testbench *top);
    void exec_test();

private:
    static void entry(vp::Block *__this, vp::ClockEvent *event);

    Testbench *top;
    vp::ClockEvent fsm_event;
    int step;
    int test_case;
    int64_t clockstamp;
};
