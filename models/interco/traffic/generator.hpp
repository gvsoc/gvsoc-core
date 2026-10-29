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
    // A second stream of <size> bytes from address2 (do_write2), its
    // packets interleaved with the first one's, as one DMA alternating two
    // transfers (GeneratorV2 only, without data check).
    bool interleave;
    uint64_t address2;
    bool do_write2;
    // Support for collective op code and mask
    int collective_op;
    uint64_t collective_mask;
    // When collectives are set, checks incoming data against this externally-supplied 
    // buffer instead to verify reduce and broadcast results
    const uint8_t *expected_data;
    // Use this externally-supplied buffer instead of the self-generated `ref_data`
    const uint8_t *write_data;
};

class TrafficGenerator
{
public:
    virtual void start_transfer() = 0;
};


class TrafficGeneratorSync
{
    friend class Generator;
    friend class GeneratorV2;
public:
    TrafficGeneratorSync(vp::ClockEvent *event=NULL) : event(event) { this->init(); }
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
        TrafficGeneratorSync *sync, bool do_write=false, bool check=false,
        int collective_op=0, uint64_t collective_mask=0,
        const uint8_t *expected_data=nullptr, const uint8_t *write_data=nullptr);
    // The same with a second stream (address2, do_write2) of the same size,
    // one packet of each in turn.
    inline void start_interleaved(uint64_t address, bool do_write, uint64_t address2,
        bool do_write2, size_t size, size_t packet_size, TrafficGeneratorSync *sync);
    inline void get_result(bool *check_status=NULL, int64_t *duration=NULL);
    inline bool is_finished();
};


inline void TrafficGeneratorConfigMaster::start(uint64_t address, size_t size, size_t packet_size,
    TrafficGeneratorSync *sync, bool do_write, bool check, int collective_op,
    uint64_t collective_mask, const uint8_t *expected_data, const uint8_t *write_data)
{
    TrafficGeneratorConfig config = { .is_start=true, .address=address, .size=size,
        .packet_size=packet_size, .sync=sync, .do_write=do_write,
        .check=check, .collective_op=collective_op,
        .collective_mask=collective_mask, .expected_data=expected_data,
        .write_data=write_data
    };
    this->sync(&config);
}

inline void TrafficGeneratorConfigMaster::start_interleaved(uint64_t address, bool do_write,
    uint64_t address2, bool do_write2, size_t size, size_t packet_size, TrafficGeneratorSync *sync)
{
    TrafficGeneratorConfig config = { .is_start=true, .address=address, .size=size,
        .packet_size=packet_size, .sync=sync, .do_write=do_write, .check=false,
        .interleave=true, .address2=address2, .do_write2=do_write2
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

inline bool TrafficGeneratorConfigMaster::is_finished()
{
    TrafficGeneratorConfig config = { .is_start=false };
    this->sync(&config);
    return config.result;
}
