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


#include <stdint.h>
#include <stdio.h>
#include <gv/gvsoc.hpp>
#include "svdpi.h"

using namespace std;

extern "C" void dpi_create_task(void *arg0, void *arg1);
extern "C" void dpi_wait_event_timeout_ps(long long int delay);
extern "C" long long int dpi_time_ps();
extern "C" void dpi_wait_event();
extern "C" void dpi_raise_event();
extern "C" void dpi_set_status(int status);
extern "C" void dpi_external_edge(int handle, uint32_t value);

static svScope scope;


class Dpi_launcher : public gv::Gvsoc_user
{
    public:
        Dpi_launcher(gv::Gvsoc &gvsoc) : gvsoc(gvsoc) {}
        void was_updated() override;
        void has_ended(int status) override;

        gv::Gvsoc &gvsoc;
        bool is_sim_finished = false;
    private:
};

class Dpi_wire : public gv::Wire_user
{
    public:
        Dpi_wire(int dpi_handle) : dpi_handle(dpi_handle) {}
        void update(int value);

    private:
        int dpi_handle;
};

class Dpi_wire_binding
{
    public:
        Dpi_wire_binding(gv::Gvsoc *gvsoc, gv::Wire_binding *binding) : gvsoc(gvsoc), binding(binding) {}

        gv::Gvsoc *gvsoc;
        gv::Wire_binding *binding;
};

static bool updated = false;

void Dpi_launcher::was_updated()
{
    // Since dpi exported function can only be called from a systemverilog thread, we can only
    // trigger the event if the scope is the one used when opening the dpi wrapper.
    // If an external thread pushed something, this will be handled with polling with some delay.
    if (svGetScope() == scope)
    {
        dpi_raise_event();
    }
}

void Dpi_launcher::has_ended(int status)
{
    this->is_sim_finished = true;
}

void Dpi_wire::update(int value)
{
    dpi_external_edge(this->dpi_handle, value);
}


extern "C" void *dpi_open(char *config_path)
{
    scope = svGetScope();

  gv::GvsocConf conf = { .config_path=config_path, .api_mode=gv::Api_mode::Api_mode_sync };
  gv::Gvsoc *gvsoc = gv::gvsoc_new(&conf);
  gvsoc->open();
  Dpi_launcher *launcher = new Dpi_launcher(*gvsoc);
  gvsoc->bind(launcher);


    if (conf.proxy_socket != -1)
    {
        fprintf(stderr, "Opened proxy on socket %d\n", conf.proxy_socket);
    }

  return (void *)launcher;
}

static void gvsoc_sync_task(void *arg)
{
    Dpi_launcher *launcher = (Dpi_launcher *)arg;
    gv::Gvsoc *gvsoc = &launcher->gvsoc;

    while(1)
    {
        int64_t time = dpi_time_ps();
        gvsoc->step_until(time);
        int64_t next_timestamp = gvsoc->get_next_event_time();

        // when we are not executing the engine, it is retained so that no one else
        // can execute it while we are leeting the systemv engine executes.
        // On the contrary, if someone else is retaining it, we should not let systemv
        // update the time.
        // If so, just call again the step function so that we release the engine for
        // a while.
        gvsoc->wait_runnable();

        if (next_timestamp == -1)
        {
            // Since raise event can not be called from external thread when using proxy mode,
            // we have to poll instead of using an event, in case an external thread pushed
            // something.
            // dpi_wait_event();
            dpi_wait_event_timeout_ps(100000);
        }
        else
        {
            dpi_wait_event_timeout_ps(next_timestamp - time);
        }

        if (launcher->is_sim_finished)
        {
            gvsoc->quit(0);
            int status = gvsoc->join();
            dpi_set_status(status);
        }
    }
}

extern "C" int dpi_start(void *instance)
{
    Dpi_launcher *launcher = (Dpi_launcher *)instance;
    gv::Gvsoc *gvsoc = &launcher->gvsoc;

  gvsoc->start();

  dpi_create_task((void *)gvsoc_sync_task, launcher);


  return 0;
}

extern "C" void dpi_start_task(void *arg0, void *arg1)
{
    void (*callback)(void *) = (void (*)(void *))arg0;
    callback(arg1);
}

extern "C" void *dpi_bind(void *handle, char *name, int sv_handle)
{
    Dpi_launcher *launcher = (Dpi_launcher *)handle;
    gv::Gvsoc *gvsoc = &launcher->gvsoc;

    Dpi_wire *wire = new Dpi_wire(sv_handle);
    gv::Wire_binding *binding = gvsoc->wire_bind(wire, name, "");
    void *result = (void *)new Dpi_wire_binding(gvsoc, binding);

    return result;
}

extern "C" void dpi_edge(void *handle, int64_t timestamp, int data)
{
    Dpi_wire_binding *binding = (Dpi_wire_binding *)handle;
    binding->gvsoc->update(timestamp);
    if (binding->binding)
    {
        binding->binding->update(data);
    }
}
