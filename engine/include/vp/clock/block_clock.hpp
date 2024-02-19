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

using namespace std;

namespace vp
{
    class TimeEngine;
    class ClockEvent;
    class ClockEngine;
    class Block;
    class Component;

    /**
     * @brief Clocking features for blocks
     *
     * This class is used within the vp::Block class to bring features for managing cycles.
     * This allows creating clock events for scheduling functions execution at specific cycles.
     */
    class BlockClock
    {
        friend class vp::Component;
        friend class vp::Block;
        friend class vp::ClockEvent;
        friend class vp::ClockEngine;

    public:
        /**
         * @brief Construct a new BlockClock
         *
         * @param top Specify the parent block class including this time block.
         */
        BlockClock(vp::Block &top);

        /**
         * @brief Get current cycles
         *
         * This returns the current absolute number of cycles of the clock engine associated to
         * this block.
         *
         * @note Cycles are set to 0 at each reset.
         * @note This is the same as calling the same method directly on the clock engine.
         */
        inline int64_t get_cycles();

        /**
         * @brief Get cycle period
         *
         * This returns the duration in picoseconds of one cycle.
         *
         * @note The period should not be stored and instead should be queried at each cycle if
         * need, as it can vary at each cycle due to the frequency which can be changed at each
         * cycle.
         * @note This is the same as calling the same method directly on the clock engine.
         */
        inline int64_t get_period();

        /**
         * @brief Get clock engine frequency
         *
         * This returns the frequency in Hz.
         *
         * @note The frequency should not be stored and instead should be queried at each cycle if
         * need, as it can be changed at each cycle.
         * @note This is the same as calling the same method directly on the clock engine.
         */
        inline int64_t get_frequency();

        /**
         * @brief Get clock engine
         *
         * Return the clock engine associated to this block.
         *
         * @return The clock engine.
         */
        inline vp::ClockEngine *get_engine();

    private:
        // Set the clock engine of this block and all its sub-blocks
        void set_engine(vp::ClockEngine *engine);
        // Used by clock events to get registered so that the block can cancel them during resets
        void add_clock_event(ClockEvent *);
        // Used by clock events to get unregistered
        void remove_clock_event(ClockEvent *);
        // Clock events currently registered in this clock. This is used to cancel them during
        // resets
        // Access to parent class owning this BlockClock
        vp::Block &top;
        std::vector<ClockEvent *> events;
        // CLock engine associated with this block
        vp::ClockEngine *clock_engine_instance = NULL;
    };

};
