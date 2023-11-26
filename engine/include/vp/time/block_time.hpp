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

#include <stdint.h>

namespace vp
{
    class TimeEvent;
    class TimeEngine;

    /**
     * @brief Time features for blocks
     *
     * This class is used within the vp::Block class to bring features for managing time.
     * This allows creating time events for scheduling functions execution at a specific time.
     */
    class BlockTime
    {
        friend class vp::Block;
        friend class vp::TimeEngine;
        friend class vp::ClockEngine;
        friend class vp::TimeEvent;

    public:
        /**
         * @brief Construct a new BlockTime
         *
         * @param parent Specify the parent block containing this block.
         * @param top Specify the parent block class including this time block.
         * @param engine Specify the time engine.
         */
        BlockTime(vp::Block *parent, vp::Block &top, vp::TimeEngine *engine);

        /**
         * @brief Get time engine
         *
         * This returns the global time engine
         *
         * @return The time engine
         */
        inline vp::TimeEngine *get_engine();

        /**
         * @brief Get current time
         *
         * This returns the current absolute time
         *
         * @return The current time
         */
        inline int64_t get_time();

        /**
         * @brief DEPRECATED
        */
        inline bool get_is_enqueued() { return is_enqueued; }

        /**
         * @brief DEPRECATED
        */
        inline int64_t get_next_event_time() { return this->next_event_time; }

        /**
         * @brief DEPRECATED
        */
        inline bool enqueue_to_engine(int64_t time);

        /**
         * @brief DEPRECATED
        */
        inline bool dequeue_from_engine();

    private:
        // True if this block is currently being executed by the time engine
        inline bool is_running() { return running; }

        // Reset this class. Used to cancel all pending time events of this block
        void reset(bool active);

        // Add an event to the block, so that it can be automatically canceled during a reset
        void add_event(TimeEvent *event);

        // Remove an event from the block, when it is destroyed
        void remove_event(TimeEvent *event);

        // Cancel an event. This will remove it from the time engine queue
        void cancel(TimeEvent *event);
        void enqueue(TimeEvent *event, int64_t time);

        // Access to parent class owning this BlockTime
        vp::Block &top;

        // Next block in the queue. Used by time scheduler to queue this block
        vp::Block *next;

        // True if this block is currently being executed by the time engine
        bool running = false;

        // True if the block is currently enqueued into the time engine because at least one time
        // event must be executed
        bool is_enqueued = false;

        // First time event of this block. Once this block will get elected by the time engine, it
        // will execute the first event.
        vp::TimeEvent *first_event = NULL;

        // List of time events owned by this component.
        // This is used to cancel all block time event when it is reset.
        std::vector<vp::TimeEvent *> events;

        // Time engine, which is obtained from the parent.
        vp::TimeEngine *time_engine = NULL;

        // This gives the time of the next event.
        // It is only valid when the client is not the currently active one,
        // and is then updated either when the client is not the active one
        // anymore or when the client is enqueued to the engine.
        int64_t next_event_time = 0;
    };

};