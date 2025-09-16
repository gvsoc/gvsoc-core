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
#include <climits>

class TestCase
{
public:
    std::string desc;
    int size;
    int nb_inputs;
    int nb_gens_per_input;
    int nb_targets;
    float expected_error;
};

static TestCase testcases[] =
{
    // Name, size, nb_inputs, nb_gens_per_input, nb_targets
    { "1 generator to 1 receiver", 2048, 1, 1, 1, 0.05f},
    { "2 generators from 2 different router inputs to 1 receiver", 2048, 2, 1, 1, 0.05f},
    { "2 generators from 2 router inputs to 2 memories", 2048, 2, 1, 2, 0.05f},
    // This one can have a higher error rate since there are more chance a generator blocks
    // for a while the generator on the same input because he gets lots of contentions of its
    { "4 generators from 4 router inputs to 4 memories", 2048, 4, 1, 4, 0.80f},
};

Test0::Test0(Testbench *top)
: TestCommon(top, "test0"), fsm_event(this, Test0::entry)
{
    this->top = top;
}

void Test0::entry(vp::Block *__this, vp::ClockEvent *event)
{
    Test0 *_this = (Test0 *)__this;

    TestCase *testcase = &testcases[_this->test_case];

    if (_this->step == 0)
    {
        printf("Test0 testcase %d entry (%s)\n", _this->test_case, testcase->desc.c_str());

        _this->top->start(testcase->size, testcase->nb_inputs, testcase->nb_gens_per_input,
            testcase->nb_targets, &_this->fsm_event);

        _this->clockstamp = _this->clock.get_cycles();
        _this->step = 1;
    }
    else
    {
        if (!_this->top->is_finished()) return;

        int64_t cycles = _this->clock.get_cycles() - _this->clockstamp;
        int64_t expected = _this->top->get_expected(testcase->size, testcase->nb_inputs,
            testcase->nb_gens_per_input, testcase->nb_targets);

        printf("    Done (size: %d, cycles: %ld, expected: %ld)\n",
            testcase->size, cycles, expected);

        int status = _this->top->check_cycles(cycles, expected, testcase->expected_error);

        _this->test_case++;
        if (_this->test_case == sizeof(testcases) / sizeof(TestCase))
        {
            _this->top->test_end(status);
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
    this->step = 0;
    this->test_case = 0;
    this->fsm_event.enqueue();
}
