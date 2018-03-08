/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 */

#ifndef __VP_CLOCK_ENGINE_HPP__
#define __VP_CLOCK_ENGINE_HPP__

#include "vp/vp.hpp"
#include "vp/component.hpp"
#include "vp/time/time_engine.hpp"

namespace vp {

  class clock_event;
  class component;

  class clock_engine : public time_engine_client
  {

  public:

    clock_engine(const char *config);

    inline clock_event *enqueue(clock_event *event, int64_t cycles)
    {
      event->enqueued = true;

      if (likely(is_running() && cycles < CLOCK_EVENT_QUEUE_SIZE))
      {
        enqueue_to_cycle(event, cycles);
      }
      else
      {
        enqueue_other(event, cycles);
      }
      return event;
    }

    clock_event *event_new(component_clock *comp, clock_event_meth_t *meth)
    {
      clock_event *event = new clock_event(comp, meth);
      return event;
    }

    void event_del(component_clock *comp, clock_event *event)
    {
      delete event;
    }

    int64_t exec();

    void set_time_engine(vp::time_engine *engine) { this->engine = engine; }

    inline int64_t get_cycles() { return cycles; }

    inline void stop(int status) { engine->stop(status); }

  protected:

    inline void enqueue_to_cycle(clock_event *event, int64_t cycles)
    {
      int cycle = (current_cycle + cycles) & CLOCK_EVENT_QUEUE_MASK;
      event->next = event_queue[cycle];
      event_queue[cycle] = event;
      nb_enqueued_to_cycle++;
    }

    clock_event *enqueue_other(clock_event *event, int64_t cycles);

    clock_event *event_queue[CLOCK_EVENT_QUEUE_SIZE];
    clock_event *delayed_queue = NULL;
    int current_cycle = 0;
    int64_t period = 0;
    int64_t freq;
    int64_t cycles = 0;
    int nb_enqueued_to_cycle = 0;

  };    

};

#endif