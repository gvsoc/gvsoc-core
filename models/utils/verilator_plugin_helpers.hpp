// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Header-only helper for building GVSoC verilator plugins (*.so).
 *
 * Each plugin .so wraps a Verilator-built RTL design behind the C ABI in
 * verilator_plugin.h. ~80 % of the per-design code (incremental VCD-byte
 * parser, plusarg parsing, trace-file vs signal-injection setup, vtable
 * boilerplate) is independent of the DUT — this header captures it.
 *
 * Per-design code shrinks to:
 *
 *   #define GV_TRACE_FST or GV_TRACE_VCD via -D
 *   #include "verilator_plugin_helpers.hpp"
 *   #include "Vmy_dut.h"
 *
 *   struct VlPlugin : gv_vlplugin::PluginCommon<Vmy_dut> {
 *       // any extra state for the stimulus state machine
 *   };
 *
 *   static VlPlugin *vl_open(int argc, const char *const *argv) { ... }
 *   static VlStepResult vl_step(VlPlugin *p)                    { ... }
 *
 *   GV_VERILATOR_PLUGIN_DEFINE(vl_open, vl_step)
 */

#ifndef GVSOC_VERILATOR_PLUGIN_HELPERS_HPP
#define GVSOC_VERILATOR_PLUGIN_HELPERS_HPP

#include "verilator_plugin.h"

#include <verilated.h>

#if defined(GV_TRACE_FST)
#include <verilated_fst_c.h>
#elif defined(GV_TRACE_VCD)
#include <verilated_vcd_c.h>
#elif defined(GV_NO_TRACE)
/* Fast flavour: model is verilated without --trace-* so it has no per-cycle
   trace book-keeping. The shim still compiles against this header but every
   trace-related code path is a no-op. */
