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

#include "test0.hpp"


Test0::Test0(Testbench *top)
: TestCommon(top, "test0"), fsm_event(this, Test0::entry)
{
    this->top = top;
}

bool Test0::check_single_path(bool do_write, bool wide, bool narrow, int initiator_bw,
    int target_bw, int size, int expected)
{
    int x0=0, y0=0, x1=this->top->nb_cluster_x-1, y1=this->top->nb_cluster_y-1;

    if (this->step == 0)
    {
        printf("Single path (write: %d, wide: %d, narrow: %d, initiator_bw: %d, target_bw: %d)\n",
            do_write, wide, narrow, initiator_bw, target_bw);

        TrafficGeneratorSync *sync = new TrafficGeneratorSync(&this->fsm_event);

        // Check communications from top/left cluster to bottom/right. Target cluster has half
        // the bandwidth of the initiator cluster

        uint64_t base1 = this->top->get_cluster_base(x1, y1);

        if (wide)
        {
            this->top->get_generator(x0, y0, false)->start(base1, size, initiator_bw, sync, do_write, true);
            this->top->get_receiver(x1, y1, false)->start(target_bw);
        }

        if (narrow)
        {
            this->top->get_generator(x0, y0, true)->start(base1 + 0x10000, size, initiator_bw, sync, do_write, true);
            this->top->get_receiver(x1, y1, true)->start(target_bw);
        }

        sync->start();

        this->step++;
    }
    else
    {
        int64_t cycles = 0;

        bool check_status = false;

        if (wide)
        {
            this->top->get_generator(x0, y0, false)->get_result(&check_status, &cycles);
        }

        if (narrow)
        {
            this->top->get_generator(x0, y0, true)->get_result(&check_status, &cycles);
        }

        bool status = check_status;
        status |= this->top->check_cycles(cycles, expected) != 0;

        printf("    %s (check: %d, size: %lld, cycles: %lld, expected: %lld)\n",
            status ? "Failed" : "Done", check_status, size, cycles, expected);

        this->status |= status;
        return true;
    }
    return false;
}

bool Test0::check_2_paths_through_same_node()
{
    // One horizontal and one vertical paths, both in the middle, so that they cross in the center
    // of the grid
    int x0=(this->top->nb_cluster_x-1)/2, y0=0;
    int x1=(this->top->nb_cluster_x-1)/2, y1=this->top->nb_cluster_y-1;
    int x2=0, y2=(this->top->nb_cluster_y-1)/2;
    int x3=this->top->nb_cluster_x-1, y3=(this->top->nb_cluster_y-1)/2;
    size_t size = 1024*256;
    size_t bw = 8;

    if (this->step == 0)
    {
        printf("Test 1, checking 2 data streams going through same router but using different paths\n");

        TrafficGeneratorSync *sync = new TrafficGeneratorSync(&this->fsm_event);

        uint64_t base1 = this->top->get_cluster_base(x1, y1);
        uint64_t base3 = this->top->get_cluster_base(x3, y3);

        this->top->get_generator(x0, y0)->start(base1, size, bw, sync, false, true);
        this->top->get_receiver(x1, y1)->start(bw);
        this->top->get_generator(x2, y2)->start(base3, size, bw, sync, false, true);
        this->top->get_receiver(x3, y3)->start(bw);

        sync->start();

        this->step++;
    }
    else
    {
        int64_t cycles = 0;
        bool check_status = false;

        this->top->get_generator(x0, y0)->get_result(&check_status, &cycles);
        this->top->get_generator(x2, y2)->get_result(&check_status, &cycles);

        int64_t expected = size / bw;

        bool status = this->top->check_cycles(cycles, expected) != 0;

        printf("    %s (size: %lld, bw: %lld, cycles: %lld, expected: %lld)\n",
            status ? "Failed" : "Done", size, bw, cycles, expected);

        this->status |= status;
        return true;
    }
    return false;
}

bool Test0::check_2_paths_to_same_target()
{
    // One generator on top/right, another one on bottom/left and the receiver on bottom/right
    int x0=this->top->nb_cluster_x-1, y0=0;
    int x1=0, y1=this->top->nb_cluster_y-1;
    int x2=this->top->nb_cluster_x-1, y2=this->top->nb_cluster_y-1;
    size_t size = 1024*256;
    size_t bw1 = 4;
    size_t bw2 = 8;

    if (this->step == 0)
    {
        printf("Test 2, checking 2 generators going to same receiver\n");

        TrafficGeneratorSync *sync = new TrafficGeneratorSync(&this->fsm_event);

        uint64_t base = this->top->get_cluster_base(x2, y2);

        this->top->get_generator(x0, y0)->start(base, size, bw1, sync, false, true);
        this->top->get_generator(x1, y1)->start(base + 0x10000, size, bw1, sync, false, true);
        this->top->get_receiver(x2, y2)->start(bw2);

        sync->start();

        this->clockstamp = this->clock.get_cycles();
        this->step++;
    }
    else
    {
        int64_t cycles = 0;
        bool check_status = false;

        this->top->get_generator(x0, y0)->get_result(&check_status, &cycles);
        this->top->get_generator(x1, y1)->get_result(&check_status, &cycles);

        // Since we have 2 generators going to same target, bandwidth is divided by 2
        int64_t expected = size / (bw1 / 2);

        bool status = this->top->check_cycles(cycles, expected) != 0;

        printf("    %s (size: %lld, bw: %lld, cycles: %lld, expected: %lld)\n",
            status ? "Failed" : "Done", size, bw1, cycles, expected);

        this->status |= status;
        return true;
    }
    return false;
}

