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

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/debug_mem.hpp>
#include <algorithm>
#include <functional>
#include <vector>

namespace vp_router_legacy_debug
{

// One legacy-router mapping, as needed to emit its backdoor regions.
struct DebugMapping
{
    uint64_t base;           // mapping base in the router's address space
    uint64_t size;           // 0 == catch-all (matches everything above base)
    uint64_t remove_offset;  // applied as: out_addr = in_addr - remove + add
    uint64_t add_offset;
    vp::MasterPort *port;     // outbound port for this mapping
};

// Saturating end-of-window, the root window being [0, ~0ULL)
inline uint64_t window_end(uint64_t base, uint64_t size)
{
    uint64_t end = base + size;
    return end < base ? UINT64_MAX : end;
}

// Emit the flat backdoor regions behind every mapping of a legacy router
// (see vp/debug_mem.hpp). Mirrors vp_router_v2_debug::collect_regions but
// works off a plain DebugMapping list since the legacy routers do not have
// a compiled RouterConfig. Mappings whose target does not implement
// vp::DebugMemIf are skipped, leaving a hole (accesses there fail).
inline void collect_regions(const std::vector<DebugMapping> &mappings,
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
        for (const DebugMapping &m : mappings)
        {
            bool catch_all = m.size == 0;
            if (catch_all != (pass == 1) || m.port == nullptr)
            {
                continue;
            }

            uint64_t map_end = catch_all ? UINT64_MAX : window_end(m.base, m.size);
            uint64_t i_base = std::max(m.base, local_base);
            uint64_t i_end = std::min(map_end, win_end);
            if (i_base >= i_end)
            {
                continue;
            }

            std::vector<vp::SlavePort *> finals = m.port->get_final_ports();
            if (finals.empty() || finals[0]->get_owner() == nullptr)
            {
                continue;
            }
            vp::DebugMemIf *child = finals[0]->get_owner()->debug_mem_if();
            if (child == nullptr)
            {
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

}  // namespace vp_router_legacy_debug

class RouterCommon : public vp::Component
{
public:
    RouterCommon(vp::ComponentConf &conf);

    std::string handle_command(gv::GvProxy *proxy, FILE *req_file,
        FILE *reply_file, std::vector<std::string> args, std::string cmd_req) override;

private:
    virtual vp::IoReqStatus handle_req(vp::IoReq *req, int port) = 0;

    vp::IoReq proxy_req;
};