/*
 * Copyright (C) 2020  GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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

#include <stdint.h>
#include <vector>
#include <vp/clock/block_clock.hpp>
#include <vp/time/block_time.hpp>
#include <vp/power/block_power.hpp>
#include <vp/trace/block_trace.hpp>
#include <vp/clock/clock_event.hpp>

namespace gv {
    class GvProxy;
    class GvsocLauncher;
};

namespace vp {

    class SignalCommon;
    class RegisterCommon;
    class TraceEngine;
    class reg;

    class BlockObject
    {
    public:
        BlockObject(Block &parent);
        virtual void reset(bool active) {};
    };

    /**
     * @brief Block model
     *
     * The block is the basic building block of a hardware model.
     * It provides features for handling time, clock cycles, power, and traces.
     * Contrary to components, blocks communicate together directly through their class methods.
     */
    class Block
    {
        friend class gv::GvProxy;
        friend class vp::SignalCommon;
        friend class vp::RegisterCommon;
        friend class gv::GvsocLauncher;
        friend class vp::BlockPower;
        friend class vp::BlockTime;
        friend class vp::BlockTrace;
        friend class vp::BlockClock;
        friend class vp::PowerTrace;
        friend class vp::CompPowerReport;
        friend class vp::ClockEngine;
        friend class vp::TraceEngine;
        friend class vp::Component;
        friend class vp::TimeEngine;
        friend class vp::BlockObject;

    public:
        /**
         * @brief Construct a new block
         *
         * @param parent Specify the parent block containing this block.
         * @param name Specify the block name.
         */
        Block(Block *parent, std::string name, vp::TimeEngine *time_engine=NULL,
        vp::TraceEngine *trace_engine=NULL, vp::PowerEngine *power_engine=NULL);
        ~Block();

        /**
         * @brief Get block name
         *
         * The name of the blocks is local to its parent and unique, so that it identifies
         * the block within its parent.
         *
         * @return The block name, specified when it was created.
         */
        string get_name();

        /**
         * @brief Get block path
         *
         * The path of the block is the concatenation of the names of his hierarchy of parents
         * from the top parent to this block.
         * This is a string which uniquely identifies the block in the the full system.
         * The path is of the form /<parent0 name>/<parent1 name>/.../<block name>.
         *
         * @return The block path.
         */
        string get_path();

        /**
         * @brief Reset method
         *
         * Method containing all what should be done when the block is resetted.
         * The virtual method is doing nothing and can be overloaded in order to model a reset.
         *
         * @note This should never be called directly, the framework will call it automatically
         * when a reset is happening.
         *
         * @note Asserting the reset should clear internal state while releasing may trigger some
         * events to start some activity.
         *
         * @param active True when the reset is asserted, false when it is released.
         */
        virtual void reset(bool active) {}

        /**
         * @brief Start method
         *
         * This method can be overloaded by components in order to execute code after the whole
         * system has been instantiated and connected, and before the reset is done.
         * This can be used for example to do some initializations which require calling component
         * interfaces.
         *
         * @note This should never be called directly, the framework will call it automatically
         * after te system has been instatiated.
         */
        virtual void start() {}

        /**
         * @brief Stop method
         *
         * This method can be overloaded by components in order to execute code before the
         * simulation is closed.
         * This can be used for example to close some files.
         *
         * @note This should never be called directly, the framework will call it automatically
         * after te system has been instatiated.
         */
        virtual void stop() {}

        /**
         * @brief Flush method
         *
         * This method can be overloaded by components in order to execute code just before the
         * simulation gets to an interactive point.
         * This can be used for example to flush some files when a gdb breakpoint is hit, or when
         * the profiler is pausing the simulation.
         *
         * @note This should never be called directly, the framework will call it automatically
         * after te system has been instatiated.
         */
        virtual void flush() {}

        /**
         * @brief External bind method
         *
         * This method can be overloaded by components in order to catch bind requests done by
         * external controllers.
         * These requests are done through the GVSOC api in gv/gvsoc.hpp and are meant to bind
         * external code to internal components like routers.
         *
         * @note This should never be called directly, the framework will call it automatically
         * after te system has been instantiated.
         *
         * @param path Path of the component where the interface should be bound
         * @param itf_name Name of the interface to be bound
         * @param handle Handle of the external code which must be used for interacting. The type
         *     of the handle depends on the interface type.
         * @return A handle that the external code can use to interact with the bound component. The
         *     type of the handle depends on the interface type.
         */
        virtual void *external_bind(std::string path, std::string itf_name, void *handle);

        /**
         * @brief Handle a command from the proxy
         *
         * This method can be overloaded by components in order to handle commands sent by the
         * proxy.
         *
         * @note This should never be called directly, the framework will call it automatically
         * after te system has been instantiated.
         *
         * @param proxy Proxy instance, which can be used to interact with it.
         * @param req_file File descriptor which can be used to read additonal data if needed.
         * @param reply_file File descriptor which can be used to reply to the command.
         * @param args Command to be handled.
         * @param req Request string to be given when replying to the command.
         */
        virtual std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file,
            std::vector<std::string> args, std::string req) { return ""; }

        /**
         * @brief Power supply method method
         *
         * This virtual method can be overloaded in order to be notified when the power supply
         * of the block is modified.
         * This can be used for example to adapt the power consumption of the block when its
         * power supply is modified.
         *
         * @param state New state of the power supply.
         */
        virtual void power_supply_set(vp::PowerSupplyState state) {}

        /**
         * @brief Declare a new service
         *
         * A service is a way to broadcast a class to all components so that they can access it
         * without being connected to it through the classic component bindings.
         *
         * @param name Name of the service
         * @param service Service to be added, which should be the instance of the class to be
         *     accessed by the other components.
         */
        void new_service(std::string name, void *service);

        /**
         * @brief Get a service
         *
         * This gets the instance associated to the service.
         * This methods must be called from a start method.
         *
         * @param name Name of the service
         * @return The service
         */
        void *get_service(string name);

        /**
         * @brief Get the default block trace
         *
         * This trace can be used to report events about this block.
         *
         * @return The trace
         */
        inline Trace *get_trace() { return &this->block_trace; }

        /**
         * @brief Reset the hierarchy of the block
         *
         * This can be called to reset this block and all its childs.
         *
         * @param active True if the reset is asserted
         * @param from_itf Reserved for the framework, should be kept to its default value.
         */
        virtual void reset_all(bool active, bool from_itf=false);

        /**
         * @brief Give access to tracing features
         *
         * This class gather all available features for system traces.
         */
        vp::BlockTrace traces;

        /**
         * @brief Give access to power features
         *
         * This class gather all available features for modeling power consumption.
         */
        vp::BlockPower power;

        /**
         * @brief Give access to time features
         *
         * This class gather all available features for modeling the global time.
         */
        vp::BlockTime time;

        /**
         * @brief Give access to clock features
         *
         * This class gather all available features for modeling clock cycles.
         */
        vp::BlockClock clock;

        /**
         * @brief DEPRECATED
        */
        inline void event_enqueue(ClockEvent *event, int64_t cycles);

        /**
         * @brief DEPRECATED
        */
        inline void event_reenqueue(ClockEvent *event, int64_t cycles);

        /**
         * @brief DEPRECATED
        */
        inline void event_enqueue_ext(ClockEvent *event, int64_t cycles);

        /**
         * @brief DEPRECATED
        */
        inline void event_reenqueue_ext(ClockEvent *event, int64_t cycles);

        /**
         * @brief DEPRECATED
        */
        inline void event_cancel(ClockEvent *event);

        /**
         * @brief DEPRECATED
        */
        ClockEvent *event_new(ClockEventMeth *meth);

        /**
         * @brief DEPRECATED
        */
        ClockEvent *event_new(vp::Block *_this, ClockEventMeth *meth);

        /**
         * @brief DEPRECATED
        */
        void event_del(ClockEvent *event);

    private:

        /*
         * Private members accessed by other classes of the framework
         */
        // Used by clock domain to register the clocks
        virtual void pre_start() {}

        // Used by the trace engine to get a trace from the full path by going through
        // the whole hierarchy
        void get_trace_from_path(std::vector<vp::Trace *> &traces, std::string path);

        // Used by external controllers (launcher or proxy)
        vp::Block *get_block_from_path(std::vector<std::string> path_list);

        // Get the block containing this block
        vp::Block *get_parent() { return this->parent; }

        // Get all blocks contained by this block
        std::vector<vp::Block *> get_childs() { return this->childs; }

        // Called by the framework to add a child block into this block hierarchy
        void add_block(Block *block);

        // Called by the framework to remove a child block from this block hierarchy
        void remove_block(Block *block);

        // Called by the framework to add a child signal into this block hierarchy
        void add_signal(SignalCommon *signal);

        // Called by the framework to add a child register into this block hierarchy
        void add_register(RegisterCommon *reg);

        // Can be called to know if this block is a component
        virtual bool is_component() { return false; }

        // Flush the whole hierarchy of this block. This is called by the top component.
        void flush_all();

        // Stop the whole hierarchy of this block This is called by the top component.
        void stop_all();

        // Prestart the whole hierarchy of this block This is called by the top component.
        void pre_start_all();

        // Start the whole hierarchy of this block This is called by the top component.
        void start_all();

        void register_object(vp::BlockObject *object);


        /*
         * Real private members
         */
        // Used by this class to propagate a service until the top block
        void add_service(std::string name, void *service);

        // Implement the scheduler for time events, and is overloaded in clock_engine
        virtual int64_t exec();

        // Used by this class to get the block path from its parents, it is then store locally
        std::string get_path_from_parents();

        // BLock name
        std::string name;
        // Block path
        std::string path;
        // Block parent
        vp::Block *parent;
        // Block childs
        std::vector<vp::Block *> childs;
        // Block signals
        std::vector<SignalCommon *> signals;
        // Block registers
        std::vector<RegisterCommon *> registers;
        // Other kinds of block objects
        std::vector<vp::BlockObject *> objects;
        // Block old-style registers, to be removed
        vector<vp::reg *> regs;
        // Services from this block and its childs
        std::map<std::string, void *> services;
        // Block trace
        Trace block_trace;
        // Tells if the reset is connected to an interface or is coming from parent
        bool reset_is_bound = false;
    };
};
