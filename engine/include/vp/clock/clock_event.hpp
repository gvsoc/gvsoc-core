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

namespace vp
{

    class Block;
    class ClockEvent;
    class Component;
    class BlockClock;
    class ClockEngine;

#define VP_CLOCK_EVENT_NB_ARGS 8

    typedef void(ClockEventMeth)(vp::Block *, ClockEvent *event);

    /**
     * @brief Clock events
     *
     * Clocks events can be used to schedule functions execution at specific clock cycles in
     * order to model timing.
     * Each event can schedule one function execution at the same time.
     * There are 2 ways of using events:
     * - Enqueue them everytime they need to be executed. This is the most flexible way to use
     * them but also the slowest. This should be used for models where functions mostly execute
     * from time to time.
     * - Enable and disable them so that they execute by default at each cycle. This is the
     * fastest way to execute events. They can also have some stall cycles to prevent them
     * from executing at each cycle.
     */
    class ClockEvent
    {

        friend class ClockEngine;
        friend class Component;
        friend class Block;
        friend class BlockClock;

    public:
        /**
         * @brief Construct a new event
         *
         * Events must be created associated to a block, from which they will get the clock engine,
         * which is the one where events will be posted.
         *
         * @note With this constructor, the method must be set later on and before the event is
         * enqueued for the first time.
         *
         * @param comp The block owning this events.
         */
        ClockEvent(Block *comp);

        /**
         * @brief Construct a new event
         *
         * Events must be created associated to a block, from which they will get the clock engine,
         * which is the one where events will be posted.
         *
         * @param comp The block owning this events.
         * @param meth The event callback to be called when event gets executed.
         */
        ClockEvent(Block *comp, ClockEventMeth *meth);

        /**
         * @brief Construct a new event
         *
         * Clocks events can be used to schedule functions execution at specific clock cycles in
         * order to model timing.
         * Each event can schedule one function execution at the same time.
         * There are 2 ways of using events:
         * - Enqueue them everytime they need to be executed. This is the most flexible way to use
         * them but also the slowest. This should be used for models where functions mostly execute
         * from time to time.
         * - Enable and disable them so that they execute by default at each cycle. This is the
         * fastest way to execute events. They can also have some stall cycles to prevent them
         * from executing at each cycle.
         * Events must be created associated to a block, from which they will get the clock engine,
         * which is the one where events will be posted.
         *
         * @param comp The block owning this events.
         * @param _this The block owning the callback. This can be used to use a callback from a
         *     child of the block owning the event.
         * @param meth The event callback to be called when event gets executed.
         */
        ClockEvent(Block *comp, vp::Block *_this, ClockEventMeth *meth);

        /**
         * @brief Destruct an event
         *
         */
        ~ClockEvent();

        /**
         * @brief Set the event callback
         *
         * The goal of clock events is to execute functions at specific cycles in order to
         * model timing.
         * The function to be called when the clock engine of the event reaches the cycles where
         * the event must be executed can be set with this method.
         *
         * @param Event callback to be called when event gets executed
         */
        inline void set_callback(ClockEventMeth *meth);

        /**
         * @brief Get the event arguments
         *
         * Since the event is passed as a parameter to the event callback when it gets called, it
         * is possible to store in the event some arguments which can then be easily used in the
         * callback.
         * This method can be called to get the array of arguments.
         *
         * @return The event arguments
         */
        inline void **get_args();

        /**
         * @brief Execute the event
         *
         * This executes the event callback. This is normally used by the clock engine to call
         * event callbacks when events gets executed but can also be called by models in case the
         * event is not enqueued and needs to be executed immediately.
         */
        inline void exec();

        /**
         * @brief Enqueue an event
         *
         * This enqueues the event into its clock engine so that it gets executed once at the
         * specified number of cycles.
         *
         * @note Once the event gets executed, it is ready to be enqueued again from the event
         * callback.
         * @note This method must be used only in the mode where the event is enqueued everytime
         * it must execute.
         *
         * @param cycles The relative number of cycles at which the event should get executed.
         */
        inline void enqueue(int64_t cycles = 1);

        /**
         * @brief Cancel an event
         *
         * This can be called to cancel an event which has been enqueued for execution but has not
         * yet been executed.
         * The event is then removed from the clock engine queues and can then be reenqueue.
         *
         * @note This should be called only if the event is enqueued.
         * @note This method must be used only in the mode where the event is enqueued everytime
         * it must execute.
         */
        inline void cancel();

        /**
         * @brief Tell if the event is enqueued
         *
         * This can be called to know is the event is currently enqueued in the clock engine
         * queues waiting to be executed.
         * The event is flagged as enqueued as soon as it is enqueued, and the flag is cleared
         * when it is either executed or canceled.
         *
         * @return True if the event is enqueued.
         */
        inline bool is_enqueued();

        /**
         * @brief Enable the event
         *
         * Once enabled, the event will get execute at each cycle, until it is disabled.
         * It can still bypass cycles if stall cycles are registered.
         *
         * @note This method must be used only in the mode where the event is enabled to execute
         * at every cycle.
         */
        inline void enable();

        /**
         * @brief Disable the event
         *
         * Once disabled, the event does not execute anymore, until it is enabled again.
         *
         * @note This method must be used only in the mode where the event is enabled to execute
         * at every cycle.
         */
        inline void disable();

        /**
         * @brief Stall the event.
         *
         * This will prevent the event from executing during the specified number of events.
         *
         * @note This method must be used only in the mode where the event is enabled to execute
         * at every cycle.
         * @note This can be used either when the event is enabled or disabled.
         */
        inline void stall_cycle_set(int64_t value);

        /**
         * @brief Increase the number of stall cycles of the event.
         *
         * This will prevent the event from executing during the specified number of events, on top
         * of the current stalls of the event.
         *
         * @note This method must be used only in the mode where the event is enabled to execute
         * at every cycle.
         * @note This can be used either when the event is enabled or disabled.
         */
        inline void stall_cycle_inc(int64_t inc);

        /**
         * @brief Get the number of stall cycles.
         *
         * This gives the remaining number of stall cycles.
         *
         * @note This method must be used only in the mode where the event is enabled to execute
         * at every cycle.
         * @note This can be used either when the event is enabled or disabled.
         *
         * @return The number of stall cycles
         */
        inline int64_t stall_cycle_get();

        // Get the number of cycles at which this event should execute. Used by the clock engine
        // to schedule this event
        inline int64_t get_cycle();


    private:
        // Set the clock domain of this event. This is done by the framework when the clock
        // is registered in the block owning this event
        inline void set_clock(ClockEngine *clock);

        // Block owning this event
        Block *comp;
        // Event callback called when this event gets executed
        ClockEventMeth *meth;
        ClockEventMeth *meth_saved;
        // Instance of the event, given as first argument of the event callback when called
        vp::Block *_this;
        // Events arguments, models are free to use them
        void *args[VP_CLOCK_EVENT_NB_ARGS];
        // Pointer for event chaining
        ClockEvent *next;
        // Pointer for event chaining
        ClockEvent *prev;
        // True if the event is enqueued into the clock engine
        bool enqueued;
        // Absolute number of cycles where the event should be executed
        int64_t cycle;
        // In case the event is permanently enabled, this tells during how many cycles it should
        // not execute
        int64_t stall_cycle;
        // Clock engine owning this event
        ClockEngine *clock;
        bool pending_disable = false;
    };

};
