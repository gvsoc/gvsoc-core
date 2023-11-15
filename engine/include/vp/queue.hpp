/*
 * Copyright (C) 2020  GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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

#include <string.h>

namespace vp {

    class QueueElem;
    class Block;

    class queue : public Block
    {
    public:
        queue(Block *parent, std::string name);
        void push_back(QueueElem *elem);
        void push_front(QueueElem *elem);
        QueueElem *head();
        QueueElem *pop();
        bool empty();
        void reset(bool active);
    private:
        static void cancel_callback(void *__this, vp::QueueElem *elem);
        QueueElem *first=NULL;
        QueueElem *last;
    };

    class QueueElem
    {
        friend class queue;

    public:
        void cancel();

    protected:
        QueueElem *next;
        void *cancel_this;
        void (*cancel_callback)(void *, vp::QueueElem *);
    };
};
