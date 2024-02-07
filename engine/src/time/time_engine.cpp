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
#include "vp/time/time_engine.hpp"
#include <vp/time/time_event.hpp>


namespace vp
{
    class Time_engine_stop_event : public vp::Block
    {
    public:
        Time_engine_stop_event(Component *top);
        int64_t step(int64_t duration);
        vp::TimeEvent *step_nofree(int64_t duration);

    private:
        static void event_handler(vp::Block *__this, vp::TimeEvent *event);
        static void event_handler_nofree(vp::Block *__this, vp::TimeEvent *event);
        Component *top;
    };
}

vp::TimeEngine::TimeEngine(js::Config *config)
    : first_client(NULL)
{
    pthread_mutex_init(&lock_mutex, NULL);
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
}

void vp::TimeEngine::init(vp::Component *top)
{
    this->top = top;
    this->stop_event = new vp::Time_engine_stop_event(this->top);
}

int64_t vp::TimeEngine::exec()
{
    vp::Block *current = this->first_client;

    if (current)
    {
        first_client = current->time.next;
        current->time.is_enqueued = false;

        // Update the global engine time with the current event time
        this->time = current->time.next_event_time;

        while (1)
        {
            current->time.running = true;

            int64_t time = current->exec();

            vp::Block *next = this->first_client;

            // Shortcut to quickly continue with the same client
            if (likely(time > 0))
            {
                time += this->time;
                if (likely((!next || next->time.next_event_time > time)))
                {
                    if (likely(!this->stop_req))
                    {
                        this->time = time;
                        continue;
                    }
                    else
                    {
                        current->time.next = first_client;
                        first_client = current;
                        current->time.next_event_time = time;
                        current->time.is_enqueued = true;
                        current->time.running = false;
                        break;
                    }
                }
            }

            // Otherwise remove it, reenqueue it and continue with the next one.
            // We can optimize a bit the operation as we already know
            // who to schedule next.

            if (time > 0)
            {
                current->time.next_event_time = time;
                vp::Block *client = next->time.next, *prev = next;
                while (client && client->time.next_event_time < time)
                {
                    prev = client;
                    client = client->time.next;
                }
                current->time.next = client;
                prev->time.next = current;
                current->time.is_enqueued = true;
            }

            current->time.running = false;

            current = first_client;

            // Leave the loop either if there is no more client to schedule or if there is a stop request.
            // In case of a stop request, always take it into account when time is increased so that teh engine
            // is stopped at the end of the current timestamp. This will ensure the step operation, which is using a
            // stop event, is stepping until the end of the timestamp.
            if (!current || (this->stop_req && current->time.next_event_time > this->time))
            {
                break;
            }

            vp_assert(first_client->time.next_event_time >= get_time(), NULL, "event time is before vp time\n");

            first_client = current->time.next;
            current->time.is_enqueued = false;

            // Update the global engine time with the current event time
            this->time = current->time.next_event_time;
        }
    }

    int64_t time = -1;

    if (this->first_client)
    {
        time = this->first_client->time.next_event_time;
    }

    return time;
}

void vp::TimeEngine::handle_locks()
{
    while (this->lock_req > 0)
    {
        pthread_cond_wait(&cond, &mutex);
    }
}

void vp::TimeEngine::step_register(int64_t end_time)
{
    this->stop_event->step(end_time);
}

int64_t vp::TimeEngine::run_until(int64_t end_time)
{
    int64_t time;
    vp::TimeEvent *event = this->stop_event->step_nofree(end_time);
    // In synchronous mode, since several threads can control the time, there is a retain
    // mechanism which makes sure time is progressins only if all threads ask for it.
    // Since wwe are now ready to make time progress, decrease our counter, it will be increased
    // again when our stop event is executed, so that time does not progress any further until
    // we return
    this->retain--;

    while (1)
    {
        // If anyone is retaining the engine, it means it did not ask for time progress, thus we
        // need to wait.
        // In this case someone else will make the time progress when engine is released.
        while (this->retain)
        {
            pthread_cond_wait(&this->cond, &this->mutex);

            // Someone made the time progress, check if we can leave.
            // This is the case once our event is not enqueued anymore
            if (!event->is_enqueued())
            {
                return this->get_next_event_time();
            }
        }

        time = this->exec();

        // Cancel now the requests that may have stopped us so that anyone can stop us again
        // when locks are handled.
        this->stop_req = false;

        // We may has been stopped because the execution is stopped or finished
        if (this->finished)
        {
            gv::Gvsoc_user *launcher = this->launcher_get();

            if (launcher)
            {
                if (this->finished)
                {
                    launcher->has_ended();
                }
            }
        }

        // Checks locks since we may have been stopped by them
        this->handle_locks();

        // Leave only once our event is over
        if (!event->is_enqueued())
        {
            break;
        }
    }

    return time;
}

