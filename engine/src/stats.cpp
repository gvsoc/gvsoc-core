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

#include <cstdio>
#include <cmath>
#include <map>
#include <vp/stats/stats_engine.hpp>
#include <vp/stats/block_stat.hpp>
#include <vp/block.hpp>
#include <vp/time/block_time.hpp>
#include <vp/time/implementation.hpp>
#include <vp/json.hpp>


// --- Formatting helpers ---

static std::string format_time_human(int64_t ps)
{
    double val = (double)ps;
    if (val >= 1e12) return std::to_string((int64_t)(val / 1e12)) + " s";
    if (val >= 1e9)  { char buf[64]; snprintf(buf, sizeof(buf), "%.2f ms", val / 1e9); return buf; }
    if (val >= 1e6)  { char buf[64]; snprintf(buf, sizeof(buf), "%.2f us", val / 1e6); return buf; }
    if (val >= 1e3)  { char buf[64]; snprintf(buf, sizeof(buf), "%.2f ns", val / 1e3); return buf; }
    return std::to_string(ps) + " ps";
}

// Extract the component path (everything except the last /stat_name)
static std::string get_component_path(const std::string &full_path)
{
    auto pos = full_path.rfind('/');
    if (pos != std::string::npos && pos > 0)
        return full_path.substr(0, pos);
    return full_path;
}

// Extract the stat name (after the last /)
static std::string get_stat_name(const std::string &full_path)
{
    auto pos = full_path.rfind('/');
    if (pos != std::string::npos)
        return full_path.substr(pos + 1);
    return full_path;
}


// --- StatsEngine ---

vp::StatsEngine::StatsEngine(js::Config *config)
{
    js::Config *stats_cfg = config->get("stats");
    if (stats_cfg != nullptr && stats_cfg->get_bool())
    {
        this->enabled = true;
    }

    // Display options (read from config, set by CLI flags)
    js::Config *cfg;
    cfg = config->get("stats_raw");
    this->opt_raw = (cfg != nullptr && cfg->get_bool());

    cfg = config->get("stats_flat");
    this->opt_flat = (cfg != nullptr && cfg->get_bool());
}


void vp::StatsEngine::init(vp::Block *top)
{
    this->top = top;

    // Auto-start at time 0 if not explicitly started
    if (this->start_timestamp < 0)
    {
        this->start_timestamp = 0;
    }
}


void vp::StatsEngine::register_stat(const std::string &path, StatCommon *stat, const std::string &desc)
{
    this->entries.push_back({path, stat, desc});
    stat->registered(this);
}





void vp::StatsEngine::dump(const std::string &filename)
{
    // First dump of the run truncates, subsequent dumps append
    const char *mode = this->first_dump ? "w" : "a";
    this->first_dump = false;

    FILE *file = fopen(filename.c_str(), mode);
    if (file == nullptr)
    {
        return;
    }
    this->dump(file);
    fclose(file);
}


// --- BlockStat ---

void vp::BlockStat::register_stat(StatCommon *stat, const std::string &name, const std::string &desc)
{
    if (this->engine != nullptr)
    {
        std::string path = this->top->get_path() + "/" + name;
        this->engine->register_stat(path, stat, desc);
    }
}


#ifdef CONFIG_GVSOC_STATS_ACTIVE

void vp::StatBw::registered(StatsEngine *engine)
{
    this->engine = engine;
}


void vp::StatBw::set_source(StatScalar *source)
{
    this->source = source;
    if (this->engine)
    {
        this->engine->register_pre_dump([this]() { this->compute(); });
    }
}


void vp::StatBw::compute()
{
    int64_t duration_ps = this->engine->get_duration_ps();
    if (duration_ps > 0 && this->source)
    {
        double duration_s = (double)duration_ps / 1e12;
        this->bw = (double)this->source->get() / duration_s;
    }
    else
    {
        this->bw = 0;
    }
}

#endif  // CONFIG_GVSOC_STATS_ACTIVE


void vp::StatsEngine::start(int64_t timestamp)
{
    this->start_timestamp = timestamp;
    this->stop_timestamp = -1;

    // Reset all stats when starting a new collection window
    for (auto &entry : this->entries)
    {
        entry.stat->reset();
    }

    // Invoke start callbacks (e.g. clock domains snapshotting start cycle)
    for (auto &cb : this->start_callbacks)
    {
        cb();
    }
}


