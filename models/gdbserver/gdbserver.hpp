/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
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
 * Authors: Andreas Traber
 */

#ifndef __VP_GDB_SERVER_HPP__
#define __VP_GDB_SERVER_HPP__

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include "vp/gdbserver/gdbserver_engine.hpp"
#include <mutex>
#include <condition_variable>


class Rsp;


class Gdb_server : public vp::component, vp::Gdbserver_engine
{
public:
    Gdb_server(js::config *config);

    void pre_pre_build();
    int build();
    void start();

    int io_access(uint32_t addr, int size, uint8_t *data, bool is_write);

    int register_core(vp::Gdbserver_core *core);
    void signal(vp::Gdbserver_core *core, int signal, std::string reason="", uint64_t info=0);
    int set_active_core(int id);
    int set_active_core_for_other(int id);
    vp::Gdbserver_core *get_core(int id=-1);
    vp::Gdbserver_core *get_active_core();
    vp::Gdbserver_core *get_active_core_for_other();
    std::vector<vp::Gdbserver_core *> get_cores() { return this->cores_list; }

    void breakpoint_insert(uint64_t addr);
    void breakpoint_remove(uint64_t addr);

    void watchpoint_insert(bool is_write, uint64_t addr, int size);
    void watchpoint_remove(bool is_write, uint64_t addr, int size);

    void lock() override { this->get_time_engine()->lock(); }
    void unlock() override { this->get_time_engine()->unlock(); }

    void exit(int status) override;

    vp::trace     trace;
    int default_hartid;


private:
    Rsp *rsp;
    std::unordered_map<int, vp::Gdbserver_core *> cores;
    std::vector<vp::Gdbserver_core *> cores_list;
    int active_core;
    int active_core_for_other;
    std::mutex mutex;
    std::condition_variable cond;
    std::unordered_map<uint64_t, bool> breakpoints;

};

#endif