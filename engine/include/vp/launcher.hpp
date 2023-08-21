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


class Gvsoc_launcher : public gv::Gvsoc
{
public:
    Gvsoc_launcher(gv::GvsocConf *conf);

    void open() override;
    void bind(gv::Gvsoc_user *user) override;
    void close() override;

    void run() override;

    void start() override;

    int64_t stop() override;

    void wait_stopped() override;

    int64_t step(int64_t duration) override;
    int64_t step_until(int64_t timestamp) override;

    int join() override;

    void update(int64_t timestamp);

    gv::Io_binding *io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name) override;
    gv::Wire_binding *wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name) override;

    void vcd_bind(gv::Vcd_user *user) override;
    void event_add(std::string path, bool is_regex) override;
    void event_exclude(std::string path, bool is_regex) override;
    void *get_component(std::string path) override;
    void register_exec_notifier(vp::Notifier *notifier);

    vp::top *top_get() { return this->handler; }

private:
    void engine_routine();
    static void *signal_routine(void *__this);

    gv::GvsocConf *conf;
    vp::top *handler;
    int retval = -1;
    gv::Gvsoc_user *user;
    bool is_async;
    std::thread *engine_thread;
    std::thread *signal_thread;
    std::vector<vp::Notifier *> exec_notifiers;
    vp::time_engine *engine;
    vp::component *instance;
    Gv_proxy *proxy;
    bool running = false;
    bool run_req = false;
};
