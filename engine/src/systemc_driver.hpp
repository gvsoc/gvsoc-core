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

#ifdef VP_USE_SYSTEMC

namespace gv
{
    class Controller;
    class ControllerClient;
};

// Returns true if the configuration at config_path contains at least one SystemC
// component (systemc=true in the engine options).
int requires_systemc(const char *config_path);

// Standalone (CLI) entry point: instantiate a synchronous GVSOC, then drive its time
// engine from the SystemC kernel. Used by the gvsoc_launcher executables.
int systemc_launcher(const char *config_path);

// Drive an already-opened/started GVSOC engine from the SystemC kernel.
// The caller must hold the engine mutex (gv::Controller::lock()) so that the in-process
// stepper can safely synchronize with other clients. Returns when the SystemC kernel
// stops (end of simulation).
int systemc_run(gv::Controller *controller, gv::ControllerClient *client);

// Spawn a dedicated thread which becomes the in-process SystemC kernel for the GUI.
// The thread acquires the engine mutex and drives the in-process engine via systemc_run.
// Must be called once the GUI has opened and started GVSOC.
void systemc_gui_start();

#endif
