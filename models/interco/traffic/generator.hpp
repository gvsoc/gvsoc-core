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
#include "vp/clock/clock_event.hpp"
#include "vp/itf/wire.hpp"

class TrafficGeneratorConfigMaster;
class TrafficGeneratorSync;
class Generator;

class TrafficGeneratorConfig
{
public:
    bool is_start;
    uint64_t address;
    size_t size;
    size_t packet_size;
    TrafficGeneratorSync *sync;
    bool do_write;
    uint64_t result;
    bool check;
    bool check_status;
    int64_t duration;
};

class TrafficGenerator
{
public:
    virtual void start_transfer() = 0;
};


class TrafficGeneratorSync
{
    friend class Generator;
public:
    TrafficGeneratorSync(vp::ClockEvent *event) : event(event) { this->init(); }
    void init() { this->nb_pre_check_done = 0; this->nb_transfers_done=0; this->nb_post_check_done = 0; this->generators.clear(); }
    void add_generator(TrafficGenerator *generator) { this->generators.push_back(generator); }
    void start() { for (auto gen: generators) { gen->start_transfer(); }}

    vp::ClockEvent *event;

private:
    std::vector<TrafficGenerator *> generators;
    int nb_pre_check_done;
    int nb_transfers_done;
    int nb_post_check_done;
};

class TrafficGeneratorConfigMaster : public vp::WireMaster<TrafficGeneratorConfig *>
{
public:
    // Start sending bursts.
    // Send one burst of size <packet_size> at each cycle for a total of <size> bytes.
    inline void start(uint64_t address, size_t size, size_t packet_size,
        TrafficGeneratorSync *sync, bool do_write=false, bool check=false);
    inline void get_result(bool *check_status=NULL, int64_t *duration=NULL);
};


inline void TrafficGeneratorConfigMaster::start(uint64_t address, size_t size, size_t packet_size,
    TrafficGeneratorSync *sync, bool do_write, bool check)
{
    TrafficGeneratorConfig config = { .is_start=true, .address=address, .size=size,
        .packet_size=packet_size, .sync=sync, .do_write=do_write,
        .check=check
    };
    this->sync(&config);
}

inline void TrafficGeneratorConfigMaster::get_result(bool *check_status, int64_t *duration)
{
    TrafficGeneratorConfig config = { .is_start=false };
    this->sync(&config);
    if (check_status)
    {
        *check_status |= config.check_status;
    }
    if (duration)
    {
        *duration = std::max(*duration, config.duration);
    }
}
