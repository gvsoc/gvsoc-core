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

#ifndef __VP_CLOCK_IMPLEMENTATION_HPP__
#define __VP_CLOCK_IMPLEMENTATION_HPP__

#include "vp/component.hpp"
#include "vp/itf/clk.hpp"
#include "vp/itf/wire.hpp"
#include "vp/clock/clock_event.hpp"
#include "vp/clock/block_clock.hpp"
#include "vp/clock/clock_engine.hpp"

using namespace std;

int64_t vp::BlockClock::get_period() { return get_engine()->get_period(); }

int64_t vp::BlockClock::get_frequency() { return get_engine()->get_frequency(); }

inline int64_t vp::BlockClock::get_cycles() { return get_engine()->get_cycles(); }


inline void vp::Block::event_enqueue(vp::ClockEvent *event, int64_t cycles)
{
  clock.get_engine()->enqueue(event, cycles);
}

inline void vp::Block::event_enqueue_ext(vp::ClockEvent *event, int64_t cycles)
{
  clock.get_engine()->enqueue_ext(event, cycles);
}

inline void vp::Block::event_cancel(vp::ClockEvent *event)
{
  event->clock->cancel(event);
}

inline void vp::Block::event_reenqueue(vp::ClockEvent *event, int64_t cycles)
{
  clock.get_engine()->reenqueue(event, cycles);
}

inline void vp::Block::event_reenqueue_ext(vp::ClockEvent *event, int64_t cycles)
{
  clock.get_engine()->reenqueue_ext(event, cycles);
}

inline vp::ClockEvent *vp::Block::event_new(vp::ClockEventMeth *meth)
{
    ClockEvent *event = new vp::ClockEvent(this);
    event->set_callback(meth);
    return event;
}

inline vp::ClockEvent *vp::Block::event_new(vp::Block *_this, vp::ClockEventMeth *meth)
{
    ClockEvent *event = new vp::ClockEvent(this);
    event->_this = (vp::Block *)_this;
    event->set_callback(meth);
    return event;
}

inline void vp::Block::event_del(vp::ClockEvent *event)
{
  clock.get_engine()->event_del(this, event);
}

inline vp::ClockEngine *vp::BlockClock::get_engine()
{
  return clock_engine_instance;
}

inline void vp::ClockEngine::sync()
{
  if (!time.is_running() && this->permanent_first == NULL)
  {
    this->update();
  }
}

inline int64_t vp::ClockEvent::get_cycle()
{
  return this->cycle == -1 ? this->clock->get_cycles() + 1: cycle;
}

inline void vp::ClockEvent::cancel()
{
    if (this->is_enqueued())
    {
      this->clock->cancel(this);
    }
}


inline void vp::ClockEvent::enqueue(int64_t cycles)
{
    this->clock->enqueue(this, cycles);
}


inline void vp::ClockEvent::enable()
{
    this->clock->enable(this);
}

inline void vp::ClockEvent::disable()
{
    this->clock->disable(this);
}

inline void vp::ClockEvent::stall_cycle_inc(int64_t inc)
{
    this->stall_cycle += inc;

    if (this->meth_saved == NULL && this->stall_cycle > 0)
    {
        this->meth_saved = this->meth;
        this->meth = &vp::ClockEngine::stalled_event_handler;
    }
}

inline void vp::ClockEvent::stall_cycle_set(int64_t value)
{
    if (this->meth_saved == NULL)
    {
        if (value != 0)
        {
            this->meth_saved = this->meth;
            this->meth = &vp::ClockEngine::stalled_event_handler;
        }
    }
    else
    {
        if (value == 0)
        {
            this->meth = this->meth_saved;
            this->meth_saved = NULL;
        }
    }
    this->stall_cycle = value;
}

inline int64_t vp::ClockEvent::stall_cycle_get()
{
    return this->stall_cycle;
}

inline bool vp::ClockEvent::is_enqueued()
{
    return enqueued;
}

inline void vp::ClockEvent::set_callback(ClockEventMeth *meth)
{
    if (this->meth_saved == NULL)
    {
        this->meth = meth;
    }
    else
    {
        this->meth_saved = meth;
    }
}

inline void **vp::ClockEvent::get_args()
{
    return args;
}

inline void vp::ClockEvent::exec()
{
    this->meth(this->_this, this);
}

inline void vp::ClockEvent::set_clock(ClockEngine *clock)
{
    this->clock = clock;
}

#endif
