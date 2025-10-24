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
#include <vp/queue.hpp>

vp::Queue::Queue(Block *parent, std::string name, vp::ClockEvent *ready_event)
    : Block(parent, name)
{
    this->ready_event = ready_event;
}

void vp::Queue::cancel_callback(void *__this, vp::QueueElem *elem)
{
    vp::Queue *_this = (vp::Queue *)__this;
    vp::QueueElem *current = _this->first, *prev=NULL;

    while(current && current != elem)
    {
        prev = current;
        current = current->next;
    }

    if (prev)
    {
        prev->next = current->next;
    }
    else
    {
        _this->first = current->next;
    }
}

int vp::Queue::size()
{
    return this->nb_elem;
}

bool vp::Queue::empty()
{
    // Queue is empty if there is no element or if first element is delayed and not yet available
    return this->first == NULL || this->first->timestamp > this->clock.get_cycles();
}

bool vp::Queue::has_reqs()
{
    return this->first != NULL;
}

int vp::Queue::nb_reqs()
{
    return this->nb_elem;
}

void vp::Queue::reset(bool active)
{
    if (active)
    {
        this->first = NULL;
        this->nb_elem = 0;
    }
}

void vp::Queue::push_delayed(QueueElem *elem, int64_t delay)
{
    int64_t timestamp = this->clock.get_cycles() + delay;
    QueueElem *current = this->first, *prev = NULL;

    while (current && current->timestamp < timestamp)
    {
        prev = current;
        current = current->next;
    }

    if (prev)
    {
        prev->next = elem;
    }
    else
    {
        this->first = elem;
    }

    if (!current)
    {
        this->last = elem;
    }

    elem->next = current;

    this->nb_elem++;
    elem->timestamp = timestamp;
    elem->cancel_callback = &vp::Queue::cancel_callback;
    elem->cancel_this = this;

    if (this->nb_elem == 1)
    {
        this->trigger_next();
    }
}

void vp::Queue::push_back(QueueElem *elem, int64_t delay)
{
    if (this->first)
    {
        this->last->next = elem;
    }
    else
    {
        this->first = elem;
    }

    this->last = elem;
    this->nb_elem++;
    elem->next = NULL;
    elem->timestamp = this->clock.get_cycles() + delay + 1;

    elem->cancel_callback = &vp::Queue::cancel_callback;
    elem->cancel_this = this;

    if (this->nb_elem == 1)
    {
        this->trigger_next();
    }
}

void vp::Queue::push_front(QueueElem *elem)
{
    if (!this->first)
    {
        this->last = elem;
    }

    this->nb_elem++;
    elem->next = this->first;
    this->first = elem;

    elem->cancel_callback = &vp::Queue::cancel_callback;
    elem->cancel_this = this;
}

vp::QueueElem *vp::Queue::head()
{
    return this->first;
}

vp::QueueElem *vp::Queue::pop()
{
    vp::QueueElem *result = this->first;
    if (result)
    {
        this->first = result->next;
    }
    this->nb_elem--;
    if (this->nb_elem > 0)
    {
        this->trigger_next();
    }
    return result;
}

void vp::QueueElem::cancel()
{
    this->cancel_callback(this->cancel_this, this);
}

void vp::Queue::trigger_next()
{
    if (this->first && this->ready_event)
    {
        this->ready_event->enqueue(std::max(this->first->timestamp - this->clock.get_cycles(), (int64_t)1));
    }
}
