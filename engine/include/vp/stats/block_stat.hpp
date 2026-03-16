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

namespace vp
{
    class Block;
    class StatsEngine;
    class StatCommon;

    /**
     * @brief Per-component interface to the statistics engine.
     *
     * Mirrors the BlockTrace/BlockPower pattern. Lives as a member of
     * vp::Component (not Block, to avoid any ABI sensitivity).
     *
     * register_stat() prepends the component's hierarchical path and
     * forwards to StatsEngine. All methods are no-ops when engine is null
     * (i.e. when --stats is not passed). Layout is fixed (two pointers)
     * regardless of build configuration — no ABI mismatch.
     */
    class BlockStat
    {
    public:
        BlockStat() : top(nullptr), engine(nullptr) {}

        BlockStat(vp::Block *top, vp::StatsEngine *engine)
            : top(top), engine(engine) {}

        /**
         * @brief Register a stat with its name and description.
         *
         * No-op when stats are disabled (engine is null).
         *
         * @param stat  Stat object to register.
         * @param name  Short name appended to component's hierarchical path.
         * @param desc  Human-readable description shown in dump output.
         */
        void register_stat(StatCommon *stat, const std::string &name, const std::string &desc);

        /**
         * @brief Register a bandwidth stat derived from a bytes counter.
         *
         * Links the StatBw to the source StatScalar. The engine will
         * automatically compute bandwidth before dump.
         *
         * @param bw    Bandwidth stat object.
         * @param source  Source bytes counter (must be registered separately).
         * @param name  Short name appended to component's hierarchical path.
         * @param desc  Human-readable description shown in dump output.
         */
        /**
         * @brief Get the underlying engine pointer (may be null).
         */
        StatsEngine *get_engine() { return this->engine; }

    private:
        vp::Block *top;
        vp::StatsEngine *engine;
    };

}  // namespace vp
