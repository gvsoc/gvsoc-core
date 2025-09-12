/*
 * Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
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

#include <stdint.h>
#include "vp/itf/wire.hpp"

class TrafficGeneratorConfig
{
public:
    bool is_start = false;
    uint64_t address;
    size_t size;
    size_t packet_size;
    vp::ClockEvent *end_trigger;
    bool do_write;
    uint64_t result;
};


class TrafficGeneratorConfigMaster : public vp::WireMaster<TrafficGeneratorConfig *>
{
public:
    // Start sending bursts.
    // Send one burst of size <packet_size> at each cycle for a total of <size> bytes.
    inline void start(uint64_t address, size_t size, size_t packet_size,
        vp::ClockEvent *end_trigger, bool do_write=false);
    inline bool is_finished();
};


inline void TrafficGeneratorConfigMaster::start(uint64_t address, size_t size, size_t packet_size,
    vp::ClockEvent *end_trigger, bool do_write)
{
    TrafficGeneratorConfig config = { .is_start=true, .address=address, .size=size,
        .packet_size=packet_size, .end_trigger=end_trigger, .do_write=do_write};
    this->sync(&config);
}

inline bool TrafficGeneratorConfigMaster::is_finished()
{
    TrafficGeneratorConfig config = { .is_start=false };
    this->sync(&config);
    return config.result;
}
