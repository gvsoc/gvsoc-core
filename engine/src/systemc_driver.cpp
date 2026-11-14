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

#include <gv/gvsoc.hpp>
#include <vp/controller.hpp>
#include <algorithm>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <vp/json.hpp>
#include <systemc.h>
#include "systemc_driver.hpp"

// SystemC module driving the in-process GVSOC time engine.
//
// The kernel is the master clock: on each iteration it advances the GVSOC engine up to
// the current SystemC time (step_until_sync), then sleeps in SystemC until the next GVSOC
// event (or until a SystemC peripheral updates a GVSOC input, which fires sync_event via
// was_updated). The whole loop runs on the thread which owns the engine mutex, so it
// reuses the controller's synchronous stepping primitives directly. Play/pause is handled
// transparently by wait_runnable(), which blocks while any client (e.g. the GUI) has
// stopped the engine.
class my_module : public sc_module, public gv::Gvsoc_user
{
public:
    SC_HAS_PROCESS(my_module);
    my_module(sc_module_name nm, gv::Controller *controller, gv::ControllerClient *client)
        : sc_module(nm), controller(controller), client(client)
    {
        SC_THREAD(run);
    }

    void run()
    {
        while(1)
        {
            int64_t time = (int64_t)sc_time_stamp().to_double();
            this->controller->step_until_sync(time, this->client);
            int64_t next_timestamp = this->client->get_next_event_time();

            if (this->is_sim_finished)
            {
                sc_stop();
                break;
            }

            // when we are not executing the engine, it is retained so that no one else
            // can execute it while we are leeting the systemv engine executes.
            // On the contrary, if someone else is retaining it, we should not let systemv
            // update the time.
            // If so, just call again the step function so that we release the engine for
            // a while.
            this->controller->wait_runnable();

            if (next_timestamp == -1)
            {
                wait(sync_event);
            }
            else
            {
                wait(next_timestamp - time, SC_PS, sync_event);
            }

        }
    }

    void was_updated() override
    {
        sync_event.notify();
    }

    void has_ended(int status) override
    {
        this->is_sim_finished = true;
        sync_event.notify();
    }

    gv::Controller *controller;
    gv::ControllerClient *client;
    sc_event sync_event;
    bool is_sim_finished = false;
};

int sc_main(int argc, char *argv[])
{
    sc_start();
    return 0;
}

int requires_systemc(const char *config_path)
{
    // In case GVSOC was compiled with SystemC, check if we have at least one SystemC component
    // and if so, forward the launch to the dedicated SystemC launcher
    js::Config *gv_config = js::import_config_from_file(config_path);
    if (gv_config)
    {
        return gv_config->get_child_bool("systemc");
    }

    return 0;
}

int systemc_run(gv::Controller *controller, gv::ControllerClient *client)
{
    my_module module("Gvsoc_SystemC_wrapper", controller, client);
    // Become the engine's user so we receive was_updated (to resynchronize the SystemC
    // wait) and has_ended (to stop the kernel). In the GUI this overrides the database's
    // no-op user binding, which is harmless since the database is fed through vcd_bind.
    client->bind(&module);
    return sc_core::sc_elab_and_sim(0, NULL);
}

int systemc_launcher(const char *config_path)
{
    gv::GvsocConf conf = { .config_path=config_path, .api_mode=gv::Api_mode::Api_mode_sync };
    gv::Gvsoc *gvsoc = gv::gvsoc_new(&conf);
    gvsoc->open();
    gvsoc->start();
    // Synchronous mode: start() already holds the engine mutex on this thread, so we can
    // drive the SystemC kernel directly.
    systemc_run(&gv::Controller::get(), (gv::ControllerClient *)gvsoc);
    gvsoc->quit(0);
    return gvsoc->join();
}

void systemc_gui_start()
{
    // The GUI engine runs asynchronously: spawn a dedicated thread which becomes the
    // SystemC kernel and drives the in-process engine. It takes the engine mutex (like the
    // normal engine_routine would) so that the controller's stepping/locking protocol is
    // respected, and the GUI/SDL thread keeps driving play/pause through the client.
    new std::thread([]()
    {
        gv::Controller &controller = gv::Controller::get();
        controller.lock();
        systemc_run(&controller, controller.default_client_get());
        controller.unlock();
    });
}
