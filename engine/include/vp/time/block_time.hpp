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
    class time_event;

    typedef void (time_event_meth_t)(void *, time_event *event);

    #define TIME_EVENT_PAYLOAD_SIZE 64
    #define TIME_EVENT_NB_ARGS 8

    class TimeEngine;

    class BlockTime
    {

        friend class TimeEngine;

    public:
        BlockTime(vp::Block &top);

        void time_event_init(vp::time_event *, vp::time_event_meth_t *meth);

        inline vp::TimeEngine *get_engine();

        inline bool is_running() { return running; }

        inline bool get_is_enqueued() { return is_enqueued; }

        inline bool enqueue_to_engine(int64_t time);

        inline bool dequeue_from_engine();

        inline int64_t get_time();

        void add_event(time_event *event);
        void cancel(time_event *event);
        void enqueue(time_event *event, int64_t time);

        vp::Block &top;
        vp::Block *next;

        // This gives the time of the next event.
        // It is only valid when the client is not the currently active one,
        // and is then updated either when the client is not the active one
        // anymore or when the client is enqueued to the engine.
        int64_t next_event_time = 0;

        bool running = false;
        bool is_enqueued = false;

        vp::time_event *first_event = NULL;
        std::vector<vp::time_event *> events;
    };

    class time_event
    {
        friend class BlockTime;
        friend class Block;

    public:

        time_event(vp::Block *top);

        inline void set_callback(time_event_meth_t *meth) { this->meth = meth; }

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
        time_event_meth_t *meth;
        time_event *next;
        bool enqueued=false;
        int64_t time;
    }; 

};