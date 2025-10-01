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

void Test0::entry(vp::Block *__this, vp::ClockEvent *event)
{
    Test0 *_this = (Test0 *)__this;

    switch (_this->step)
    {
        case 0:
        case 1:
        {
            size_t size = 1024*256;
            size_t bw0 = 8, bw1 = 4;
            int x0=0, y0=0, x1=_this->top->nb_cluster_x-1, y1=_this->top->nb_cluster_y-1;

            if (_this->step == 0)
            {
                printf("Test 0, checking stalls, high bandwidth source going to low bandwidth target\n");

                // Check communications from top/left cluster to bottom/right. Target cluster has half
                // the bandwidth of the initiator cluster

                uint64_t base1 = _this->top->get_cluster_base(x1, y1);

                _this->top->get_generator(x0, y0)->start(base1, size, bw0, &_this->fsm_event, true, true);
                _this->top->get_receiver(x1, y1)->start(bw1);

                _this->step++;
            }
            else
            {
                int64_t cycles;
                int64_t expected = size / bw1;

                bool is_finished = true;
                bool check_status = false;

                is_finished &= _this->top->get_generator(x0, y0)->is_finished(&check_status, &cycles);

                bool status = check_status;
                status |= _this->top->check_cycles(cycles, expected) != 0;

                printf("    %s (check: %d, size: %lld, bw: %lld, cycles: %lld, expected: %lld)\n",
                    status ? "Failed" : "Done", check_status, size, bw1, cycles, expected);

                _this->status |= status;
                _this->step++;
                _this->fsm_event.enqueue();
            }
            break;
        }

        case 2:
        case 3:
        {
            // One horizontal and one vertical paths, both in the middle, so that they cross in the center
            // of the grid
            int x0=(_this->top->nb_cluster_x-1)/2, y0=0;
            int x1=(_this->top->nb_cluster_x-1)/2, y1=_this->top->nb_cluster_y-1;
            int x2=0, y2=(_this->top->nb_cluster_y-1)/2;
            int x3=_this->top->nb_cluster_x-1, y3=(_this->top->nb_cluster_y-1)/2;
            size_t size = 1024*256;
            size_t bw = 8;

            if (_this->step == 2)
            {
                printf("Test 1, checking 2 data streams going through same router but using different paths\n");

                uint64_t base1 = _this->top->get_cluster_base(x1, y1);
                uint64_t base3 = _this->top->get_cluster_base(x3, y3);

                _this->top->get_generator(x0, y0)->start(base1, size, bw, &_this->fsm_event, false, true);
                _this->top->get_receiver(x1, y1)->start(bw);
                _this->top->get_generator(x2, y2)->start(base3, size, bw, &_this->fsm_event, false, true);
                _this->top->get_receiver(x3, y3)->start(bw);

                _this->step++;
            }
            else
            {
                int64_t cycles = 0;
                bool is_finished = true;
                bool check_status = false;

                is_finished &= _this->top->get_generator(x0, y0)->is_finished(&check_status, &cycles);
                is_finished &= _this->top->get_generator(x2, y2)->is_finished(&check_status, &cycles);

                if (is_finished)
                {
                    int64_t expected = size / bw;

                    bool status = _this->top->check_cycles(cycles, expected) != 0;

                    printf("    %s (size: %lld, bw: %lld, cycles: %lld, expected: %lld)\n",
                        status ? "Failed" : "Done", size, bw, cycles, expected);

                    _this->status |= status;
                    _this->step++;
                    _this->fsm_event.enqueue();
                }
            }
            break;
        }

        case 4:
        case 5:
        {
            // One generator on top/right, another one on bottom/left and the receiver on bottom/right
            int x0=_this->top->nb_cluster_x-1, y0=0;
            int x1=0, y1=_this->top->nb_cluster_y-1;
            int x2=_this->top->nb_cluster_x-1, y2=_this->top->nb_cluster_y-1;
            size_t size = 1024*256;
            size_t bw1 = 4;
            size_t bw2 = 8;

            if (_this->step == 4)
            {
                printf("Test 2, checking 2 generators going to same receiver\n");

                uint64_t base = _this->top->get_cluster_base(x2, y2);

                _this->top->get_generator(x0, y0)->start(base, size, bw1, &_this->fsm_event, false, true);
                _this->top->get_generator(x1, y1)->start(base + 0x10000, size, bw1, &_this->fsm_event, false, true);
                _this->top->get_receiver(x2, y2)->start(bw2);

                _this->clockstamp = _this->clock.get_cycles();
                _this->step++;
            }
            else
            {
                int64_t cycles = 0;
                bool is_finished = true;
                bool check_status = false;

                is_finished &= _this->top->get_generator(x0, y0)->is_finished(&check_status, &cycles);
                is_finished &= _this->top->get_generator(x1, y1)->is_finished(&check_status, &cycles);

                if (is_finished)
                {
                    // Since we have 2 generators going to same target, bandwidth is divided by 2
                    int64_t expected = size / (bw1 / 2);

                    bool status = _this->top->check_cycles(cycles, expected) != 0;

                    printf("    %s (size: %lld, bw: %lld, cycles: %lld, expected: %lld)\n",
                        status ? "Failed" : "Done", size, bw1, cycles, expected);

                    _this->status |= status;
                    _this->step++;
                    _this->fsm_event.enqueue();
                }
            }
            break;
        }

        case 6:
        case 7:
        {
            size_t size = 1024*256;
            size_t bw0 = 8;

            if (_this->step == 6)
            {
                printf("Test 3, checking 1 generator to 1 border\n");

                int x0=0, y0=0, x1=_this->top->nb_cluster_x-1, y1=_this->top->nb_cluster_y-1;


                _this->top->get_generator(x0, y0)->start(0xc0200000, size, bw0, &_this->fsm_event);



                _this->clockstamp = _this->clock.get_cycles();
                _this->step++;
            }
            else
            {
                printf("    Done\n");

                _this->step++;
                _this->fsm_event.enqueue();
            }
            break;
        }

        case 8:
        case 9:
        {
            size_t size = 1024*256;
            size_t bw0 = 8;
            int x0=0, y0=0, x1=1, y1=0, x2=2, y2=0;

            if (_this->step == 8)
            {
                printf("Test 4, checking 3 generators to 1 border with prefered direction\n");



                _this->top->get_generator(x0, y0)->start(0x90000000, size, bw0, &_this->fsm_event, false, true);
                _this->top->get_generator(x1, y1)->start(0x90010000, size, bw0, &_this->fsm_event, false, true);
                _this->top->get_generator(x2, y2)->start(0x90020000, size, bw0, &_this->fsm_event, false, true);



                _this->clockstamp = _this->clock.get_cycles();
                _this->step++;
            }
            else
            {
                int64_t cycles = 0;
                bool is_finished = true;
                bool check_status = false;

                is_finished &= _this->top->get_generator(x0, y0)->is_finished(&check_status, &cycles);
                is_finished &= _this->top->get_generator(x1, y1)->is_finished(&check_status, &cycles);
                is_finished &= _this->top->get_generator(x2, y2)->is_finished(&check_status, &cycles);

                if (is_finished)
                {
                    printf("    Done\n");

                    _this->step++;
                    _this->fsm_event.enqueue();
                }
            }
            break;
        }

        default:
            _this->top->test_end(_this->status);
            break;
    }
}

void Test0::exec_test()
{
    this->status = false;
    this->step = 0;
    this->fsm_event.enqueue();
}