bool Test0::check_prefered_path()
{
    size_t size = 1024*256;
    size_t bw0 = 8;
    int x0=0, y0=0, x1=1, y1=0, x2=2, y2=0;

    if (this->step == 0)
    {
        printf("Test 4, checking 3 generators to 1 border with prefered direction\n");

        TrafficGeneratorSync *sync = new TrafficGeneratorSync(&this->fsm_event);

        this->top->get_generator(x0, y0)->start(0x90000000, size, bw0, sync, false, true);
        this->top->get_generator(x1, y1)->start(0x90010000, size, bw0, sync, false, true);
        this->top->get_generator(x2, y2)->start(0x90020000, size, bw0, sync, false, true);

        this->clockstamp = this->clock.get_cycles();
        sync->start();
        this->step++;
    }
    else
    {
        int64_t cycles = 0;
        bool check_status = false;

        this->top->get_generator(x0, y0)->get_result(&check_status, &cycles);
        this->top->get_generator(x1, y1)->get_result(&check_status, &cycles);
        this->top->get_generator(x2, y2)->get_result(&check_status, &cycles);

        printf("    Done\n");

        return true;
    }
    return false;
}

void Test0::entry(vp::Block *__this, vp::ClockEvent *event)
{
    Test0 *_this = (Test0 *)__this;
    bool done;

    std::vector<std::function<bool()>> testcases = {
        // Wide channel is 64 bytes and narrow is 8 bytes

        //                                    write | wide | narrow | initiator bw | target bw | size    | expected cycles
        [&] { return _this->check_single_path(true  , true , false  , 8            , 4         , 1024*256, 65536); }, // Target limited bandwidth is the bottleneck
        [&] { return _this->check_single_path(true  , true , true   , 8            , 4         , 1024*256, 65536); }, // Target limited bandwidth is the bottleneck
        [&] { return _this->check_single_path(false , true , false  , 8            , 4         , 1024*256, 65536); }, // Target limited bandwidth is the bottleneck
        [&] { return _this->check_single_path(false , true , true   , 8            , 4         , 1024*256, 65536); }, // Target limited bandwidth is the bottleneck
        [&] { return _this->check_single_path(true  , true , false  , 8            , 64         , 1024*256, 65536); }, // writes put address on same channel before data, this reduces bw for small bursts
        [&] { return _this->check_single_path(true  , true , true   , 8            , 64         , 1024*256, 65536); }, // writes put address on same channel before data, this reduces bw for small bursts
        [&] { return _this->check_single_path(false , true , false  , 8            , 64         , 1024*256, 32768); },
        [&] { return _this->check_single_path(false , true , true   , 8            , 64         , 1024*256, 65536); }, // writes put address on narrow channel which conflicts with narrow generator as full bandwidth is already taken
        // Check wide channel with write channel-width bursts to target reduced bandwidth
        [&] { return _this->check_single_path(true  , true , false  , 64           , 8         , 1024*256, 32768); }, // Target limited bandwidth is the bottleneck
        // Check wide and narrow channels with write wide-channel-width bursts to target reduced bandwidth
        // [&] { return _this->check_single_path(true  , true , true   , 64           , 8         , 1024*256, 36864); }, // Big burst size amortizes header cost
        // [&] { return _this->check_single_path(false , true , false  , 64           , 8         , 1024*256, 32768); }, // No header cost, done in parallel
        // [&] { return _this->check_single_path(false , true , true   , 64           , 8         , 1024*256, 32768); }, // No header cost, done in parallel
        // Check wide channel with write channel-width bursts
        [&] { return _this->check_single_path(true  , true , false  , 64           , 64        , 1024*256, 8192); }, // Bandwidth divided by 2 due to header cost
        // Check wide channel with write big bursts
        [&] { return _this->check_single_path(true  , true , false  , 256          , 64        , 1024*256, 5120); }, // Big burst size amortizes header cost
        [&] { return _this->check_2_paths_through_same_node(); },
        [&] { return _this->check_2_paths_to_same_target(); },
        [&] { return _this->check_prefered_path(); },
    };

    if (testcases[_this->testcase]())
    {
        _this->testcase++;
        if (_this->testcase >= testcases.size())
        {
            _this->top->test_end(_this->status);
        }
        else
        {
            _this->step = 0;
            _this->fsm_event.enqueue();
        }
    }
}

void Test0::exec_test()
{
    this->status = false;
    this->step = 0;
    this->testcase = 0;
    this->fsm_event.enqueue();
}
