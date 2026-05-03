// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * GVSoC component that drives a Verilator-built RTL design through a
 * tiny C ABI plugin (see verilator_plugin.h).
 *
 * The component is design-agnostic: the path to the plugin .so is a
 * generator property. The plugin owns the VerilatedContext and all
 * design-specific state; this component just calls open/step/close
 * from the GVSoC engine.
 *
 * Pause/resume "just works": the per-cycle step is wrapped in a
 * permanent ClockEvent. When the GUI pauses the engine, the clock
 * engine stops calling permanent events and Verilator stalls.
 * When the GUI plays again, events resume and Verilator continues.
 *
 * Signal injection: the plugin can opt-in to push signal value changes
 * back to GVSoC via set_host_callbacks. Each registered signal becomes
 * a vp::Signal in the GVSoC trace engine, visible in the gvsoc-gui3
 * timeline. The plugin uses Verilator's VPI cbValueChange to drive the
 * stream.
 */

#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/time/time_event.hpp>
#include <dlfcn.h>
#include <memory>
#include <string>
#include <vector>
#include "verilator_plugin.h"

class VerilatorControl : public vp::Component
{
public:
    VerilatorControl(vp::ComponentConf &config);
    void start() override;
    void reset(bool active) override;
    void stop() override;
    void on_pause() override;

private:
    static void step_handler(vp::Block *_this, vp::TimeEvent *event);

    /* Host-side callbacks plugged into the plugin via set_host_callbacks.
       The plugin uses these to expose signals to the GVSoC trace engine. */
    static VlSignal vl_reg_logical(void *ctx, const char *path, int width);
    static void vl_push_logical(void *ctx, VlSignal sig, uint64_t value,
                                int64_t time_ps);

    vp::Trace trace;
    vp::TimeEvent step_event;

    std::string plugin_path;
    std::string trace_path;
    std::vector<std::string> firmwares;
    bool inject_signals = false;

    /* Storage for the argv array we hand to the plugin. argv_storage owns
       the strings; argv_ptrs holds pointers into it for plugin->open(). */
    std::vector<std::string> argv_storage;
    std::vector<const char *> argv_ptrs;

    void *handle = nullptr;
    const VlPluginVtable *vt = nullptr;
    VlPlugin *design = nullptr;

    /* Signals registered by the plugin. Held by unique_ptr so their
       lifetime matches the component, not the plugin. The cookie returned
       to the plugin (VlSignal) is just `signals[i].get()`. */
    std::vector<std::unique_ptr<vp::Signal<uint64_t>>> signals;
    VlHostCb host_cb;

    /* set_host_callbacks runs only once, on the first reset(false) — see
       reset() for why we can't do it in start(). */
    bool host_callbacks_armed = false;
};

VerilatorControl::VerilatorControl(vp::ComponentConf &config)
    : vp::Component(config),
      step_event(this, &VerilatorControl::step_handler)
{
    traces.new_trace("trace", &this->trace, vp::DEBUG);

    js::Config *js = this->get_js_config();

    js::Config *path_cfg = js->get("plugin_path");
    if (path_cfg == nullptr)
    {
        this->trace.fatal("verilator_control: missing 'plugin_path' property\n");
    }
    this->plugin_path = path_cfg->get_str();

    js::Config *trace_cfg = js->get("trace_path");
    if (trace_cfg != nullptr)
    {
        this->trace_path = trace_cfg->get_str();
    }

    js::Config *fw_cfg = js->get("firmwares");
    if (fw_cfg != nullptr)
    {
        for (auto *elem : fw_cfg->get_elems())
        {
            this->firmwares.push_back(elem->get_str());
        }
    }

    js::Config *inject_cfg = js->get("inject_signals");
    if (inject_cfg != nullptr)
    {
        this->inject_signals = inject_cfg->get_bool();
    }
}

