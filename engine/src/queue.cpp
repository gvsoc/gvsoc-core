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

vp::queue::queue(Block *parent, std::string name)
    : Block(parent, name)
{

}

void vp::queue::cancel_callback(void *__this, vp::QueueElem *elem)
{
    vp::queue *_this = (vp::queue *)__this;
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

bool vp::queue::empty()
{
    return this->first == NULL;
}

void vp::queue::reset(bool active)
{
    this->first = NULL;
}

void vp::queue::push_back(QueueElem *elem)
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
    elem->next = NULL;

    elem->cancel_callback = &vp::queue::cancel_callback;
    elem->cancel_this = this;
}

void vp::queue::push_front(QueueElem *elem)
{
    if (!this->first)
    {
        this->last = elem;
    }

    elem->next = this->first;
    this->first = elem;

    elem->cancel_callback = &vp::queue::cancel_callback;
    elem->cancel_this = this;
}

vp::QueueElem *vp::queue::head()
{
    return this->first;
}

vp::QueueElem *vp::queue::pop()
{
    vp::QueueElem *result = this->first;
    if (result)
    {
        this->first = result->next;
    }
    return result;
}

void vp::QueueElem::cancel()
{
    this->cancel_callback(this->cancel_this, this);
}
