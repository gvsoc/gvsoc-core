// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Shared debug_mem_regions() implementation for the io_v2 routers.
 *
 * Walks one router variant's compiled mappings and emits the flat backdoor
 * regions (see vp/debug_mem.hpp) behind each of them: the component bound
 * to a mapping's output port is resolved through the port's final bindings
 * and recursed into with the mapping's address translation applied.
 * Mappings whose target does not implement vp::DebugMemIf are skipped,
 * leaving a hole in the map (accesses there fail instead of hanging).
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

#include <vp/vp.hpp>
#include <vp/debug_mem.hpp>
#include <interco/router_v2/router_config.hpp>

namespace vp_router_v2_debug
{

// Saturating end-of-window, the root window being [0, ~0ULL)
inline uint64_t window_end(uint64_t base, uint64_t size)
{
    uint64_t end = base + size;
    return end < base ? UINT64_MAX : end;
}

// Emit the flat regions behind every mapping of one router variant.
// `get_port` returns the outbound master port of mapping `id` (the variants
// store it as OutputPort::itf or OutputPort::bus). Parameters past `regions`
// are the debug_mem_regions() ones, forwarded unchanged from the router.
inline void collect_regions(const RouterConfig &cfg, int error_id,
    const std::function<vp::MasterPort *(int)> &get_port,
    std::vector<vp::DebugMemRegion> &regions,
    uint64_t local_base, uint64_t window_size, uint64_t entry_base, int depth)
{
    if (depth >= vp::DebugMemIf::MAX_DEPTH)
    {
        return;
    }

    uint64_t win_end = window_end(local_base, window_size);

    // Two passes: explicit windows first, size==0 catch-alls second, so the
    // explicit ones win under DebugMemMap's first-inserted-wins clipping.
    for (int pass = 0; pass < 2; pass++)
    {
        for (int id = 0; id < (int)cfg.mappings_count; id++)
        {
            const RouterMapping &m = cfg.mappings[id];
            bool catch_all = m.size == 0;
            if (catch_all != (pass == 1) || id == error_id)
            {
                continue;
            }

            // Intersect the mapping with the requested window
            uint64_t map_base = (uint64_t)m.base;
            uint64_t map_end = catch_all ?
                UINT64_MAX : window_end(map_base, (uint64_t)m.size);
            uint64_t i_base = std::max(map_base, local_base);
            uint64_t i_end = std::min(map_end, win_end);
            if (i_base >= i_end)
            {
                continue;
            }

            // Resolve the component bound behind the output port
            std::vector<vp::SlavePort *> finals = get_port(id)->get_final_ports();
            if (finals.empty() || finals[0]->get_owner() == nullptr)
            {
                if (getenv("GV_DEBUG_MEM_VERBOSE"))
                {
                    fprintf(stderr, "DEBUG_MEM skip mapping %s: no final port\n", m.name);
                }
                continue;
            }
            vp::DebugMemIf *child = finals[0]->get_owner()->debug_mem_if();
            if (child == nullptr)
            {
                if (getenv("GV_DEBUG_MEM_VERBOSE"))
                {
                    fprintf(stderr, "DEBUG_MEM skip mapping %s: owner %s has no debug_mem_if\n",
                        m.name, finals[0]->get_owner()->get_path().c_str());
                }
                continue;
            }

            child->debug_mem_regions(regions,
                i_base - m.remove_offset + m.add_offset,
                i_end - i_base,
                entry_base + (i_base - local_base),
                depth + 1);
        }
    }
}

}  // namespace vp_router_v2_debug