void VerilatorControl::start()
{
    this->handle = dlopen(this->plugin_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (this->handle == nullptr)
    {
        this->trace.fatal("verilator_control: dlopen failed for '%s': %s\n",
            this->plugin_path.c_str(), dlerror());
    }

    auto get_vt = (const VlPluginVtable *(*)())dlsym(this->handle, "gv_verilator_plugin_get");
    if (get_vt == nullptr)
    {
        this->trace.fatal("verilator_control: missing 'gv_verilator_plugin_get' in '%s'\n",
            this->plugin_path.c_str());
    }
    this->vt = get_vt();

    /* Build a Verilator-style argv. argv[0] is conventionally the program name. */
    this->argv_storage.clear();
    this->argv_storage.push_back("gvsoc-verilator");
    for (auto &fw : this->firmwares)
    {
        this->argv_storage.push_back("+firmware=" + fw);
    }
    if (!this->trace_path.empty())
    {
        this->argv_storage.push_back("+trace=" + this->trace_path);
    }
    if (this->inject_signals)
    {
        /* Tells the plugin to pipe Verilator's VCD output through a
           parser that calls our reg_logical/push_logical instead of
           writing to a file. */
        this->argv_storage.push_back("+inject_signals=1");
    }

    this->argv_ptrs.clear();
    for (auto &s : this->argv_storage)
    {
        this->argv_ptrs.push_back(s.c_str());
    }

    this->design = this->vt->open((int)this->argv_ptrs.size(), this->argv_ptrs.data());
    if (this->design == nullptr)
    {
        this->trace.fatal("verilator_control: plugin->open() failed\n");
    }

    this->trace.msg(vp::Trace::LEVEL_INFO,
        "verilator_control: opened plugin '%s' (%zu firmware(s)%s)\n",
        this->plugin_path.c_str(),
        this->firmwares.size(),
        this->trace_path.empty() ? "" : ", tracing enabled");
}

void VerilatorControl::reset(bool active)
{
    if (!active)
    {
        /* reset(false) is the first hook that runs after Db::bind, so the
           trace engine has a vcd_user and any vp::Signal we create here
           will reach the GUI. Doing this in start() would silently drop
           every signal because vcd_user is still NULL there. Run once.
           Skip entirely when inject_signals is false: the per-cycle
           VPI dispatch is heavy and pointless without a GUI consumer. */
        if (this->inject_signals
            && !this->host_callbacks_armed
            && this->vt->set_host_callbacks != nullptr)
        {
            this->host_cb.ctx = this;
            this->host_cb.reg_logical = &VerilatorControl::vl_reg_logical;
            this->host_cb.push_logical = &VerilatorControl::vl_push_logical;
            this->vt->set_host_callbacks(this->design, &this->host_cb);
            this->host_callbacks_armed = true;
        }
        /* Kick off the first step "now" (delta = 0). step_handler
           re-enqueues itself for time_to_next ps later. Note: enqueue
           takes a delta from current time, despite the header docstring
           — same convention utils.fst_dumper uses. */
        this->step_event.enqueue(0);
    }
    else
    {
        this->step_event.cancel();
    }
}

void VerilatorControl::stop()
{
    if (this->design != nullptr && this->vt != nullptr)
    {
        this->vt->close(this->design);
        this->design = nullptr;
    }
    if (this->handle != nullptr)
    {
        dlclose(this->handle);
        this->handle = nullptr;
    }
}

void VerilatorControl::on_pause()
{
    /* Engine just paused — push any buffered VCD bytes through the
       plugin's parser so the GUI sees current signal values. The plugin
       leaves vt->flush NULL when it has nothing to flush. */
    if (this->design != nullptr
        && this->vt != nullptr
        && this->vt->flush != nullptr)
    {
        this->vt->flush(this->design);
    }
}

void VerilatorControl::step_handler(vp::Block *_this, vp::TimeEvent *)
{
    VerilatorControl *t = (VerilatorControl *)_this;
    VlStepResult r = t->vt->step(t->design);
    if (r.exit_code >= 0)
    {
        t->trace.msg(vp::Trace::LEVEL_INFO,
            "verilator_control: design exited with code %d\n", r.exit_code);
        t->time.get_engine()->quit(r.exit_code);
        return;
    }
    /* Re-enqueue at +time_to_next ps. The plugin chooses how far ahead,
       so it can batch multiple internal cycles per host call when
       desirable. enqueue() takes a delta from current time. */
    t->step_event.enqueue(r.time_to_next);
}

VlSignal VerilatorControl::vl_reg_logical(void *ctx, const char *path, int width)
{
    auto *self = static_cast<VerilatorControl *>(ctx);
    if (path == nullptr || width <= 0 || width > 64)
    {
        return nullptr;
    }
    /* Anchor signals at the model's top (our parent Block) rather than at
       this component, so the trace path is "/<rtl-path>" instead of
       "/verilator/<rtl-path>". This lifts the entire RTL hierarchy up to
       the GUI root with no synthetic 'verilator' wrapper in the way. */
    vp::Block *anchor = self->get_parent();
    if (anchor == nullptr) anchor = self;
    auto sig = std::make_unique<vp::Signal<uint64_t>>(
        *anchor, path, width, vp::SignalCommon::ResetKind::None);
    sig->enable();
    auto *raw = sig.get();
    self->signals.push_back(std::move(sig));
    return raw;
}

void VerilatorControl::vl_push_logical(void *ctx, VlSignal sig, uint64_t value,
                                       int64_t time_ps)
{
    if (sig == nullptr) return;
    auto *self = static_cast<VerilatorControl *>(ctx);
    /* time_ps is the absolute Verilator time of the event. Convert to a
       delta relative to the current GVSoC time — vp::Signal::set then
       stamps the event at (now + delta). The delta is typically <=0
       when called from on_pause (events buffered up until pause time,
       flushed retroactively). The trace engine accepts past timestamps
       (Event::dump_* just stores time + delta). */
    int64_t delta = time_ps - self->time.get_time();
    /* int64_t literal disambiguates from the 4-arg set(value, flags, ...). */
    static_cast<vp::Signal<uint64_t> *>(sig)->set(value, (int64_t)0, delta);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new VerilatorControl(config);
}
