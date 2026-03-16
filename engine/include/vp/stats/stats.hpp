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

#include <cstdint>
#include <cstdio>
#include <string>

namespace vp
{
    class StatsEngine;

#ifdef CONFIG_GVSOC_STATS_ACTIVE

    /**
     * @brief Base class for all statistic types.
     *
     * Each stat type formats its own value via format_value().
     * The engine handles all structural layout (label, padding, comments).
     */
    class StatCommon
    {
    public:
        virtual ~StatCommon() = default;

        /**
         * @brief Format this stat's value as a string.
         *
         * @param raw  If true, return a machine-readable number.
         *             If false, return a human-readable representation.
         */
        virtual std::string format_value(bool raw) const = 0;

        virtual void reset() = 0;

        /**
         * @brief Called by the engine when this stat is registered.
         *
         * Allows derived classes to store the engine pointer for
         * self-registering callbacks (e.g. pre-dump).
         */
        virtual void registered(StatsEngine *engine) {}
    };

    /**
     * @brief Scalar counter statistic (uint64_t).
     */
    class StatScalar : public StatCommon
    {
    public:
        StatScalar() : value(0) {}

        inline void operator++() { value++; }
        inline void operator++(int) { value++; }
        inline void operator+=(uint64_t v) { value += v; }
        inline void set(uint64_t v) { value = v; }
        inline uint64_t get() const { return value; }

        void reset() override { value = 0; }

        std::string format_value(bool raw) const override
        {
            return std::to_string(this->value);
        }

    private:
        uint64_t value;
    };

    /**
     * @brief Bandwidth statistic derived from a bytes counter.
     *
     * Register like any other stat with register_stat(), then call
     * set_source() to link to a bytes counter. Bandwidth is computed
     * automatically before dump via a self-registered pre-dump callback.
     */
    class StatBw : public StatCommon
    {
    public:
        StatBw() : source(nullptr), engine(nullptr), bw(0) {}

        /**
         * @brief Link this bandwidth stat to a bytes counter.
         *
         * Must be called after register_stat() so the engine is known.
         */
        void set_source(StatScalar *source);

        double get() const { return this->bw; }

        void reset() override { this->bw = 0; }

        void registered(StatsEngine *engine) override;

        std::string format_value(bool raw) const override
        {
            char buf[64];
            if (raw)
            {
                snprintf(buf, sizeof(buf), "%.2f", this->bw);
                return buf;
            }
            if (bw >= 1e9)  { snprintf(buf, sizeof(buf), "%.2f GB/s", bw / 1e9); return buf; }
            if (bw >= 1e6)  { snprintf(buf, sizeof(buf), "%.2f MB/s", bw / 1e6); return buf; }
            if (bw >= 1e3)  { snprintf(buf, sizeof(buf), "%.2f KB/s", bw / 1e3); return buf; }
            snprintf(buf, sizeof(buf), "%.2f B/s", bw);
            return buf;
        }

    private:
        void compute();

        StatScalar *source;
        StatsEngine *engine;
        double bw;
    };

#else  // !CONFIG_GVSOC_STATS_ACTIVE

    class StatCommon
    {
    public:
        virtual ~StatCommon() = default;
        virtual std::string format_value(bool) const { return ""; }
        virtual void reset() {}
        virtual void registered(StatsEngine *) {}
    };

    class StatScalar : public StatCommon
    {
    public:
        inline void operator++() {}
        inline void operator++(int) {}
        inline void operator+=(uint64_t) {}
        inline void set(uint64_t) {}
        inline uint64_t get() const { return 0; }
        void reset() override {}
    };

    class StatBw : public StatCommon
    {
    public:
        void set_source(StatScalar *) {}
        double get() const { return 0; }
        void reset() override {}
    };

#endif  // CONFIG_GVSOC_STATS_ACTIVE

}  // namespace vp
