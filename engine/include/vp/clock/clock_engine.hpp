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

#include "vp/component.hpp"
#include "vp/time/time_engine.hpp"
#include <vp/itf/clk.hpp>
#include <vp/itf/implem/clock_class.hpp>

namespace vp
{

    class ClockEvent;
    class Component;

    /**
     * @brief Clock engine
     *
     * This class is instantiated once for each clock domain.
     * It is in charge to managing the cycles inside the clock domain.
     * This mostly consists of executing functions at specific cycles.
     */
    class ClockEngine : public vp::Component
    {
        friend class vp::Block;
        friend class vp::BlockClock;
        friend class vp::ClockEvent;
        friend class vp::BlockTrace;
        friend class vp::TraceEngine;

    public:
        /**
         * @brief Construct a new clock engine
         *
         * The clock engine is a component which must be instantiated only from Python
         * generators, as it needs to be connected to the components of the clock domain
         * though component bindings.
         *
         * @param config The component config coming from Python generator.
         */
        ClockEngine(vp::ComponentConf &config);

        void start();

        /**
         * @brief Get current cycles
         *
         * This returns the current absolute number of cycles of this clock engine.
         *
         * @note Cycles are set to 0 at each reset.
         */
        inline int64_t get_cycles() { return cycles; }

        /**
         * @brief Get cycle period
         *
         * This returns the duration in picoseconds of one cycle.
         *
         * @note The period should not be stored and instead should be queried at each cycle if
         * need, as it can vary at each cycle due to the frequency which can be changed at each
         * cycle.
         */
        int64_t get_period() { return period; }

        /**
         * @brief Get clock engine frequency
         *
         * This returns the frequency in Hz.
         *
         * @note The frequency should not be stored and instead should be queried at each cycle if
         * need, as it can be changed at each cycle.
         */
        int64_t get_frequency() { return freq; }

        /**
         * @brief Synchronize the engine
         *
         * Since the engine is synchronizing its cycle count only when it has a clock event to,
         * execute, it has to be explicitely synchronized in some cases bby calling this method.
         * This must be called everytime a function call is crossing 2 different clock domains.
         * This can happen for example when a component from a clock domain is calling another
         * component from another clock domain. This case is automatically handled by the
         * interface wrappers.
         * This can also happen in case a component is having an access directly through a
         * class instead of a binding, like for example in components having 2 clock domains. In
         * this case it is important to call this method before anything of the target component
         * is used.
         */
        inline void sync();

        void reorder_permanent_events();

        /**
         * @brief DEPRECATED
        */
        void cancel(ClockEvent *event);

        /**
         * @brief DEPRECATED
        */
        ClockEvent *reenqueue(ClockEvent *event, int64_t cycles);

        /**
         * @brief DEPRECATED
        */
        ClockEvent *enqueue(ClockEvent *event, int64_t cycles);

        /**
         * @brief DEPRECATED
        */
        inline ClockEvent *enqueue_ext(ClockEvent *event, int64_t cycles)
        {
            this->sync();
            return this->enqueue(event, cycles);
        }


    private:
        static void stalled_event_handler(vp::Block *, ClockEvent *event);

        void update();

        vp::ClockEvent *get_next_event();

        void event_del(Block *comp, ClockEvent *event)
        {
            delete event;
        }

        void disable(ClockEvent *event);

        void apply_frequency(int64_t frequency);

        ClockEvent *reenqueue_ext(ClockEvent *event, int64_t cycles);

        ClockEvent *enable(ClockEvent *event);

        void reenqueue_to_engine();

        bool dequeue_from_engine();

        int64_t exec();

        bool has_events() { return this->nb_enqueued_to_cycle || this->delayed_queue || this->permanent_first; }

        void pre_start();

        static void set_frequency(vp::Block *__this, int64_t frequency);

        static void apply_frequency_handler(vp::Block *_this, vp::ClockEvent *event);

        void change_frequency(int64_t frequency);

        vp::ClkMaster out;

        vp::ClockSlave clock_in;

        vp::Trace clock_trace;

        int factor;
        ClockEvent *delayed_queue = NULL;
        ClockEvent *permanent_first = NULL;
        ClockEvent *permanent_last = NULL;
        int current_cycle = 0;
        int64_t period = 0;
        int64_t freq;

        // Gives the current cycle count of this engine.
        // It is always usable, whatever the state of the engine.
        // It is updated either when events are executed or when the
        // engine is updated by an external interaction.
        int64_t cycles = 0;

        // Tells how many events are enqueued to the circular buffer.
        // If it is zero, there could still be some events in the delayed queue.
        int nb_enqueued_to_cycle = 0;

        // This time is relevant only when no event is enqueued into the circular
        // buffer of event so that the number of cycles can be resynchronized when
        // something happen (an event is pushed or the frequency is changed).
        // This is set when control is given back to the time engine and used
        // to recompute the numer of cycles when the engine is updated by an
        // external event.
        int64_t stop_time = 0;

        vp::Trace cycles_trace;

        vp::TimeEngine *time_engine = NULL;

        int64_t next_delayed_cycle = INT64_MAX;

        vp::ClockEvent apply_frequency_event;
        int64_t frequency_to_be_applied;
    };

};

#include <vp/itf/implem/clock.hpp>