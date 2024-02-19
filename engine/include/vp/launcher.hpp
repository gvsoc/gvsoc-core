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

#include <vp/vp.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

class GvsocLauncher_notifier
{
public:
    virtual void notify_stop(int64_t time) {}
    virtual void notify_run(int64_t time) {}
};


namespace gv {

class GvsocLauncher : public gv::Gvsoc
{
    public:
        GvsocLauncher(gv::GvsocConf *conf);

        void open() override;
        void bind(gv::Gvsoc_user *user) override;
        void close() override;

        void run() override;

        void start() override;

        void flush() override;

        int64_t stop() override;

        void wait_stopped() override;

        double get_instant_power(double &dynamic_power, double &static_power) override;
        double get_average_power(double &dynamic_power, double &static_power) override;
        void report_start() override;
        void report_stop() override;
        gv::PowerReport *report_get() override;

        int64_t step(int64_t duration) override;
        int64_t step_until(int64_t timestamp) override;

        int join() override;

        void retain() override;

        int retain_count() override;

        void release() override;

        void update(int64_t timestamp);

        gv::Io_binding *io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name) override;
        gv::Wire_binding *wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name) override;

        void vcd_bind(gv::Vcd_user *user) override;
        void vcd_enable() override;
        void vcd_disable() override;
        void event_add(std::string path, bool is_regex) override;
        void event_exclude(std::string path, bool is_regex) override;
        void *get_component(std::string path) override;
        void register_exec_notifier(GvsocLauncher_notifier *notifier);

        vp::Top *top_get() { return this->handler; }

        bool get_is_async() { return this->is_async; }

    private:
        void engine_routine();
        static void *signal_routine(void *__this);

        gv::GvsocConf *conf;
        vp::Top *handler;
        int retval = -1;
        gv::Gvsoc_user *user;
        bool is_async;
        std::thread *engine_thread;
        std::thread *signal_thread;
        std::vector<GvsocLauncher_notifier *> exec_notifiers;
        vp::Component *instance;
        GvProxy *proxy;
        bool running = false;
        bool run_req = false;
    };

};