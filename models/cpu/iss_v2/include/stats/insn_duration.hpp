/*
 * Copyright (C) 2026 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#pragma once

#include <vp/vp.hpp>

#ifdef CONFIG_GVSOC_STATS_ACTIVE

#include <string>
#include <unordered_map>
#include <vp/stats/stats.hpp>
#include <vp/stats/block_stat.hpp>

// Per-label duration accumulator. One instance is registered with the stats
// engine per distinct instruction label; it tracks the occurrence count, the
// total, minimum and maximum duration in cycles, and reports the average at
// dump time. Modeled on the derived stats in event/event.hpp (StatIpc, ...).
class StatInsnDuration : public vp::StatCommon
{
public:
    inline void account(int64_t cycles)
    {
        if (this->count == 0 || cycles < this->min) this->min = cycles;
        if (this->count == 0 || cycles > this->max) this->max = cycles;
        this->total += cycles;
        this->count++;
    }

    std::string format_value(bool raw) const override
    {
        double avg = this->count ? (double)this->total / (double)this->count : 0.0;
        char buf[96];
        if (raw)
        {
            snprintf(buf, sizeof(buf), "%f", avg);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%.2f cyc  (n=%llu, min=%lld, max=%lld)",
                avg, (unsigned long long)this->count,
                (long long)(this->count ? this->min : 0),
                (long long)(this->count ? this->max : 0));
        }
        return buf;
    }

    void reset() override { this->count = 0; this->total = 0; this->min = 0; this->max = 0; }

private:
    uint64_t count = 0;
    uint64_t total = 0;
    int64_t min = 0;
    int64_t max = 0;
};

// Collection of per-label duration stats. Labels are discovered at runtime, so
// the matching StatInsnDuration is allocated and registered lazily the first
// time a label is seen. The map is node-based, so the address handed to
// register_stat() stays valid for the rest of the run. Registering mid-sim is
// safe: register_stat() only appends to the engine's entry list, which is
// iterated at dump time.
class InsnDurationStats
{
public:
    // Cache the block stat helper and the group prefix (e.g. "insn_duration").
    // Each label becomes "<group>/<label>", which the grouped dumper collects
    // under "<core_path>/<group>".
    void init(vp::BlockStat *stats, const std::string &group)
    {
        this->stats = stats;
        this->group = group;
    }

    inline void account(const char *label, int64_t cycles)
    {
        Entry &stat = this->map[label];
        if (!stat.registered_in_engine)
        {
            stat.registered_in_engine = true;
            this->stats->register_stat(&stat, this->group + "/" + label,
                "Average execution duration in cycles");
        }
        stat.account(cycles);
    }

private:
    // Per-label stat plus a flag tracking whether it has been registered yet.
    struct Entry : public StatInsnDuration
    {
        bool registered_in_engine = false;
    };

    std::unordered_map<std::string, Entry> map;
    vp::BlockStat *stats = nullptr;
    std::string group;
};

#endif  // CONFIG_GVSOC_STATS_ACTIVE
