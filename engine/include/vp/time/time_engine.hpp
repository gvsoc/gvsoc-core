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

#include "vp/json.hpp"

namespace vp
{
    class BlockTime;
    class Component;
    class Time_engine_stop_event;

    class TimeEngine
    {

        friend class gv::GvsocLauncher;
        friend class vp::Time_engine_stop_event;
        friend class vp::BlockTime;
        friend class vp::ClockEngine;
        friend class vp::Top;
        friend class gv::GvProxy;
        friend class gv::GvProxySession;

    public:
        TimeEngine(js::Config *config, gv::GvsocLauncher *launcher);

        /**
         * @brief Get current time
         *
         * THis returns the absolute time of the platform.
         *
         * @return The current time
         */
        int64_t get_time() { return time; }

        /**
         * @brief Lock the engine
         *
         * This will block the current thread until the time engine has stopped, so that
         * the current thread can interact with the models.
         */
        inline void lock();

        /**
         * @brief Unlock the engine
         *
         * This will unblock the time engine which can then resume event execution.
         */
        inline void unlock();

        /**
         * @brief Quit simulation
         *
         * Make the time engine quit the simulation with the specified exit status.
         *
         * @param the exit status
         */
        void quit(int status);

        /**
         * @brief Pause simulation
         *
         * This makes the time engine pause the simulation. It will not execute events until
         * someone runs the simulation again.
         */
        void pause();


        /**
         * @brief DEPRECATED
         * I2s_verif still using it, should switch to time_event
        */
        bool dequeue(vp::Block *client);

        /**
         * @brief DEPRECATED
         * dpi wrapper still using to sync time before propagating an edge.
         * needs to be friend class.
        */
        inline void update(int64_t time);

    private:
        void step_register(int64_t time);

        int64_t run(bool main_controller);
        int64_t run_until(int64_t end_time, bool main_controller);

        void init(Component *top);

        void flush();

        void fatal(const char *fmt, ...);

        bool enqueue(vp::Block *client, int64_t time);

        bool handle_locks();

        inline int64_t get_next_event_time();

        void bind_to_launcher(gv::Gvsoc_user *launcher);

        int status_get() { return this->stop_status; }
        bool finished_get() { return this->finished; }

        gv::Gvsoc_user *launcher_get() { return this->launcher; }

        int64_t exec();
        void flush_all();

        inline void critical_enter();
        inline void critical_exit();
        inline void critical_wait();
        inline void critical_notify();

        // List of clients having time events to execute.
        // They are sorted out according to the timestamp of their first event,
        // from lowest to highest
        vp::Block *first_client = NULL;

        // Mutex used to update the count of pending lock requests
        pthread_mutex_t lock_mutex;

        // Mutex used to lock the engine. It is taken by the engine when it is executing events and
        // released when it is stopped.
        // Other threads can then try to lock it to wait until the engine is released.
        pthread_mutex_t mutex;

        // Condition used to wait for some event to happen, like waiting that there is no
        // lock request
        pthread_cond_t cond;

        // Current time of the engine. This gives the absolute time of the platform
        int64_t time = 0;

        // Top component of the system
        Component *top;

        // True to make the engine goes out of his main fast loop. This is a way to make it go to
        // his slow loop so that it checks if there is a lock or a pause request
        bool stop_req = false;

        // Set to true when the engine should pause the simulation. A stop request should also be
        // posted to make the engine goes out of the fast loop.
        bool pause_req = false;

        // True if someone has requested to lock the engine. Locking the engine means that
        // an external thread wants to interact with the models.
        // A stop request must also be posted.
        int lock_req = 0;

        // True if the engine should quit. A stop request must also be posted.
        bool finished = false;

        // This gives teh exit status when engine has finished
        int stop_status = -1;

        // Pointer to the top launcher
        gv::Gvsoc_user *launcher = NULL;

        gv::GvsocLauncher *gv_launcher;

        // The stop event is used in step mode to register a time event which will pause
        // simulation at a specific timestamp corresponding to the step
        Time_engine_stop_event *stop_event;
    };
};
