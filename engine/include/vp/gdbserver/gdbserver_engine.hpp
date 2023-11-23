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

#ifndef __VP_GDBSERVER_GDBSERVER_ENGINE_HPP__
#define __VP_GDBSERVER_GDBSERVER_ENGINE_HPP__

#include "vp/component.hpp"

namespace vp
{

    class Gdbserver_core
    {
    public:
        typedef enum
        {
            running,
            stopped
        } state;

        virtual int gdbserver_get_id() = 0;
        virtual void gdbserver_set_id(int i) = 0;
        virtual std::string gdbserver_get_name() = 0;
        virtual int gdbserver_reg_set(int reg, uint8_t *value) = 0;
        virtual int gdbserver_reg_get(int reg, uint8_t *value) = 0;
        virtual int gdbserver_regs_get(int *nb_regs, int *reg_size, uint8_t *value) = 0;
        virtual int gdbserver_stop() = 0;
        virtual int gdbserver_cont() = 0;
        virtual int gdbserver_stepi() = 0;
        virtual int gdbserver_state() = 0;
        virtual void gdbserver_breakpoint_insert(uint64_t addr) = 0;
        virtual void gdbserver_breakpoint_remove(uint64_t addr) = 0;
        virtual void gdbserver_watchpoint_insert(bool is_write, uint64_t addr, int size) = 0;
        virtual void gdbserver_watchpoint_remove(bool is_write, uint64_t addr, int size) = 0;
        virtual int gdbserver_io_access(uint64_t addr, int size, uint8_t *data, bool is_write) = 0;

    };


    class Gdbserver_engine
    {
    public:
        static const int SIGNAL_NONE = 0;
        static const int SIGNAL_INT = 2;
        static const int SIGNAL_ILL = 4;
        static const int SIGNAL_TRAP = 5;
        static const int SIGNAL_ABRT = 6;
        static const int SIGNAL_BUS = 10;
        static const int SIGNAL_STOP = 17;

        virtual int register_core(Gdbserver_core *core) = 0;
        virtual void signal(Gdbserver_core *core, int signal, std::string reason="", int info=0) = 0;

        virtual void lock() = 0;
        virtual void unlock() = 0;

        virtual void exit(int status) = 0;
    };

};

#endif
