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
 *
 * A v2 plugin (gv_verilator_plugin_get_v2) registers every signal of the
 * design but only produces values for the enabled ones: its signals are
 * registered at start() without being enabled, so the trace engine declares
 * them to the GUI (signal browser), and the GUI enables the ones added to
 * the timeline. Enable changes are forwarded to the plugin before each step.
 * A v2 plugin is also stepped with step_until: it simulates up to the next
 * event of the rest of the platform (e.g. the end of a run-for-time or cycle
 * step) and returns earlier when the engine is asked to stop (GUI stop
 * button), so the design never runs ahead of GVSoC's time.
 */

#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <vp/time/time_event.hpp>
#include <dlfcn.h>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "verilator_plugin.h"

// A vp::Signal whose enable state (set by the GUI's subscriptions) the
// component can read, to forward it to a v2 plugin.
class PluginSignal : public vp::Signal<uint64_t>
{
public:
    using vp::Signal<uint64_t>::Signal;
    bool active() { return this->event.get_event_active(); }
};

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
    static int should_stop(void *engine);

    /* Host-side callbacks plugged into the plugin via set_host_callbacks.
       The plugin uses these to expose signals to the GVSoC trace engine. */
    static VlSignal vl_reg_logical(void *ctx, const char *path, int width,
                                   const char *description);
    static void vl_push_logical_flags(void *ctx, VlSignal sig, uint64_t value,
                                      uint64_t flags, int64_t time_ps);
    static void vl_push_logical(void *ctx, VlSignal sig, uint64_t value,
                                int64_t time_ps);

    vp::Trace trace;
    vp::TimeEvent step_event;

    std::string plugin_path;
    std::string trace_path;
    std::vector<std::string> firmwares;
    std::vector<std::string> plusargs;
    bool inject_signals = false;

    /* Storage for the argv array we hand to the plugin. argv_storage owns
       the strings; argv_ptrs holds pointers into it for plugin->open(). */
    std::vector<std::string> argv_storage;
    std::vector<const char *> argv_ptrs;

    void *handle = nullptr;
    const VlPluginVtable *vt = nullptr;
    const VlPluginVtableV2 *vt2 = nullptr;  // non-null for a v2 plugin
    VlPlugin *design = nullptr;

    /* Signals registered by the plugin. Held by unique_ptr so their
       lifetime matches the component, not the plugin. The cookie returned
       to the plugin (VlSignal) is just `signals[i].get()`. */
    std::vector<std::unique_ptr<PluginSignal>> signals;
    /* v2: last enable state forwarded to the plugin, per signal. */
    std::vector<bool> signal_enabled;
    void arm_host_callbacks();
    void forward_enables();
    /* Owning store for per-signal description strings. vp::Event keeps the
       description as a non-owning const char*, so we hold the std::string
       here for the component's lifetime. std::deque (not std::vector) so
       push_back never invalidates the c_str() pointers we already handed
       out to earlier vp::Signal::description_set calls. */
    std::deque<std::string> signal_descriptions;
    VlHostCb host_cb;
    VlHostCbV3 host_cb3;  /* v3 plugins: host_cb and push_logical_flags */

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

    /* Free-form plusarg pass-through: each element in the `plusargs`
       array is appended verbatim to the plugin's argv. Target files
       use this to plumb design-specific options (e.g. audio source /
       sink specs) without modifying this component. */
    js::Config *plusargs_cfg = js->get("plusargs");
    if (plusargs_cfg != nullptr)
    {
        for (auto *elem : plusargs_cfg->get_elems())
        {
            this->plusargs.push_back(elem->get_str());
        }
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

    auto get_vt2 = (const VlPluginVtableV2 *(*)())dlsym(this->handle, "gv_verilator_plugin_get_v2");
    if (get_vt2 != nullptr)
    {
        this->vt2 = get_vt2();
        this->vt = &this->vt2->base;
    }
    else
    {
        auto get_vt = (const VlPluginVtable *(*)())dlsym(this->handle, "gv_verilator_plugin_get");
        if (get_vt == nullptr)
        {
            this->trace.fatal("verilator_control: missing 'gv_verilator_plugin_get' in '%s'\n",
                this->plugin_path.c_str());
        }
        this->vt = get_vt();
    }

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
    /* Free-form plusargs from the `plusargs` property. */
    for (auto &arg : this->plusargs)
    {
        this->argv_storage.push_back(arg);
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

    /* v2: register the signals now, disabled. The trace engine starts after
       the components and declares every existing event to the GUI then. */
    if (this->vt2 != nullptr && this->inject_signals)
    {
        this->arm_host_callbacks();
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
        if (this->inject_signals)
        {
            this->arm_host_callbacks();
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

void VerilatorControl::arm_host_callbacks()
{
    if (this->host_callbacks_armed || this->vt->set_host_callbacks == nullptr)
    {
        return;
    }
    this->host_cb.ctx = this;
    this->host_cb.reg_logical = &VerilatorControl::vl_reg_logical;
    this->host_cb.push_logical = &VerilatorControl::vl_push_logical;
    if (this->vt2 != nullptr && this->vt2->version >= 3 && this->vt2->set_host_callbacks_v3 != nullptr)
    {
        /* v3: 4-state values, X and Z bits as flags. */
        this->host_cb3.base = this->host_cb;
        this->host_cb3.push_logical_flags = &VerilatorControl::vl_push_logical_flags;
        this->vt2->set_host_callbacks_v3(this->design, &this->host_cb3);
    }
    else
    {
        this->vt->set_host_callbacks(this->design, &this->host_cb);
    }
    this->host_callbacks_armed = true;
    this->signal_enabled.assign(this->signals.size(), false);
}

// v2: tell the plugin which signals the GUI enabled or disabled since the
// last step, so that it produces the values of the shown signals only.
void VerilatorControl::forward_enables()
{
    if (this->vt2 == nullptr || !this->host_callbacks_armed || this->vt2->signal_enabled == nullptr)
    {
        return;
    }
    for (size_t i = 0; i < this->signals.size(); i++)
    {
        bool active = this->signals[i]->active();
        if (active != this->signal_enabled[i])
        {
            this->signal_enabled[i] = active;
            this->vt2->signal_enabled(this->design, this->signals[i].get(), active);
        }
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
    this->forward_enables();
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
    t->forward_enables();
    if (t->vt2 != nullptr && t->vt2->step_until != nullptr)
    {
        vp::TimeEngine *engine = t->time.get_engine();
        VlStepUntilResult r = t->vt2->step_until(t->design, engine->next_event_time_get(),
            &VerilatorControl::should_stop, engine);
        /* The design was simulated up to reached_ps, never past the next event
           of the rest of the platform: that is now the platform time. */
        engine->update(r.reached_ps);
        if (r.exit_code >= 0)
        {
            t->trace.msg(vp::Trace::LEVEL_INFO,
                "verilator_control: design exited with code %d\n", r.exit_code);
            engine->quit(r.exit_code);
            return;
        }
        if (r.next_ps >= 0)
        {
            t->step_event.enqueue(r.next_ps - engine->get_time());
        }
        return;
    }
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

int VerilatorControl::should_stop(void *engine)
{
    return static_cast<vp::TimeEngine *>(engine)->stop_requested();
}

VlSignal VerilatorControl::vl_reg_logical(void *ctx, const char *path, int width,
                                          const char *description)
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
    auto sig = std::make_unique<PluginSignal>(
        *anchor, path, width, vp::SignalCommon::ResetKind::None);
    /* Forward the "<dir>|<type>" metadata string the plugin built from the
       VCD $var line so the GUI's signal browser populates Dir/Type. We own
       a copy because vp::Event stores description as a non-owning const
       char* and the plugin frees its own copy after arm_callbacks. */
    if (description != nullptr && description[0] != '\0')
    {
        self->signal_descriptions.emplace_back(description);
        sig->description_set(self->signal_descriptions.back().c_str());
    }
    /* A v1 plugin pushes every signal: enable them all. A v2 plugin's signals
       are enabled by the GUI when shown (see forward_enables). */
    if (self->vt2 == nullptr)
    {
        sig->enable();
    }
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

void VerilatorControl::vl_push_logical_flags(void *ctx, VlSignal sig, uint64_t value,
                                             uint64_t flags, int64_t time_ps)
{
    if (sig == nullptr) return;
    auto *self = static_cast<VerilatorControl *>(ctx);
    int64_t delta = time_ps - self->time.get_time();
    /* Per bit: flag 1 is X (value 0) or Z (value 1). */
    static_cast<vp::Signal<uint64_t> *>(sig)->set(value, flags, (int64_t)0, delta);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new VerilatorControl(config);
}
