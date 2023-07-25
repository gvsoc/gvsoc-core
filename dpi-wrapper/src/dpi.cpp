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
#include <gv/gvsoc.h>
#include <gv/gvsoc.hpp>
#include <gv/dpi_chip_wrapper.hpp>

using namespace std;

extern "C" void dpi_create_task(void *arg0, void *arg1);
extern "C" void dpi_wait_event_timeout_ps(long long int delay);
extern "C" long long int dpi_time_ps();
extern "C" void dpi_wait_event();

extern "C" void *dpi_open(char *config_path)
{
  gv::GvsocConf conf = { .config_path=config_path, .api_mode=gv::Api_mode::Api_mode_sync };
  gv::Gvsoc *gvsoc = gv::gvsoc_new(&conf);
  gvsoc->open();

  return (void *)gvsoc;
}

static void gvsoc_sync_task(void *arg)
{
    gv::Gvsoc *gvsoc = (gv::Gvsoc *)arg;

    while(1)
    {
        int64_t time = dpi_time_ps();
        int64_t next_timestamp = gvsoc->step_until(time);
        if (next_timestamp == -1)
        {
          dpi_wait_event();
        }
        else
        {
          dpi_wait_event_timeout_ps(next_timestamp - time);
        }
    }
}

extern "C" int dpi_start(void *instance)
{
  gv::Gvsoc *gvsoc = (gv::Gvsoc *)instance;
  gvsoc->start();

  dpi_create_task((void *)gvsoc_sync_task, gvsoc);


  return 0;
}

extern "C" void dpi_start_task(void *arg0, void *arg1)
{
    void (*callback)(void *) = (void (*)(void *))arg0;
    callback(arg1);
}

extern "C" void *dpi_bind(void *handle, char *name, int sv_handle)
{
  void *result = gv_chip_pad_bind(handle, name, sv_handle);
  return result;
}

extern "C" void dpi_edge(void *handle, int64_t timestamp, int data)
{
  Dpi_chip_wrapper_callback *callback = (Dpi_chip_wrapper_callback *)handle;
  callback->function(callback->_this, timestamp, data);
}