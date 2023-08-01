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

#include <string>
#include <vp/vp.hpp>


vp::time_scheduler::time_scheduler(js::config *config)
    : time_engine_client(config), first_event(NULL)
{

}


int64_t vp::time_scheduler::exec()
{
    vp::time_event *current = this->first_event;

    while (current && current->time == this->get_time())
    {
        this->first_event = current->next;
        current->set_enqueued(false);

        current->meth(current->_this, current);

        current = this->first_event;
    }

    if (this->first_event == NULL)
    {
        return -1;
    }
    else
    {
        return this->first_event->time - this->get_time();
    }
}


vp::time_event::time_event(time_scheduler *comp, time_event_meth_t *meth)
    : comp(comp), _this((void *)static_cast<vp::component *>((vp::time_scheduler *)(comp))), meth(meth), enqueued(false)
{
    comp->add_event(this);
}


void vp::time_scheduler::reset(bool active)
{
    if (active)
    {
        for (time_event *event: this->events)
        {
            this->cancel(event);
        }
    }
}

void vp::time_scheduler::cancel(vp::time_event *event)
{
    if (!event->is_enqueued())
        return;

    vp::time_event *current = this->first_event, *prev = NULL;

    while (current && current != event)
    {
        prev = current;
        current = current->next;
    }

    if (prev)
    {
        prev->next = event->next;
    }
    else
    {
        this->first_event = event->next;
    }

    event->set_enqueued(false);

    if (this->first_event == NULL)
    {
        this->dequeue_from_engine();
    }
}

void vp::time_scheduler::add_event(time_event *event)
{
    this->events.push_back(event);
}


vp::time_event *vp::time_scheduler::enqueue(time_event *event, int64_t time)
{
    vp::time_event *current = this->first_event, *prev = NULL;
    int64_t full_time = time + this->get_time();

    event->set_enqueued(true);

    while (current && current->time < full_time)
    {
        prev = current;
        current = current->next;
    }

    if (prev)
        prev->next = event;
    else
        this->first_event = event;
    event->next = current;
    event->time = full_time;

    this->enqueue_to_engine(this->get_time() + time);

    return event;
}
