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
    class TimeEvent;

    typedef void (TimeEventMeth)(vp::Block *, TimeEvent *event);

    #define VP_TIME_EVENT_NB_ARGS 8

    /**
     * @brief Time events
     *
     * Time events can be used to schedule functions execution at specific timestamps in
     * order to model timing.
     */
    class TimeEvent
    {
        friend class BlockTime;
        friend class Block;

    public:

        /**
         * @brief Construct a new event
         *
         * Events must be created associated to a block, so that they can be cancelled when a reset
         * is happening on this block.
         *
         * @note With this constructor, the method must be set later on and before the event is
         * enqueued for the first time.
         *
         * @param comp The block owning this events.
         */
        TimeEvent(vp::Block *top);

        /**
         * @brief Construct a new event
         *
         * Events must be created associated to a block, so that they can be cancelled when a reset
         * is happening on this block.
         *
         * @note With this constructor, the method must be set later on and before the event is
         * enqueued for the first time.
         *
         * @param comp The block owning this events.
         * @param meth The event callback to be called when event gets executed.
         */
        TimeEvent(vp::Block *top, vp::TimeEventMeth *meth);

        /**
         * @brief Destruct an event
         *
         */
        ~TimeEvent();

        /**
         * @brief Set the event callback
         *
         * The goal of time events is to execute functions at specific timestamps in order to
         * model timing.
         * The function to be called when the time engine of the event reaches the timestamp where
         * the event must be executed can be set with this method.
         *
         * @param Event callback to be called when event gets executed
         */
        inline void set_callback(vp::TimeEventMeth *meth) { this->meth = meth; }

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
        inline void **get_args() { return args; }

        /**
         * @brief Tell if the event is enqueued
         *
         * This tells if the event is currently enqueued into the time engine queue.
         *
         * @return True if the event is enqueued
         */
        inline bool is_enqueued() { return enqueued; }

        /**
         * @brief Get event time
         *
         * This returns the timestamp at which the event must be executed
         *
         * @param time Event time
         */
        int64_t get_time() { return time; }

        /**
         * @brief Enqueue the event
         *
         * The event is enqueued into the time engine queue and will be executed when
         * the specified timestamp is reached.
         *
         * @param time Timestamp at which the event must be executed.
         */
        void enqueue(int64_t time);

    private:
        // Can be called to set the event as enqueued or not
        inline void set_enqueued(bool enqueued) { this->enqueued = enqueued; }

        // Event argument
        void *args[VP_TIME_EVENT_NB_ARGS];

        // Block owning the event
        vp::Block *top;

        // Method to be called when the event gets executed
        vp::TimeEventMeth *meth;

        // Pointer to next event in the queue. Used by BlockTime to chain events
        vp::TimeEvent *next;

        // True if the event is currently enqueued into the time engine
        bool enqueued=false;

        // Time where the time event must be executed
        int64_t time;
    };

};