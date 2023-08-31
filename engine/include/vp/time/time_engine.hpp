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
class Time_engine_stop_event;

class time_engine
{
public:
    time_engine(component *top, js::config *config);

    void step_register(int64_t time);

    int64_t run();
    int64_t run_until(int64_t end_time);

    inline void lock();

    inline void unlock();

    inline void critical_enter();
    inline void critical_exit();
    inline void critical_wait();
    inline void critical_notify();

    void quit(int status);

    void handle_locks();

    void pause();

    void flush();

    bool dequeue(time_engine_client *client);

    bool enqueue(time_engine_client *client, int64_t time);

    int64_t get_time() { return time; }

    inline int64_t get_next_event_time();

    void fatal(const char *fmt, ...);

    inline void update(int64_t time);

    void bind_to_launcher(gv::Gvsoc_user *launcher);

    int status_get() { return this->stop_status; }
    bool finished_get() { return this->finished; }
    gv::Gvsoc_user *launcher_get() { return this->launcher; }

    void retain_inc(int inc);
    int retain_count() { return this->retain; }

private:

    int64_t exec();
    void flush_all() {this->top->flush_all(); }


    time_engine_client *first_client = NULL;


    pthread_mutex_t lock_mutex;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int64_t time = 0;
    int stop_status = -1;

    component *top;
    js::config *config;
    bool finished = false;
    int lock_req = 0;
    bool stop_req = false;
    bool pause_req = false;
    gv::Gvsoc_user *launcher = NULL;
    Time_engine_stop_event *stop_event;
    int retain = 0;
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


#include "vp/time/time_scheduler.hpp"

namespace vp
{

    class Time_engine_stop_event : public time_scheduler
    {
    public:
        Time_engine_stop_event(component *top, time_engine *engine);
        int64_t step(int64_t duration);
        vp::time_event *step_nofree(int64_t duration);

    private:
        static void event_handler(void *__this, vp::time_event *event);
        static void event_handler_nofree(void *__this, vp::time_event *event);
        component *top;
    };
}

inline void vp::time_engine::lock()
{
    // Increase the number of lock request by one, so that the main engine loop leaves the critical loop
    // This needs to be protected as severall thread may try to lock at the same time.
    pthread_mutex_lock(&lock_mutex);
    this->lock_req++;
    this->stop_req = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock_mutex);

    // Now wait until the engine has left the main loop.
    pthread_mutex_lock(&mutex);
}

inline void vp::time_engine::unlock()
{
    pthread_mutex_lock(&lock_mutex);
    this->lock_req--;
    pthread_mutex_unlock(&lock_mutex);

    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

inline void vp::time_engine::critical_enter()
{
    pthread_mutex_lock(&mutex);
}

inline void vp::time_engine::critical_exit()
{
    pthread_mutex_unlock(&mutex);
}

inline void vp::time_engine::critical_wait()
{
    pthread_cond_wait(&cond, &mutex);
}

inline void vp::time_engine::critical_notify()
{
    pthread_cond_broadcast(&cond);
}

inline void vp::time_engine::update(int64_t time)
{
    if (time > this->time)
        this->time = time;
}

inline int64_t vp::time_engine::get_next_event_time()
{
    return this->first_client ? this->first_client->next_event_time : -1;
}

#endif
