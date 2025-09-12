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

#include "test.hpp"


Test0::Test0(Testbench *top)
: TestCommon(top, "test0"), fsm_event(this, Test0::entry)
{
    this->top = top;
}

void Test0::entry(vp::Block *__this, vp::ClockEvent *event)
{
    Test0 *_this = (Test0 *)__this;

    size_t size = 2048;
    size_t bw0 = _this->top->packet_size, bw1 = _this->top->bandwidth;
    int nb_inputs = 1;
    int nb_gens_per_input = 1;
    bool do_read = _this->top->access_type == "r" || _this->top->access_type == "rw";
    bool do_write = _this->top->access_type == "w" || _this->top->access_type == "rw";

    if (_this->step == 0)
    {
        printf("Test 0 entry (1 generator to 1 receiver)\n");

        for (int i=0; i<nb_inputs; i++)
        {
            for (int j=0; j<nb_gens_per_input; j++)
            {
                uint64_t offset = 0x10000000 + 0x10000 * (i * nb_gens_per_input + j) * 2;
                if (do_read)
                {
                    _this->top->get_generator(i, j, 0)->start(offset, size, bw0, &_this->fsm_event, false);
                }
                if (do_write)
                {
                    _this->top->get_generator(i, j, 1)->start(offset + 0x10000, size, bw0, &_this->fsm_event, true);
                }
            }
        }

        _this->clockstamp = _this->clock.get_cycles();
        _this->step = 1;
    }
    else
    {
        if (!_this->top->is_finished()) return;

        int64_t cycles = _this->clock.get_cycles() - _this->clockstamp;
        int64_t expected = size * nb_inputs * nb_gens_per_input / std::min(bw0, bw1);

        if (do_read && do_write && _this->top->shared_rw)
        {
            expected *= 2;
        }

        printf("    Done (size: %lld, bw: %lld, cycles: %lld, expected: %lld)\n",
            size, bw1, cycles, expected);

        int status = _this->top->check_cycles(cycles, expected);

        _this->top->test_end(status);
    }
}

void Test0::exec_test()
{
    this->step = 0;
    this->fsm_event.enqueue();
}
