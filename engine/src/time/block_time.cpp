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

#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/time/block_time.hpp>
#include <vp/time/time_event.hpp>


vp::BlockTime::BlockTime(vp::Block *parent, vp::Block &top, vp::TimeEngine *engine) : top(top), time_engine(engine)
{
    if (this->time_engine == NULL && parent != NULL)
    {
        this->time_engine = parent->time.get_engine();
    }
}

int64_t vp::Block::exec()
{
    vp::TimeEvent *current = this->time.first_event;

    while (current && current->time == this->time.get_time())
    {
        this->time.first_event = current->next;
        current->set_enqueued(false);

        current->meth(current->top, current);

        current = this->time.first_event;
    }

    if (this->time.first_event == NULL)
    {
        return -1;
    }
    else
    {
        return this->time.first_event->time - this->time.get_time();
    }
}


void vp::BlockTime::cancel(vp::TimeEvent *event)
{
    if (!event->is_enqueued())
        return;

    vp::TimeEvent *current = this->first_event, *prev = NULL;

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

void vp::BlockTime::add_event(TimeEvent *event)
{
    this->events.push_back(event);
}

void vp::BlockTime::remove_event(TimeEvent *event)
{
    this->events.erase(std::remove(this->events.begin(), this->events.end(), event),
        this->events.end());
}


void vp::BlockTime::enqueue(TimeEvent *event, int64_t time)
{
    vp::TimeEvent *current = this->first_event, *prev = NULL;
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
}


void vp::BlockTime::reset(bool active)
{
    if (active)
    {
        for (vp::TimeEvent *event: this->events)
        {
            this->cancel(event);
        }
    }
}