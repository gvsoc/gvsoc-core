// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Shared proxy `mem_read`/`mem_write` handler for the io_v2 routers.
 *
 * Proxy accesses are debug accesses: they must complete while the simulation
 * is paused, without advancing time. They are therefore served through the
 * backdoor debug-memory path (vp/debug_mem.hpp) instead of the timed io_v2
 * protocol: the router resolves the access through its flat DebugMemMap down
 * to a direct access into the terminal memory's backing storage.
 *
 * The proxy session thread already holds the engine lock when handle_command
 * is called; the backdoor access is performed inline under it — no engine
 * unlocking, no waiting for simulation cycles. Accesses falling into a hole
 * of the map (no mapping, or a target that does not implement vp::DebugMemIf)
 * return "err=1" instead of hanging.
 */

#pragma once

#include <vp/vp.hpp>
#include <vp/debug_mem.hpp>
#include <vp/proxy.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace vp_router_v2_proxy
{

// Shared `handle_command` implementation. Returns the reply string in the
// proxy protocol format ("err=0" / "err=1").
inline std::string handle_proxy_command(
    gv::GvProxy *proxy,
    FILE *req_file,
    FILE *reply_file,
    const std::vector<std::string> &args,
    const std::string &cmd_req,
    vp::DebugMemIf *dbg)
{
    if (args.size() < 3 ||
        (args[0] != "mem_read" && args[0] != "mem_write"))
    {
        return "err=1";
    }

    bool is_write = args[0] == "mem_write";
    uint64_t addr = (uint64_t)std::strtoll(args[1].c_str(), nullptr, 0);
    uint64_t size = (uint64_t)std::strtoll(args[2].c_str(), nullptr, 0);
    std::vector<uint8_t> buffer(size);
    int error = 0;

    if (is_write)
    {
        size_t read_size = std::fread(buffer.data(), 1, size, req_file);
        if (read_size != size)
        {
            error = 1;
        }
    }

    if (dbg->debug_mem_access(addr, buffer.data(), size, is_write))
    {
        error |= 1;
    }

    if (!is_write)
    {
        if (proxy->send_payload(reply_file, cmd_req, buffer.data(), (int)size))
        {
            error |= 1;
        }
    }

    return "err=" + std::to_string(error);
}

}  // namespace vp_router_v2_proxy
