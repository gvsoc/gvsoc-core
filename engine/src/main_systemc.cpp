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
#include <algorithm>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <vp/json.hpp>
#include <systemc.h>
#include "main_systemc.hpp"

static gv::Gvsoc *sc_gvsoc;

class my_module : public sc_module, public gv::Gvsoc_user
{
public:
    SC_HAS_PROCESS(my_module);
    my_module(sc_module_name nm, gv::Gvsoc *gvsoc) : sc_module(nm), gvsoc(gvsoc)
    {
        SC_THREAD(run);
    }

    void run()
    {
        while(1)
        {
            int64_t time = (int64_t)sc_time_stamp().to_double();
            int64_t next_timestamp = gvsoc->step_until(time);

            // when we are not executing the engine, it is retained so that no one else
            // can execute it while we are leeting the systemv engine executes.
            // On the contrary, if someone else is retaining it, we should not let systemv
            // update the time.
            // If so, just call again the step function so that we release the engine for
            // a while.
            if (1) //gvsoc->retain_count() == 1)
            {
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
    }

    void was_updated() override
    {
        sync_event.notify();
    }

    void has_ended() override
    {
        sc_stop();
    }

    gv::Gvsoc *gvsoc;
    sc_event sync_event;
};

int sc_main(int argc, char *argv[])
{
    sc_start();
    return sc_gvsoc->join();
}

int requires_systemc(const char *config_path)
{
    // In case GVSOC was compiled with SystemC, check if we have at least one SystemC component
    // and if so, forward the launch to the dedicated SystemC launcher
    js::Config *js_config = js::import_config_from_file(config_path);
    if (js_config)
    {
        js::Config *gv_config = js_config->get("target/gvsoc");
        if (gv_config)
        {
            return gv_config->get_child_bool("systemc");
        }
    }

    return 0;
}

int systemc_launcher(const char *config_path)
{
    gv::GvsocConf conf = { .config_path=config_path, .api_mode=gv::Api_mode::Api_mode_sync };
    gv::Gvsoc *gvsoc = gv::gvsoc_new(&conf);
    gvsoc->open();
    gvsoc->start();
    sc_gvsoc = gvsoc;
    my_module module("Gvsoc SystemC wrapper", gvsoc);
    gvsoc->bind(&module);
    return sc_core::sc_elab_and_sim(0, NULL);
}
