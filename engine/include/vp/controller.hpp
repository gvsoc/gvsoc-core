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

    class ControllerClient;
    class Controller;

    extern gv::Controller controller;

    class Logger
    {
    public:
        Logger(std::string module);
        inline void info(const char *fmt, ...);

    private:
        std::string module;
    };

    class Controller
    {
        friend class ControllerClient;

    public:
        Controller();
        void init(gv::GvsocConf *conf);

        static Controller &get() {
            static Controller controller;
            return controller;
        }

        // Open launcher. This instantiate the system and setup simulation loop
        void open(ControllerClient *client);
        // Bind a user controller so that it is notified about event enqueueuing
        void bind(gv::Gvsoc_user *user, ControllerClient *client);
        // Start the system. This build the whole system, call start on it, reset it,
        // and wait proxy connection if needed and check if simulation can be run
        void start(ControllerClient *client);
        // Close system models
        void close(ControllerClient *client);
        // Run events until a stop request is received.
        int64_t run_sync();
        // Ask internal loop to run events until a stop request is received
        void run_async(ControllerClient *client);
        // Flush system models
        void flush(ControllerClient *client);
        // Terminate simulation. THis will mark it as over and will notified all clients
        void sim_finished(int status);
        // BLock caller untiul simulation is over and every client is over
        int join(ControllerClient *client);
        // Lock launcher. This is a simple mutex, can be used to enter the launcher when we are
        // sure the engine is stopped, e.g. for a step or a run.
        void lock();
        // Unlock launcher
        void unlock();
        // Stop the engine and lock the launcher. Can be used either when engine is running or
        // stopped but is much expensive that the simple lock
        void engine_lock();
        // Unlock the launcher and run the engine if needed.
        void engine_unlock();
        // Stop the client. This will prevent simulation from running until client is run again
        int64_t stop(ControllerClient *client);
        // Run simulation for specified duration in synchronous mode
        int64_t step_sync(int64_t duration, ControllerClient *client);
        // Run simulation for specified duration in asynchronous mode
        int64_t step_async(int64_t duration, ControllerClient *client, void *request);
        // Run simulation until specified timestamp is reached, in synchronous mode
        int64_t step_until_sync(int64_t timestamp, ControllerClient *client);
        // Run simulation until specified timestamp is reached, in asynchronous mode
        int64_t step_until_async(int64_t timestamp, ControllerClient *client, void *request);
        // Run simulation for specified duration in asynchronous mode and blocks caller until
        // step is reached
        int64_t step_and_wait_async(int64_t duration, ControllerClient *client);
        // Run simulation until specified timestamp is reached, in asynchronous mode and blocks
        // caller until step is reached
        int64_t step_until_and_wait_async(int64_t timestamp, ControllerClient *client);
        // Block caller until simulation can be run. Used in synchronous mode to make no other client
        // is blocking simulation before continuing.
        void wait_runnable();
        // Must be called when client wants to quit. This will unblock other clients waiting for its
        // termination in the join method
        void client_quit(gv::ControllerClient *client);
        // Add a client. This may stop simulation.
        void register_client(ControllerClient *client);
        // Remove a client. This may stop simulation.
        void unregister_client(ControllerClient *client);
        // Can be called to handle a semi-hosting stop
        void syscall_stop_handle();

        double get_instant_power(double &dynamic_power, double &static_power, ControllerClient *client);
        double get_average_power(double &dynamic_power, double &static_power, ControllerClient *client);
        void report_start(ControllerClient *client);
        void report_stop(ControllerClient *client);
        gv::PowerReport *report_get(ControllerClient *client);


        void update(int64_t timestamp, ControllerClient *client);

        gv::Io_binding *io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name, ControllerClient *client);
        gv::Wire_binding *wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name, ControllerClient *client);

        void vcd_bind(gv::Vcd_user *user, ControllerClient *client);
        void vcd_enable(ControllerClient *client);
        void vcd_disable(ControllerClient *client);
        void event_add(std::string path, bool is_regex, ControllerClient *client);
        void event_exclude(std::string path, bool is_regex, ControllerClient *client);
        void *get_component(std::string path, ControllerClient *client);

        vp::Top *top_get() { return this->handler; }
        ControllerClient *default_client_get() { return this->clients[0]; }

    protected:
        bool is_init = false;

    private:
        // Tells if no client is preventing simulation from being run.
        bool is_runnable();
        // Thread engine entry in asynchronous mode
        void engine_routine();
        static void *signal_routine(void *__this);
        // Check if simulation must be running or stopped depending on client state (stopped/running
        // and lock count)
        void check_run();
        // Mark client as runnable, and enable simulation if needed
        void client_run(ControllerClient *client);
        // Mark client as stopped, and stop simulation if needed
        void client_stop(ControllerClient *client);
        // Static handler used as time event callback, used to stop engine when a step is reached,
        // for asynchronous mode
        static void step_async_handler(vp::Block *__this, vp::TimeEvent *event);
        // Static handler used as time event callback, used to stop engine when a step is reached,
        // for synchronous mode
        static void step_sync_handler(vp::Block *__this, vp::TimeEvent *event);

        // Internal logger
        Logger logger;
        // Main GVSOC controller configuration
        gv::GvsocConf *conf;
        // Top model class containing all engines
        vp::Top *handler;
        // Simulation retval set when simulation terminates
        int retval = -1;
        // User launcher to be notified when an event is enqueued
        gv::Gvsoc_user *user;
        // Tell if main controller is asynchronous
        bool is_async;
        // Thread running the engine in asynchronous mode
        std::thread *engine_thread;
        // Thread handling ctrlC
        std::thread *signal_thread;
        // Top system instance
        vp::Component *instance;
        // Proxy
        GvProxy *proxy;
        // Tell if time engine should be running. In asynchronous mode, there might be a delay
        // with the actual state of the engine
        bool running = false;
        // List of current clients
        std::vector<ControllerClient *> clients;
        // True if simulation has terminated
        bool is_sim_finished = false;
        // Mutex used for managing simulation state, i.e. the fields running, run_count and
        // lock_count
        pthread_mutex_t lock_mutex;
        // Time engine mutex. The engine owns the mutex when it is running. Any actor must take
        // it before accessing the models
        pthread_mutex_t mutex;
        // Time engine condition used for various wakeup
        pthread_cond_t cond;
        // Number of runnable clients
        int run_count = 0;
        // Number of locked clients
        int lock_count = 0;
        // True when simulation termination has been received and notified to clients
        bool notified_finish = false;
        vp::Block *step_block;
    };

    class ControllerClient : public gv::Gvsoc
    {
        friend class Controller;

    public:
        ControllerClient(gv::GvsocConf *conf, std::string name="main");
        ~ControllerClient();

        void open() override;
        void bind(gv::Gvsoc_user *user) override;
        void close() override;
        void run() override;
        void start() override;
        void flush() override;
        int64_t stop() override;
        double get_instant_power(double &dynamic_power, double &static_power) override;
        double get_average_power(double &dynamic_power, double &static_power) override;
        void report_start() override;
        void report_stop() override;
        gv::PowerReport *report_get() override;
        int64_t step(int64_t duration) override;
        void step_request(int64_t duration, void *data) override;
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
        void terminate() override;
        void quit(int status) override;

        // Called by launcher when simulation is over to notify each client
        virtual void sim_finished(int status);
        virtual void step_handle(void *request) {}
        virtual void syscall_stop_handle() {}

    private:

        // Internal logger
        Logger logger;
        // Client name used for debugging
        std::string name;
        // True when the client has quit
        bool has_quit = false;
        // Return status given when the client has quit
        int status;
        // Tell if the client is runnable or stopped
        bool running = false;
        // Tell if the client is synchronous or asynchronous
        bool async;
        // User controller to be notified when simulation has ended
        gv::Gvsoc_user *user = NULL;
        // Step event used to stop engine when stepping in synchronous mode
        vp::TimeEvent *step_event = NULL;
    };

};

inline void gv::Logger::info(const char *fmt, ...)
{
// #ifdef VP_TRACE_ACTIVE
    // fprintf(stderr, "[\033[34m%s\033[0m] ", this->module.c_str());
    // va_list ap;
    // va_start(ap, fmt);
    // if (vfprintf(stderr, fmt, ap) < 0) {}
    // va_end(ap);
// #endif
}
