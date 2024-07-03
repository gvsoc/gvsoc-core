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


#include <map>
#include <list>
#include <string>
#include <vector>

#include "vp/vp.hpp"
#include "vp/clock/implementation.hpp"


inline vp::TimeEngine *vp::BlockTime::get_engine()
{
    return this->time_engine;
}

inline void vp::TimeEngine::lock()
{
    // Increase the number of lock request by one, so that the main engine loop leaves the critical loop
    // This needs to be protected as severall thread may try to lock at the same time.
    pthread_mutex_lock(&lock_mutex);

    // The locking mechanism is lock-free so that the time engine can check at each timestamp if there is a lock
    // request with small overhead
    // After it executes a timestampt, the engine will stop if stop_req is true, will then set it to false,
    // and check if there is a lock request.
    // It is important to put a barrier between the accesses to lock_req and stop_req so that the lock-free
    // mechanism wotks well
    this->lock_req++;
    __sync_synchronize();
    this->stop_req = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock_mutex);

    // Now wait until the engine has left the main loop.
    pthread_mutex_lock(&mutex);
}

inline void vp::TimeEngine::unlock()
{
    pthread_mutex_lock(&lock_mutex);
    this->lock_req--;
    pthread_mutex_unlock(&lock_mutex);

    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

inline void vp::TimeEngine::critical_enter()
{
    pthread_mutex_lock(&mutex);
}

inline void vp::TimeEngine::critical_exit()
{
    pthread_mutex_unlock(&mutex);
}

inline void vp::TimeEngine::critical_wait()
{
    pthread_cond_wait(&cond, &mutex);
}

inline void vp::TimeEngine::critical_notify()
{
    pthread_cond_broadcast(&cond);
}

inline void vp::TimeEngine::update(int64_t time)
{
    if (time > this->time)
        this->time = time;
}

inline int64_t vp::TimeEngine::get_next_event_time()
{
    return this->first_client ? this->first_client->time.next_event_time : -1;
}

inline bool vp::BlockTime::enqueue_to_engine(int64_t time)
{
    return this->get_engine()->enqueue(&this->top, time);
}

inline bool vp::BlockTime::dequeue_from_engine()
{
    return this->get_engine()->dequeue(&this->top);
}

inline int64_t vp::BlockTime::get_time()
{
    return this->get_engine()->get_time();
}

inline int vp::TimeEngine::retain_count()
{
    return this->retain;
}