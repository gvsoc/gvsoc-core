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

#ifndef __VP_TIME_ENGINE_HPP__
#define __VP_TIME_ENGINE_HPP__

#include "vp/vp_data.hpp"
#include "vp/component.hpp"
#include "json.hpp"
#include "gv/gvsoc.hpp"

namespace vp
{

class time_engine_client;
class component;

class time_engine
{
public:
    time_engine(component *top, js::config *config);

    int64_t run_until(int64_t time);

    int64_t run();

    inline void lock();

    inline void unlock();

    void quit(int status);

    void wait_for_lock();
    void wait_for_lock_stop();

    void pause();

    void flush();

    bool dequeue(time_engine_client *client);

    bool enqueue(time_engine_client *client, int64_t time);

    int64_t get_time() { return time; }

    void fatal(const char *fmt, ...);

    inline void update(int64_t time);

    void bind_to_launcher(gv::Gvsoc_user *launcher);

    int status_get() { return this->stop_status; }
    bool finished_get() { return this->finished; }
    gv::Gvsoc_user *launcher_get() { return this->launcher; }

private:

    int64_t exec(int64_t end_time);
    void flush_all() {this->top->flush_all(); }


    time_engine_client *first_client = NULL;
    bool locked = false;


    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int64_t time = 0;
    int stop_status = -1;

    component *top;
    js::config *config;
    bool finished = false;
    bool lock_req = false;
    bool stop_req = false;
    bool pause_req = false;
    gv::Gvsoc_user *launcher = NULL;
};

class time_engine_client : public component
{

    friend class time_engine;

public:
    time_engine_client(js::config *config)
        : component(config)
    {
    }

    inline bool is_running() { return running; }

    inline bool enqueue_to_engine(int64_t time)
    {
        return engine->enqueue(this, time);
    }

    inline bool dequeue_from_engine()
    {
        return engine->dequeue(this);
    }

    inline int64_t get_time() { return engine->get_time(); }

    virtual int64_t exec() = 0;

protected:
    time_engine_client *next;

    // This gives the time of the next event.
    // It is only valid when the client is not the currently active one,
    // and is then updated either when the client is not the active one
    // anymore or when the client is enqueued to the engine.
    int64_t next_event_time = 0;

    time_engine *engine;
    bool running = false;
    bool is_enqueued = false;
};

}; // namespace vp


inline void vp::time_engine::lock()
{
    pthread_mutex_lock(&mutex);

    while(this->lock_req)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    this->stop_req = true;
    this->lock_req = true;

    pthread_cond_broadcast(&cond);

    while(!this->locked)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    pthread_mutex_unlock(&mutex);
}

inline void vp::time_engine::unlock()
{
    pthread_mutex_lock(&mutex);
    this->locked = false;
    this->lock_req = false;
    this->stop_req = false;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

inline void vp::time_engine::update(int64_t time)
{
    if (time > this->time)
        this->time = time;
}

#endif
