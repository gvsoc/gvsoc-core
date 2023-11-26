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

#include "vp/vp.hpp"
#include "vp/trace/trace.hpp"



void vp::PowerEngine::reg_trace(vp::PowerTrace *trace)
{
    this->traces.push_back(trace);
}



void vp::PowerEngine::start_capture()
{
    // When capture is started, just broadcast to all traces so that they
    // reset all current values
    for (auto trace : this->traces)
    {
        trace->report_start();
    }
}



void vp::PowerEngine::stop_capture()
{
    // When stopping, dump recursively all traces to a file

    if (this->file)
    {
        fprintf(file, "Power report start\n");
        fprintf(file, "------------------\n");
        fprintf(file, "\n");
        this->top->power.dump_traces_recursive(file);
        fprintf(file, "\n\n\n");
    }
}



double vp::PowerEngine::get_average_power(double &dynamic_power, double &static_power)
{
    return this->top->power.get_average_power(dynamic_power, static_power);
}



vp::PowerEngine::PowerEngine()
{
    this->file = fopen("power_report.csv", "w");
    if (this->file == NULL)
    {
        //vp_warning_always(&this->warning, "Failed to open power report file (path: %s)\n", "power_report.csv");
    }
}


void vp::PowerEngine::init(vp::Block *top)
{
    this->top = top;
}


vp::PowerEngine::~PowerEngine()
{
    if (this->file)
    {
        fclose(this->file);
    }
}