#else
#error "Define GV_TRACE_FST, GV_TRACE_VCD or GV_NO_TRACE when compiling a verilator plugin"
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gv_vlplugin {

#if defined(GV_TRACE_FST)
using VlTracer = VerilatedFstC;
#elif defined(GV_TRACE_VCD)
using VlTracer = VerilatedVcdC;
#else  /* GV_NO_TRACE — the only operations the helper invokes on tfp are
          guarded by `tfp != nullptr`, and tfp stays null in this flavour;
          this stub just lets the templated code compile. */
struct VlTracer { void close() {} void flush() {} void dump(int64_t) {} };
#endif

/* GVSoC stores trace values in uint64_t; signals wider than this are dropped
   on the floor (the VCD parser silently skips them). */
constexpr int kMaxSignalWidth = 64;

struct PluginArgs
{
    /* explicit path from +trace=<path>; nullptr → fall back to default */
    const char *trace_file = nullptr;
    bool do_trace = false;
    bool inject = false;
};

inline PluginArgs parse_plusargs(int argc, const char *const *argv)
{
    PluginArgs out;
    for (int i = 1; i < argc; i++)
    {
        if (std::strncmp(argv[i], "+trace=", 7) == 0)
        {
            out.trace_file = argv[i] + 7;
            out.do_trace = true;
        }
        else if (std::strcmp(argv[i], "+trace") == 0)
        {
            out.do_trace = true;
        }
        else if (std::strncmp(argv[i], "+inject_signals", 15) == 0)
        {
            out.inject = true;
        }
    }
    return out;
}

#if defined(GV_TRACE_VCD)

/* ---------------------------------------------------------------------------
 * MemoryVcdFile — a VerilatedVcdFile that intercepts the VCD byte stream
 * Verilator's VCD writer produces, parses it line-by-line, and forwards
 * value changes to the host via VlHostCb instead of writing to disk.
 * --------------------------------------------------------------------------- */
class MemoryVcdFile : public VerilatedVcdFile
{
public:
    /* host_cb: pointer to the live VlHostCb in the user's VlPlugin (the
       parser dereferences this every push, so the storage must outlive
       the MemoryVcdFile — that's true when both are members of VlPlugin).
       ps_per_vcd_tick: 1 if Verilator was built with --timescale 1ps/1ps,
                        1000 if --timescale 1ns/1ns, etc. */
    MemoryVcdFile(VlHostCb *host_cb, int64_t ps_per_vcd_tick)
        : host_cb_(host_cb), ps_per_vcd_tick_(ps_per_vcd_tick) {}

    bool open(const std::string &) VL_MT_UNSAFE override { return true; }
    void close() VL_MT_UNSAFE override {}

    ssize_t write(const char *bufp, ssize_t len) VL_MT_UNSAFE override
    {
        bytes_.append(bufp, (size_t)len);
        process_buffer();
        return len;
    }

    /* Call once after the host has installed VlHostCb (typically from
       on_set_host_callbacks). Until called, value-change lines are
       discarded — there's no host yet to push them to. */
    void arm_callbacks()
    {
        for (auto &ps : pending_)
        {
            VlSignal h = host_cb_->reg_logical(host_cb_->ctx,
                                               ps.path.c_str(), ps.width);
            if (h == nullptr) continue;
            codes_[ps.code] = SignalRef{h, ps.width};
        }
        pending_.clear();
        pending_.shrink_to_fit();
        callbacks_armed_ = true;
    }

    size_t signal_count() const { return codes_.size(); }

private:
    enum Phase { HEADER, DATA };
    struct PendingSignal { std::string path; std::string code; int width; };
    struct SignalRef    { VlSignal handle; int width; };

    void process_buffer()
    {
        size_t pos = 0;
        while (pos < bytes_.size())
        {
            size_t nl = bytes_.find('\n', pos);
            if (nl == std::string::npos) break;
            std::string line = bytes_.substr(pos, nl - pos);
            if (phase_ == HEADER) handle_header_line(line);
            else                  handle_data_line(line);
            pos = nl + 1;
        }
        if (pos > 0) bytes_.erase(0, pos);
    }

    void handle_header_line(const std::string &line)
    {
        size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos) return;
        const char *p = line.c_str() + s;

        if (std::strncmp(p, "$scope", 6) == 0)
        {
            const char *q = p + 6;
            while (*q == ' ' || *q == '\t') q++;
            while (*q && *q != ' ' && *q != '\t') q++;
            while (*q == ' ' || *q == '\t') q++;
            const char *name_start = q;
            while (*q && *q != ' ' && *q != '\t') q++;
            scope_stack_.emplace_back(name_start, q - name_start);
        }
        else if (std::strncmp(p, "$upscope", 8) == 0)
        {
            if (!scope_stack_.empty()) scope_stack_.pop_back();
        }
        else if (std::strncmp(p, "$var", 4) == 0)
        {
            const char *q = p + 4;
            while (*q == ' ' || *q == '\t') q++;
            while (*q && *q != ' ' && *q != '\t') q++;
            while (*q == ' ' || *q == '\t') q++;
            int width = std::atoi(q);
            while (*q && *q != ' ' && *q != '\t') q++;
            while (*q == ' ' || *q == '\t') q++;
            const char *code_start = q;
            while (*q && *q != ' ' && *q != '\t') q++;
            std::string code(code_start, q - code_start);
            while (*q == ' ' || *q == '\t') q++;
            const char *name_start = q;
            while (*q && *q != ' ' && *q != '\t') q++;
            std::string name(name_start, q - name_start);
            register_var(width, code, name);
        }
        else if (std::strncmp(p, "$enddefinitions", 15) == 0)
        {
            phase_ = DATA;
        }
    }

    void register_var(int width, const std::string &code,
                      const std::string &name)
    {
        if (width <= 0 || width > kMaxSignalWidth) return;

        std::string path;
        for (auto &s : scope_stack_)
        {
            if (!path.empty()) path += '/';
            path += s;
        }
        if (!path.empty()) path += '/';

        size_t bracket = name.find('[');
        if (bracket != std::string::npos) path.append(name, 0, bracket);
        else                              path.append(name);

        pending_.push_back({std::move(path), code, width});
    }

    void handle_data_line(const std::string &line)
    {
        if (line.empty()) return;
        if (!callbacks_armed_) return;

        char first = line[0];
        if (first == '#')
        {
            const char *p = line.c_str() + 1;
            int64_t ticks = std::strtoll(p, nullptr, 10);
            current_time_ps_ = ticks * ps_per_vcd_tick_;
            return;
        }
        if (first == '$') return;

        if (first == 'b' || first == 'B')
        {
            size_t sp = line.find(' ', 1);
            if (sp == std::string::npos) return;
            uint64_t v = parse_binary(line.c_str() + 1, line.c_str() + sp);
            std::string code = line.substr(sp + 1);
            push_value(code, v);
        }
        else if (first == 'r' || first == 'R'
              || first == 's' || first == 'S')
        {
            return;
        }
        else
        {
            uint64_t v = (first == '1') ? 1u : 0u;
            std::string code = line.substr(1);
            push_value(code, v);
        }
    }

    static uint64_t parse_binary(const char *p, const char *end)
    {
        uint64_t v = 0;
        for (; p < end; p++)
        {
            v <<= 1;
            if (*p == '1') v |= 1u;
        }
        return v;
    }

    void push_value(const std::string &code, uint64_t value)
    {
        auto it = codes_.find(code);
        if (it == codes_.end()) return;
        host_cb_->push_logical(host_cb_->ctx, it->second.handle,
                               value, current_time_ps_);
    }

    VlHostCb *host_cb_;
    int64_t ps_per_vcd_tick_;
    std::string bytes_;
    Phase phase_ = HEADER;
    std::vector<std::string> scope_stack_;
    std::vector<PendingSignal> pending_;
    std::unordered_map<std::string, SignalRef> codes_;
    bool callbacks_armed_ = false;
    int64_t current_time_ps_ = 0;
};

