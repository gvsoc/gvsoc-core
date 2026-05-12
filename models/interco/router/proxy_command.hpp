// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Shared proxy `mem_read`/`mem_write` handler for the io_v2 routers.
 *
 * The four v2 router variants (untimed/bandwidth/backpressure/beat) each
 * supply their own request dispatch (mapping lookup + forward) and their
 * own InFlight/retry tracking. This header gives them a single
 * `handle_proxy_command` that parses the proxy command, drives the
 * dispatch + wait state machine, and ships the reply payload — so each
 * variant boils down to a small `handle_command` glue and a
 * `dispatch_proxy_req` method.
 *
 * Async support:
 *   - DONE   : returned synchronously, reply shipped immediately.
 *   - GRANTED: the variant's dispatch installed an InFlight whose
 *              `proxy_waiter` is the ProxyWaiter we passed in; we release
 *              the engine lock, wait on the waiter's cv, re-acquire.
 *              The variant's resp callback notifies the waiter.
 *   - DENIED : we register the waiter as the "next-retry" target on the
 *              variant, release the engine lock, wait, re-acquire, then
 *              re-dispatch. The variant's retry callback notifies the
 *              waiter.
 *
 * Note on locking: the proxy session thread already holds the engine
 * lock (`gv::Controller::engine_lock()`) when handle_command is called.
 * We release it whenever we need the engine to actually run cycles to
 * generate a resp or clear a deny condition, then re-acquire before
 * returning to the proxy. The session unconditionally calls
 * `engine_unlock()` once handle_command returns, so the lock count must
 * be balanced on every path.
 */

#pragma once

#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/proxy.hpp>
#include <vp/controller.hpp>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>

namespace vp_router_v2_proxy
{

// Synchronization object exchanged between handle_proxy_command and the
// variant's resp_muxed / retry_muxed callbacks.
struct ProxyWaiter
{
    enum State { Pending, Replied, Retry };

    std::mutex mu;
    std::condition_variable cv;
    State state = Pending;
};

// Per-variant dispatch: walk the mapping, forward to the matching output
// port, return DONE / GRANTED / DENIED. On GRANTED the variant MUST
// install an InFlight whose `proxy_waiter` is `waiter` so its resp
// callback can signal it back.
using DispatchFn = std::function<vp::IoReqStatus(vp::IoReq *, ProxyWaiter *)>;

// Per-variant retry hook: store `waiter` (nullptr to clear) so the
// variant's retry callback wakes it when a downstream becomes ready.
using SetRetryWaiterFn = std::function<void(ProxyWaiter *)>;

// Wait helper, defined here so each variant gets the same lock dance.
inline void wait_for_state(gv::Controller *launcher, ProxyWaiter *w,
                            ProxyWaiter::State target)
{
    launcher->engine_unlock();
    {
        std::unique_lock<std::mutex> lk(w->mu);
        w->cv.wait(lk, [&] { return w->state == target; });
    }
    launcher->engine_lock();
}

// Shared `handle_command` implementation. Returns the reply string in
// the proxy protocol format ("err=0" / "err=1").
inline std::string handle_proxy_command(
    gv::GvProxy *proxy,
    gv::Controller *launcher,
    FILE *req_file,
    FILE *reply_file,
    const std::vector<std::string> &args,
    const std::string &cmd_req,
    DispatchFn dispatch,
    SetRetryWaiterFn set_retry_waiter)
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

    vp::IoReq req(addr, buffer.data(), size, is_write);
    ProxyWaiter waiter;
    bool replied = false;

    while (!replied)
    {
        {
            std::lock_guard<std::mutex> lk(waiter.mu);
            waiter.state = ProxyWaiter::Pending;
        }

        vp::IoReqStatus status = dispatch(&req, &waiter);

        if (status == vp::IO_REQ_DONE)
        {
            replied = true;
        }
        else if (status == vp::IO_REQ_GRANTED)
        {
            // Variant installed InFlight pointing to `waiter` so its
            // resp_muxed can notify us. Let the engine run.
            wait_for_state(launcher, &waiter, ProxyWaiter::Replied);
            replied = true;
        }
        else // IO_REQ_DENIED
        {
            // Variant did not take ownership of `req`; arm the retry
            // hook so the next retry_muxed wakes us, then re-dispatch.
            set_retry_waiter(&waiter);
            wait_for_state(launcher, &waiter, ProxyWaiter::Retry);
            set_retry_waiter(nullptr);
        }
    }

    if (req.get_resp_status() != vp::IO_RESP_OK)
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

// Variant-side helper: notify a ProxyWaiter that the response arrived.
inline void notify_replied(ProxyWaiter *w)
{
    {
        std::lock_guard<std::mutex> lk(w->mu);
        w->state = ProxyWaiter::Replied;
    }
    w->cv.notify_one();
}

// Variant-side helper: notify a ProxyWaiter that a retry can be issued.
inline void notify_retry(ProxyWaiter *w)
{
    {
        std::lock_guard<std::mutex> lk(w->mu);
        w->state = ProxyWaiter::Retry;
    }
    w->cv.notify_one();
}

}  // namespace vp_router_v2_proxy
