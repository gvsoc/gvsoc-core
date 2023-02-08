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

#pragma once

#include <types.hpp>
#include <mutex>
#include <condition_variable>

class Watchpoint
{
public:
    Watchpoint(iss_addr_t addr, int size) : addr(addr), size(size) {}
    iss_addr_t addr;
    int size;
};

class Gdbserver : public vp::Gdbserver_core
{
public:
    Gdbserver(Iss &iss);
    void build();
    void start();
    void reset(bool active);

    bool is_enabled() { return this->gdbserver != NULL; }

    int gdbserver_get_id();
    std::string gdbserver_get_name();
    int gdbserver_reg_set(int reg, uint8_t *value);
    int gdbserver_reg_get(int reg, uint8_t *value);
    int gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value);
    int gdbserver_stop();
    int gdbserver_cont();
    int gdbserver_stepi();
    int gdbserver_state();
    void gdbserver_breakpoint_insert(uint64_t addr);
    void gdbserver_breakpoint_remove(uint64_t addr);
    void gdbserver_watchpoint_insert(bool is_write, uint64_t addr, int size);
    void gdbserver_watchpoint_remove(bool is_write, uint64_t addr, int size);
    int gdbserver_io_access(uint64_t addr, int size, uint8_t *data, bool is_write);

    void enable_breakpoint(iss_addr_t addr);
    void disable_breakpoint(iss_addr_t addr);
    void enable_all_breakpoints();
    bool watchpoint_check(bool is_write, iss_addr_t addr, int size);

    void handle_pending_io_access();
    static void handle_pending_io_access_stub(void *__this, vp::clock_event *event);
    static void data_response(void *_this, vp::io_req *req);

    Iss &iss;
    vp::io_master io_itf;
    vp::io_req io_req;
    vp::trace trace;
    vp::clock_event *event;
    vp::Gdbserver_engine *gdbserver;
    std::list<iss_addr_t> breakpoints;
    bool halt_on_reset;
    std::mutex mutex;
    std::condition_variable cond;
    int io_retval;
    bool waiting_io_response;
    uint64_t io_pending_addr;
    int io_pending_size;
    uint8_t *io_pending_data;
    bool io_pending_is_write;
    std::list<Watchpoint *> write_watchpoints;
    std::list<Watchpoint *> read_watchpoints;
};