void vp::StatsEngine::stop(int64_t timestamp)
{
    this->stop_timestamp = timestamp;
}


void vp::StatsEngine::register_pre_dump(std::function<void()> callback)
{
    this->pre_dump_callbacks.push_back(callback);
}


void vp::StatsEngine::register_start_callback(std::function<void()> callback)
{
    this->start_callbacks.push_back(callback);
}


int64_t vp::StatsEngine::get_duration_ps() const
{
    if (this->start_timestamp >= 0 && this->stop_timestamp >= 0)
    {
        return this->stop_timestamp - this->start_timestamp;
    }
    return 0;
}


void vp::StatsEngine::dump_flat(FILE *file)
{
    int64_t duration_ps = (this->start_timestamp >= 0 && this->stop_timestamp >= 0)
        ? this->stop_timestamp - this->start_timestamp : 0;

    if (this->opt_raw)
    {
        fprintf(file, "========== Simulation Statistics (raw) ==========\n\n");
        fprintf(file, "  Start time:  %ld ps\n", this->start_timestamp);
        fprintf(file, "  Stop time:   %ld ps\n", this->stop_timestamp);
        fprintf(file, "  Duration:    %ld ps\n\n", duration_ps);
    }
    else
    {
        fprintf(file, "========== Simulation Statistics ==========\n\n");
        fprintf(file, "  Start time:  %s\n", format_time_human(this->start_timestamp).c_str());
        fprintf(file, "  Stop time:   %s\n", format_time_human(this->stop_timestamp).c_str());
        fprintf(file, "  Duration:    %s\n\n", format_time_human(duration_ps).c_str());
    }

    for (auto &entry : this->entries)
    {
        std::string val = entry.stat->format_value(this->opt_raw);
        fprintf(file, "  %-55s %12s", entry.path.c_str(), val.c_str());
        if (!entry.desc.empty())
            fprintf(file, "  # %s", entry.desc.c_str());
        fprintf(file, "\n");
    }

    fprintf(file, "\n==========================================\n");
}


void vp::StatsEngine::dump_grouped(FILE *file)
{
    int64_t duration_ps = (this->start_timestamp >= 0 && this->stop_timestamp >= 0)
        ? this->stop_timestamp - this->start_timestamp : 0;

    fprintf(file, "========== Simulation Statistics ==========\n\n");
    if (this->opt_raw)
    {
        fprintf(file, "  Start:     %ld ps\n", this->start_timestamp);
        fprintf(file, "  Stop:      %ld ps\n", this->stop_timestamp);
        fprintf(file, "  Duration:  %ld ps\n", duration_ps);
    }
    else
    {
        fprintf(file, "  Start:     %s\n", format_time_human(this->start_timestamp).c_str());
        fprintf(file, "  Stop:      %s\n", format_time_human(this->stop_timestamp).c_str());
        fprintf(file, "  Duration:  %s\n", format_time_human(duration_ps).c_str());
    }
    fprintf(file, "\n");

    // Group entries by component path
    struct GroupEntry {
        std::string name;
        StatCommon *stat;
        std::string desc;
    };
    std::map<std::string, std::vector<GroupEntry>> groups;
    std::vector<std::string> group_order;

    for (auto &entry : this->entries)
    {
        std::string comp = get_component_path(entry.path);
        std::string name = get_stat_name(entry.path);
        if (groups.find(comp) == groups.end())
            group_order.push_back(comp);
        groups[comp].push_back({name, entry.stat, entry.desc});
    }

    for (auto &comp : group_order)
    {
        auto &stats = groups[comp];
        fprintf(file, "--- %s ---\n", comp.c_str());

        for (auto &s : stats)
        {
            std::string val = s.stat->format_value(this->opt_raw);
            fprintf(file, "  %-28s %12s\n", s.name.c_str(), val.c_str());
        }

        fprintf(file, "\n");
    }

    fprintf(file, "==========================================\n");
}


void vp::StatsEngine::dump(FILE *file)
{
    // Auto-stop at current time if not explicitly stopped
    if (this->stop_timestamp < 0 && this->top != nullptr)
    {
        this->stop_timestamp = this->top->time.get_time();
    }

    // Invoke pre-dump callbacks (e.g. clock domains snapshotting cycles)
    for (auto &cb : this->pre_dump_callbacks)
    {
        cb();
    }

    if (this->opt_flat)
    {
        this->dump_flat(file);
    }
    else
    {
        this->dump_grouped(file);
    }
}
