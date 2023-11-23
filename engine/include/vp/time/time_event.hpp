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

    class TimeEvent
    {
        friend class BlockTime;
        friend class Block;

    public:

        TimeEvent(vp::Block *top);

        TimeEvent(vp::Block *top, vp::TimeEventMeth *meth);

        inline void set_callback(vp::TimeEventMeth *meth) { this->meth = meth; }

        inline void **get_args() { return args; }

        inline bool is_enqueued() { return enqueued; }

        int64_t get_time() { return time; }

        void enqueue(int64_t time);

    private:
        inline void set_enqueued(bool enqueued) { this->enqueued = enqueued; }
        void *args[VP_TIME_EVENT_NB_ARGS];
        vp::Block *top;
        vp::TimeEventMeth *meth;
        vp::TimeEvent *next;
        bool enqueued=false;
        int64_t time;
    };

};