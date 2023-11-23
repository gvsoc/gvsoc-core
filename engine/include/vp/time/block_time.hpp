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

    class BlockTime
    {
        friend class vp::Block;
        friend class vp::TimeEngine;
        friend class vp::ClockEngine;
        friend class vp::TimeEvent;

    public:
        BlockTime(vp::Block *parent, vp::Block &top, vp::TimeEngine *engine);

        inline vp::TimeEngine *get_engine();

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
        inline bool is_running() { return running; }

        void add_event(TimeEvent *event);
        void cancel(TimeEvent *event);
        void enqueue(TimeEvent *event, int64_t time);

        vp::Block &top;
        vp::Block *next;

        bool running = false;
        bool is_enqueued = false;

        vp::TimeEvent *first_event = NULL;
        std::vector<vp::TimeEvent *> events;
        vp::TimeEngine *time_engine = NULL;

        // This gives the time of the next event.
        // It is only valid when the client is not the currently active one,
        // and is then updated either when the client is not the active one
        // anymore or when the client is enqueued to the engine.
        int64_t next_event_time = 0;

    };

};