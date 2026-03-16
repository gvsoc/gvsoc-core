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

#include <string>
#include <vector>
#include <functional>
#include <vp/stats/stats.hpp>

namespace js
{
    class Config;
}

namespace vp
{
    class Block;

    /**
     * @brief Central engine for collecting and dumping statistics.
     *
     * Created by Top and passed through ComponentConf. Each component registers
     * its stats via register_stat(). On shutdown, dump() writes all non-zero stats
     * to a file.
     */
    class StatsEngine
    {
    public:
        /**
         * @brief Construct a new StatsEngine.
         *
         * @param config Global GVSOC config (reads "stats" and "stats/output" keys).
         */
        StatsEngine(js::Config *config);

        /**
         * @brief Initialize engine after component tree is built.
         *
         * @param top Top-level component.
         */
        void init(vp::Block *top);

        /**
         * @brief Register a stat with its full hierarchical path.
         *
         * @param path Full path (e.g. "/chip/soc/l2/stat_reads").
         * @param stat Pointer to the stat object.
         */
        void register_stat(const std::string &path, StatCommon *stat, const std::string &desc = "");

        /**
         * @brief Dump all non-zero stats to a file.
         *
         * @param filename Output file path.
         */
        void dump(const std::string &filename);

        /**
         * @brief Dump all non-zero stats to an open file handle.
         *
         * @param file Output file handle.
         */
        void dump(FILE *file);

        /**
         * @brief Check whether statistics collection is enabled.
         *
         * @return true if --stats was passed.
         */
        bool is_enabled() const { return this->enabled; }

        /**
         * @brief Check whether dump() has been called at least once.
         *
         * Used to skip the automatic end-of-simulation dump when the user
         * already dumped explicitly via semihosting.
         */
        bool has_dumped() const { return !this->first_dump; }

        /**
         * @brief Start collecting statistics from current timestamp.
         *
         * Records the start timestamp. Can be called from semihosting.
         *
         * @param timestamp Simulation time in picoseconds.
         */
        void start(int64_t timestamp);

        /**
         * @brief Stop collecting and record end timestamp.
         *
         * @param timestamp Simulation time in picoseconds.
         */
        void stop(int64_t timestamp);

        /**
         * @brief Register a callback invoked before stats are dumped.
         *
         * Used by clock domains to snapshot their cycle counts.
         */
        void register_pre_dump(std::function<void()> callback);

        /**
         * @brief Register a callback invoked when stats collection starts.
         *
         * Used by clock domains to snapshot their cycle count at start time.
         */
        void register_start_callback(std::function<void()> callback);

        /**
         * @brief Get the duration of the stats window in picoseconds.
         *
         * Returns stop - start if both are set, 0 otherwise.
         */
        int64_t get_duration_ps() const;



    // public for dump helpers
    public:
        struct StatEntry
        {
            std::string path;
            StatCommon *stat;
            std::string desc;
        };

    private:
        void dump_flat(FILE *file);
        void dump_grouped(FILE *file);

        std::vector<StatEntry> entries;
        vp::Block *top = nullptr;
        bool enabled = false;

        bool opt_raw = false;        // --stats-raw: machine-readable flat format
        bool opt_flat = false;       // --stats-flat: human-readable but no grouping
        int64_t start_timestamp = -1;
        int64_t stop_timestamp = -1;
        bool first_dump = true;
        std::vector<std::function<void()>> pre_dump_callbacks;
        std::vector<std::function<void()>> start_callbacks;
    };

}  // namespace vp