int64_t vp::TimeEngine::run()
{
    int64_t time;

    while (1)
    {
        // Cancel any pause request which was done before running
        this->pause_req = false;

        time = this->exec();

        // Cancel now the requests that may have stopped us so that anyone can stop us again
        // when locks are handled.
        this->stop_req = false;

        // In case there is no more event, stall the engine until something happens.
        if (time == -1)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        // Checks locks since we may have been stopped by them
        this->handle_locks();

        // In case of a pause request or the simulation is finished, we leave the engine to let
        // the launcher handles it, otherwise we just continue to run events
        if (this->pause_req || this->finished)
        {
            gv::Gvsoc_user *launcher = this->launcher_get();

            this->pause_req = false;
            time = -1;
            if (launcher)
            {
                if (this->finished)
                {
                    launcher->has_ended();
                }
                else
                {
                    launcher->has_stopped();
                }
            }
            break;
        }
    }

    return time;
}

void vp::TimeEngine::bind_to_launcher(gv::Gvsoc_user *launcher)
{
    this->launcher = launcher;
}

bool vp::TimeEngine::dequeue(vp::Block *client)
{
    if (!client->time.is_enqueued)
        return false;

    client->time.is_enqueued = false;

    vp::Block *current = this->first_client, *prev = NULL;
    while (current && current != client)
    {
        prev = current;
        current = current->time.next;
    }
    if (prev)
        prev->time.next = client->time.next;
    else
        this->first_client = client->time.next;

    return true;
}

bool vp::TimeEngine::enqueue(vp::Block *client, int64_t full_time)
{
    bool update = false;

    vp_assert(full_time >= get_time(), NULL, "Time must be higher than current time\n");

    if (client->time.is_running())
        return false;

    if (client->time.is_enqueued)
    {
        if (client->time.next_event_time <= full_time)
            return false;
        this->dequeue(client);
    }

    client->time.is_enqueued = true;

    vp::Block *current = first_client, *prev = NULL;
    client->time.next_event_time = full_time;
    while (current && current->time.next_event_time < client->time.next_event_time)
    {
        prev = current;
        current = current->time.next;
    }
    if (prev)
        prev->time.next = client;
    else
    {
        update = true;
        first_client = client;
    }
    client->time.next = current;

    if (update && this->launcher)
    {
        this->launcher->was_updated();
    }

    return true;
}

void vp::TimeEngine::quit(int status)
{
    this->pause();
    this->stop_status = status;
    this->finished = true;

    // Notify the condition in case we are waiting for locks, to allow leaving the engine.
    pthread_cond_broadcast(&cond);
}

void vp::TimeEngine::pause()
{
    this->stop_req = true;
    this->pause_req = true;

    // Notify the condition in case we are waiting for locks, to allow leaving the engine.
    pthread_cond_broadcast(&cond);
}

// TODO shoud be moved to Top class
void vp::TimeEngine::flush()
{
    this->top->flush_all();
    fflush(NULL);
}

void vp::TimeEngine::fatal(const char *fmt, ...)
{
    fprintf(stdout, "[\033[31mFATAL\033[0m] ");
    va_list ap;
    va_start(ap, fmt);
    if (vfprintf(stdout, fmt, ap) < 0)
    {
    }
    va_end(ap);
    this->quit(-1);
}

vp::Time_engine_stop_event::Time_engine_stop_event(vp::Component *top)
    : vp::Block(top, "stop_event"), top(top)
{
}

int64_t vp::Time_engine_stop_event::step(int64_t time)
{
    vp::TimeEvent *event = new vp::TimeEvent(this);
    event->set_callback(this->event_handler);
    event->enqueue(time - top->time.get_engine()->get_time());
    return 0;
}

vp::TimeEvent *vp::Time_engine_stop_event::step_nofree(int64_t time)
{
    vp::TimeEvent *event = new vp::TimeEvent(this);
    event->set_callback(this->event_handler_nofree);
    event->enqueue(time - top->time.get_engine()->get_time());
    return event;
}

void vp::Time_engine_stop_event::event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    Time_engine_stop_event *_this = (Time_engine_stop_event *)__this;
    _this->top->time.get_engine()->pause();
    _this->top->time.get_engine()->retain_inc(1);
}

void vp::Time_engine_stop_event::event_handler_nofree(vp::Block *__this, vp::TimeEvent *event)
{
    Time_engine_stop_event *_this = (Time_engine_stop_event *)__this;
    _this->top->time.get_engine()->pause();

    // Increase the retain count to stop time progress.
    _this->top->time.get_engine()->retain_inc(1);
}

void vp::TimeEngine::retain_inc(int inc)
{
    this->retain += inc;
}


void vp::TimeEngine::flush_all()
{
    this->top->flush_all();
}