#endif /* GV_TRACE_VCD */

/* ---------------------------------------------------------------------------
 * PluginCommon — base class for the user's VlPlugin struct. Owns the
 * Verilator context, DUT, tracer, and (in VCD builds) the parser + host
 * callbacks. Provides the boilerplate hooks the GV_VERILATOR_PLUGIN_DEFINE
 * macro wires into the C ABI.
 * --------------------------------------------------------------------------- */
template <typename DutT>
struct PluginCommon
{
    std::unique_ptr<VerilatedContext> ctx;
    std::unique_ptr<DutT> dut;
    VlTracer *tfp = nullptr;

#if defined(GV_TRACE_VCD)
    /* Non-null only when +inject_signals=1; redirects Verilator's VCD
       byte stream into the in-process parser. */
    std::unique_ptr<MemoryVcdFile> vcd_sink;
    VlHostCb host_cb{};
    bool host_cb_set = false;
#endif

    ~PluginCommon()
    {
        if (tfp != nullptr)
        {
            tfp->close();
            delete tfp;
            tfp = nullptr;
        }
    }

    /* Call from vl_open after constructing ctx + dut. Reads +trace and
       +inject_signals plusargs (via parse_plusargs) and configures
       file output or signal injection accordingly. */
    void setup_trace(const PluginArgs &args, int64_t ps_per_vcd_tick,
                     const char *default_trace_file)
    {
#if defined(GV_TRACE_VCD)
        if (args.inject)
        {
            vcd_sink.reset(new MemoryVcdFile(&host_cb, ps_per_vcd_tick));
            tfp = new VerilatedVcdC(vcd_sink.get());
            dut->trace(tfp, 99);
            tfp->open("memory");
            std::fprintf(stderr,
                "[SIM] VCD trace piped to host (signal injection)\n");
        }
        else if (args.do_trace)
        {
            const char *path = args.trace_file ? args.trace_file
                                               : default_trace_file;
            tfp = new VerilatedVcdC();
            dut->trace(tfp, 99);
            tfp->open(path);
            std::fprintf(stderr, "[SIM] VCD tracing to %s\n", path);
        }
#elif defined(GV_TRACE_FST)
        (void)ps_per_vcd_tick;
        if (args.inject)
        {
            std::fprintf(stderr,
                "[SIM] WARNING: +inject_signals=1 ignored (this plugin was "
                "built with --trace-fst; signal injection requires the VCD "
                "plugin)\n");
        }
        if (args.do_trace)
        {
            const char *path = args.trace_file ? args.trace_file
                                               : default_trace_file;
            tfp = new VerilatedFstC();
            dut->trace(tfp, 99);
            tfp->open(path);
            std::fprintf(stderr, "[SIM] FST tracing to %s\n", path);
        }
#else  /* GV_NO_TRACE */
        (void)ps_per_vcd_tick;
        (void)default_trace_file;
        if (args.inject || args.do_trace)
        {
            std::fprintf(stderr,
                "[SIM] WARNING: tracing/injection ignored — fast plugin built "
                "without trace support; rebuild with the FST or inject "
                "flavour to enable tracing\n");
        }
#endif
    }

