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

    typedef void (TimeEventMeth)(void *, TimeEvent *event);

    #define TIME_EVENT_PAYLOAD_SIZE 64
    #define TIME_EVENT_NB_ARGS 8

    class TimeEngine;

    class BlockTime
    {

        friend class TimeEngine;

    public:
        BlockTime(vp::Block *parent, vp::Block &top, vp::TimeEngine *engine);

        void time_event_init(vp::TimeEvent *, vp::TimeEventMeth *meth);

        inline vp::TimeEngine *get_engine();

        inline bool is_running() { return running; }

        inline bool get_is_enqueued() { return is_enqueued; }

        inline bool enqueue_to_engine(int64_t time);

        inline bool dequeue_from_engine();

        inline int64_t get_time();

        void add_event(TimeEvent *event);
        void cancel(TimeEvent *event);
        void enqueue(TimeEvent *event, int64_t time);

        vp::Block &top;
        vp::Block *next;

        // This gives the time of the next event.
        // It is only valid when the client is not the currently active one,
        // and is then updated either when the client is not the active one
        // anymore or when the client is enqueued to the engine.
        int64_t next_event_time = 0;

        bool running = false;
        bool is_enqueued = false;

        vp::TimeEvent *first_event = NULL;
        std::vector<vp::TimeEvent *> events;
        vp::TimeEngine *time_engine = NULL;
    };

    class TimeEvent
    {
        friend class BlockTime;
        friend class Block;

    public:

        TimeEvent(vp::Block *top);

        inline void set_callback(TimeEventMeth *meth) { this->meth = meth; }

        inline int get_payload_size() { return TIME_EVENT_PAYLOAD_SIZE; }
        inline uint8_t *get_payload() { return payload; }

        inline int get_nb_args() { return TIME_EVENT_NB_ARGS; }
        inline void **get_args() { return args; }

        inline bool is_enqueued() { return enqueued; }
        inline void set_enqueued(bool enqueued) { this->enqueued = enqueued; }

        int64_t get_time() { return time; }

        void enqueue(int64_t time);

    private:
        uint8_t payload[TIME_EVENT_PAYLOAD_SIZE];
        void *args[TIME_EVENT_NB_ARGS];
        vp::Block *top;
        TimeEventMeth *meth;
        TimeEvent *next;
        bool enqueued=false;
        int64_t time;
    };

};