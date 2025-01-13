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


namespace gv {

class GvsocLauncherClient;

class Logger
{
public:
    Logger(std::string module);
    inline void info(const char *fmt, ...);

private:
    std::string module;
};

class GvsocLauncher
{
    public:
        GvsocLauncher(gv::GvsocConf *conf);

        void sim_finished(int status);

        void register_client(GvsocLauncherClient *client);
        void unregister_client(GvsocLauncherClient *client);
        void open(GvsocLauncherClient *client);
        void bind(gv::Gvsoc_user *user, GvsocLauncherClient *client);
        void close(GvsocLauncherClient *client);

        void run_async(GvsocLauncherClient *client);
        int64_t run_sync(GvsocLauncherClient *client);

        void start(GvsocLauncherClient *client);

        void flush(GvsocLauncherClient *client);

        int64_t stop(GvsocLauncherClient *client);

        void wait_stopped(GvsocLauncherClient *client);

        double get_instant_power(double &dynamic_power, double &static_power, GvsocLauncherClient *client);
        double get_average_power(double &dynamic_power, double &static_power, GvsocLauncherClient *client);
        void report_start(GvsocLauncherClient *client);
        void report_stop(GvsocLauncherClient *client);
        gv::PowerReport *report_get(GvsocLauncherClient *client);


        int64_t step_sync(int64_t duration, GvsocLauncherClient *client);
        int64_t step_async(int64_t duration, GvsocLauncherClient *client);
        int64_t step_until_sync(int64_t timestamp, GvsocLauncherClient *client);
        int64_t step_until_async(int64_t timestamp, GvsocLauncherClient *client);
        int64_t step_and_wait_sync(int64_t duration, GvsocLauncherClient *client);
        int64_t step_and_wait_async(int64_t duration, GvsocLauncherClient *client);
        int64_t step_until_and_wait_sync(int64_t timestamp, GvsocLauncherClient *client);
        int64_t step_until_and_wait_async(int64_t timestamp, GvsocLauncherClient *client);

        int join(GvsocLauncherClient *client);

        void lock();
        void unlock();

        void engine_lock();
        void engine_unlock();

        void client_quit(gv::GvsocLauncherClient *client);

        void update(int64_t timestamp, GvsocLauncherClient *client);

        gv::Io_binding *io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name, GvsocLauncherClient *client);
        gv::Wire_binding *wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name, GvsocLauncherClient *client);

        void vcd_bind(gv::Vcd_user *user, GvsocLauncherClient *client);
        void vcd_enable(GvsocLauncherClient *client);
        void vcd_disable(GvsocLauncherClient *client);
        void event_add(std::string path, bool is_regex, GvsocLauncherClient *client);
        void event_exclude(std::string path, bool is_regex, GvsocLauncherClient *client);
        void *get_component(std::string path, GvsocLauncherClient *client);
        bool is_runnable();
        void wait_runnable();

        vp::Top *top_get() { return this->handler; }

        bool get_is_async() { return this->is_async; }


    private:
        void engine_routine();
        static void *signal_routine(void *__this);
        void check_run();
        void client_run(GvsocLauncherClient *client);
        void client_stop(GvsocLauncherClient *client);
        static void step_handler(vp::Block *__this, vp::TimeEvent *event);

        Logger logger;
        gv::GvsocConf *conf;
        vp::Top *handler;
        int retval = -1;
        gv::Gvsoc_user *user;
        bool is_async;
        std::thread *engine_thread;
        std::thread *signal_thread;
        vp::Component *instance;
        GvProxy *proxy;
        bool running = false;
        std::vector<GvsocLauncherClient *> clients;
        bool is_sim_finished = false;
        pthread_mutex_t lock_mutex;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int run_count = 0;
        int lock_count = 0;
        bool notified_finish = false;
    };

    class GvsocLauncherClient : public gv::Gvsoc
    {
        friend class GvsocLauncher;

    public:
        GvsocLauncherClient(gv::GvsocConf *conf, std::string name="main");
        ~GvsocLauncherClient();

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
        int64_t step_and_wait(int64_t duration) override;
        int64_t step_until_and_wait(int64_t timestamp) override;
        int join() override;
        void retain() override;
        void release() override;
        void lock() override;
        void unlock() override;
        void update(int64_t timestamp) override;
        gv::Io_binding *io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name) override;
        gv::Wire_binding *wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name) override;
        void vcd_bind(gv::Vcd_user *user) override;
        void vcd_enable() override;
        void vcd_disable() override;
        void event_add(std::string path, bool is_regex) override;
        void event_exclude(std::string path, bool is_regex) override;
        void *get_component(std::string path) override;
        void wait_runnable() override;

        virtual void sim_finished(int status);

        void quit(int status);
        void terminate();

    private:
        Logger logger;
        std::string name;
        bool has_quit = false;
        int status;
        bool running = false;
        bool async;
        gv::Gvsoc_user *user = NULL;
    };

};

inline void gv::Logger::info(const char *fmt, ...)
{
// #ifdef VP_TRACE_ACTIVE
//     fprintf(stderr, "[\033[34m%s\033[0m] ", this->module.c_str());
//     va_list ap;
//     va_start(ap, fmt);
//     if (vfprintf(stderr, fmt, ap) < 0) {}
//     va_end(ap);
// #endif
}