    /* Wired into the C ABI by GV_VERILATOR_PLUGIN_DEFINE. */

    void on_set_host_callbacks(const VlHostCb *cb)
    {
#if defined(GV_TRACE_VCD)
        if (cb == nullptr) return;
        host_cb = *cb;
        host_cb_set = true;
        if (vcd_sink != nullptr)
        {
            if (tfp != nullptr) tfp->flush();
            vcd_sink->arm_callbacks();
            std::fprintf(stderr,
                "[SIM] VCD trace registered %zu signal(s)\n",
                vcd_sink->signal_count());
        }
#else
        (void)cb;
#endif
    }

    void on_flush()
    {
        if (tfp != nullptr) tfp->flush();
    }

    void on_close()
    {
        if (tfp != nullptr) tfp->flush();
        if (dut) dut->final();
    }
};

} /* namespace gv_vlplugin */

/* ---------------------------------------------------------------------------
 * GV_VERILATOR_PLUGIN_DEFINE — emit the C ABI exports + vtable.
 *
 * The caller must provide, before invoking the macro:
 *   - struct VlPlugin (typically inheriting gv_vlplugin::PluginCommon<DutT>)
 *   - static VlPlugin *vl_open(int argc, const char *const *argv);
 *   - static VlStepResult vl_step(VlPlugin *p);
 * --------------------------------------------------------------------------- */
/* Emits a tiny standalone main() that walks the vtable. The same .o is
   linked both into the per-design .so (where main is an unused exported
   symbol — dlopen ignores it) and into the standalone test binary. */
#define GV_VERILATOR_PLUGIN_STANDALONE_MAIN()                              \
    int main(int argc, char **argv)                                        \
    {                                                                      \
        const VlPluginVtable *vt = gv_verilator_plugin_get();              \
        VlPlugin *p = vt->open(argc,                                       \
            const_cast<const char *const *>(argv));                        \
        if (p == nullptr) {                                                \
            std::fprintf(stderr, "[SIM] plugin open() failed\n");          \
            return 1;                                                      \
        }                                                                  \
        int rc = -1;                                                       \
        while (true) {                                                     \
            VlStepResult r = vt->step(p);                                  \
            if (r.exit_code >= 0) { rc = r.exit_code; break; }             \
        }                                                                  \
        vt->close(p);                                                      \
        return rc;                                                         \
    }

#if defined(GV_TRACE_VCD)
#define GV_VERILATOR_PLUGIN_DEFINE(open_fn, step_fn)                       \
    static void gv_vl_close_(VlPlugin *p)                                  \
    {                                                                      \
        if (p == nullptr) return;                                          \
        p->on_close();                                                     \
        delete p;                                                          \
    }                                                                      \
    static void gv_vl_set_host_cb_(VlPlugin *p, const VlHostCb *cb)        \
    {                                                                      \
        p->on_set_host_callbacks(cb);                                      \
    }                                                                      \
    static void gv_vl_flush_(VlPlugin *p) { p->on_flush(); }               \
    static const VlPluginVtable gv_vl_vtable_ = {                          \
        (open_fn), (step_fn), gv_vl_close_,                                \
        gv_vl_set_host_cb_, gv_vl_flush_,                                  \
    };                                                                     \
    extern "C" const VlPluginVtable *gv_verilator_plugin_get(void)         \
    {                                                                      \
        return &gv_vl_vtable_;                                             \
    }                                                                      \
    GV_VERILATOR_PLUGIN_STANDALONE_MAIN()
#else
#define GV_VERILATOR_PLUGIN_DEFINE(open_fn, step_fn)                       \
    static void gv_vl_close_(VlPlugin *p)                                  \
    {                                                                      \
        if (p == nullptr) return;                                          \
        p->on_close();                                                     \
        delete p;                                                          \
    }                                                                      \
    static const VlPluginVtable gv_vl_vtable_ = {                          \
        (open_fn), (step_fn), gv_vl_close_,                                \
        nullptr, nullptr,                                                  \
    };                                                                     \
    extern "C" const VlPluginVtable *gv_verilator_plugin_get(void)         \
    {                                                                      \
        return &gv_vl_vtable_;                                             \
    }                                                                      \
    GV_VERILATOR_PLUGIN_STANDALONE_MAIN()
#endif

#endif /* GVSOC_VERILATOR_PLUGIN_HELPERS_HPP */
