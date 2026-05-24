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
#include <dlfcn.h>
#include <fstream>
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
 * Verilator XML metadata loader. Verilator's VCD output drops all signals
 * to type "wire" with no direction info, but its --xml-output describes
 * every variable with `dir`, `vartype`, and `param` attributes. We load
 * the XML once at plugin startup, build a flat
 *   "TOP/<scope>/<...>/<varname>" -> { dir, type }
 * map, and have the VCD parser consult it to populate the Dir/Type
 * columns in the GUI.
 *
 * The XML is a few MB even for medium designs; the scanner is hand-written
 * (no XML lib deps) and relies on Verilator emitting one element per line
 * for <cell>, <module>, <package>, <var>. Quotes inside attribute values
 * aren't allowed in XML so the simple " key=\"value\"" pattern is robust.
 * --------------------------------------------------------------------------- */
struct VlXmlVarMeta { std::string dir; std::string vcd_type; };

inline std::string vl_xml_attr(const std::string &line, const char *key)
{
    std::string pat = std::string(" ") + key + "=\"";
    size_t k = line.find(pat);
    if (k == std::string::npos) return "";
    size_t v = k + pat.size();
    size_t e = line.find('"', v);
    if (e == std::string::npos) return "";
    return line.substr(v, e - v);
}

inline std::string vl_xml_map_dir(const std::string &dir)
{
    if (dir == "input")  return "I";
    if (dir == "output") return "O";
    if (dir == "inout")  return "IO";
    return "";
}

struct VlXmlMetadata
{
    /* Full hierarchical-path → (dir, type) map. Built from <cell> tree +
       <module>/<package> vars. Primary lookup. */
    std::unordered_map<std::string, VlXmlVarMeta> by_path;
    /* var-name → "parm" fallback. SystemVerilog package parameters get
       inlined per importing module in the XML but show up under a
       synthetic TOP/<pkg-name>/ scope in Verilator's VCD output. The path
       lookup misses; the name lookup recovers the "parm" type at least. */
    std::unordered_map<std::string, std::string> param_by_name;
};

inline bool vl_xml_load(const std::string &xml_path, VlXmlMetadata &out)
{
    std::ifstream f(xml_path);
    if (!f) return false;

    struct VarLine { std::string name, dir, vartype; bool param; };

    /* Pass 1: scan modules and packages, collect their var lists. */
    std::unordered_map<std::string, std::vector<VarLine>> mod_vars;
    std::unordered_map<std::string, std::vector<VarLine>> pkg_vars;
    std::string current_mod, current_pkg;
    std::string line;
    while (std::getline(f, line))
    {
        size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos) continue;
        const char *p = line.c_str() + s;
        if (std::strncmp(p, "<module ", 8) == 0)
        {
            current_mod = vl_xml_attr(line, "name");
        }
        else if (std::strncmp(p, "</module>", 9) == 0)
        {
            current_mod.clear();
        }
        else if (std::strncmp(p, "<package ", 9) == 0)
        {
            current_pkg = vl_xml_attr(line, "name");
        }
        else if (std::strncmp(p, "</package>", 10) == 0)
        {
            current_pkg.clear();
        }
        else if (std::strncmp(p, "<var ", 5) == 0)
        {
            VarLine v;
            v.name    = vl_xml_attr(line, "name");
            v.dir     = vl_xml_attr(line, "dir");
            v.vartype = vl_xml_attr(line, "vartype");
            /* SV uses both `parameter` (param="true") and `localparam`
               (localparam="true"). Treat them the same for the Type
               column. */
            v.param   = (vl_xml_attr(line, "param") == "true"
                      || vl_xml_attr(line, "localparam") == "true");
            if (v.name.empty()) continue;
            if (!current_mod.empty()) mod_vars[current_mod].push_back(std::move(v));
            else if (!current_pkg.empty()) pkg_vars[current_pkg].push_back(std::move(v));
        }
    }

    /* Pass 2: walk <cell> tree to flatten module-var into instance paths. */
    f.clear(); f.seekg(0);
    while (std::getline(f, line))
    {
        size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos) continue;
        const char *p = line.c_str() + s;
        if (std::strncmp(p, "<cell ", 6) != 0) continue;
        std::string hier = vl_xml_attr(line, "hier");
        std::string subm = vl_xml_attr(line, "submodname");
        if (hier.empty() || subm.empty()) continue;
        /* "a.b.c" -> "TOP/a/b/c" so the key matches what the VCD parser
           builds for register_var. */
        std::string base = "TOP/";
        for (char c : hier) base += (c == '.') ? '/' : c;
        auto it = mod_vars.find(subm);
        if (it == mod_vars.end()) continue;
        for (auto &v : it->second)
        {
            std::string type_str = v.param ? "parm" : v.vartype;
            if (type_str == "integer") type_str = "int";
            out.by_path[base + "/" + v.name] =
                { vl_xml_map_dir(v.dir), type_str };
            if (v.param) out.param_by_name[v.name] = "parm";
        }
    }

    /* Packages live directly under TOP/<pkg>/<var> in Verilator's VCD. */
    for (auto &kv : pkg_vars)
    {
        for (auto &v : kv.second)
        {
            std::string type_str = v.param ? "parm" : v.vartype;
            if (type_str == "integer") type_str = "int";
            out.by_path["TOP/" + kv.first + "/" + v.name] =
                { vl_xml_map_dir(v.dir), type_str };
            if (v.param) out.param_by_name[v.name] = "parm";
        }
    }

    return true;
}

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
                                               ps.path.c_str(), ps.width,
                                               ps.desc.c_str());
            if (h == nullptr) continue;
            /* Verilator dedups internally-equivalent signals to a single
               VCD code (e.g. all 8 cores' clk_i share '?'). Each $var line
               still declares a distinct hierarchical path, and the host
               wants a distinct vp::Signal per path — so push every handle
               into the vector for this code and fan out on value-change. */
            codes_[ps.code].push_back(SignalRef{h, ps.width});
        }
        pending_.clear();
        pending_.shrink_to_fit();
        callbacks_armed_ = true;
    }

    size_t signal_count() const { return codes_.size(); }

    /* Replace the metadata populated from Verilator's XML (loaded once
       at plugin startup). Overrides the type/direction extracted from
       the bare VCD $var line, which is always just "wire". */
    void xml_meta_set(VlXmlMetadata meta)
    {
        xml_meta_ = std::move(meta);
    }

private:
    enum Phase { HEADER, DATA };
    // `desc` is the "<dir>|<type>" string forwarded to host_cb_->reg_logical
    // so the GUI can populate the Dir/Type columns in the signal browser.
    struct PendingSignal { std::string path; std::string code; int width; std::string desc; };
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
            // VCD $var line: "$var <type> <width> <code> <name> $end".
            // We capture the type token so the host can populate the Type
            // column in the GUI (direction isn't in VCD; only FST has it).
            const char *type_start = q;
            while (*q && *q != ' ' && *q != '\t') q++;
            std::string type_str(type_start, q - type_start);
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
            register_var(width, code, name, type_str);
        }
        else if (std::strncmp(p, "$enddefinitions", 15) == 0)
        {
            phase_ = DATA;
        }
    }

    void register_var(int width, const std::string &code,
                      const std::string &name,
                      const std::string &type_str)
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

        /* Look up the rich (dir, type) pair in the XML metadata map. The
           map's keys match the path format we just built. If no entry
           exists (XML wasn't loaded, or signal not in XML), fall back to
           the VCD $var type token (which Verilator emits as "wire" for
           everything). Format: "<dir>|<type>" — same as fst_dumper, so
           the GUI's signal browser parses both with the same code. */
        std::string desc;
        auto it = xml_meta_.by_path.find(path);
        if (it != xml_meta_.by_path.end())
        {
            desc = it->second.dir + "|" + it->second.vcd_type;
        }
        else
        {
            /* Fallback: VCD path didn't match XML (typical for SV package
               parameters that VCD groups under a synthetic TOP/<pkg>/
               scope). Look up by the trailing var name — if it's a known
               parameter elsewhere in the design, we at least get "parm"
               in the Type column. */
            std::string ty = type_str;
            if (ty == "parameter") ty = "parm";
            else if (ty == "integer") ty = "int";
            /* Strip "[hi:lo]" suffix from name when looking up params. */
            std::string base_name = name;
            size_t bracket = base_name.find('[');
            if (bracket != std::string::npos) base_name.resize(bracket);
            auto pit = xml_meta_.param_by_name.find(base_name);
            if (pit != xml_meta_.param_by_name.end()) ty = pit->second;
            desc = "|" + ty;
        }

        pending_.push_back({std::move(path), code, width, std::move(desc)});
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
        for (auto &ref : it->second)
        {
            host_cb_->push_logical(host_cb_->ctx, ref.handle,
                                   value, current_time_ps_);
        }
    }

    VlHostCb *host_cb_;
    int64_t ps_per_vcd_tick_;
    std::string bytes_;
    Phase phase_ = HEADER;
    std::vector<std::string> scope_stack_;
    std::vector<PendingSignal> pending_;
    std::unordered_map<std::string, std::vector<SignalRef>> codes_;
    /* Per-path (dir, type) from Verilator's --xml-output, loaded at
       plugin startup. Empty when no XML is available. */
    VlXmlMetadata xml_meta_;
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
            /* Load Verilator's --xml-output BEFORE tfp->open() runs the
               VCD header processing — that's what triggers MemoryVcdFile
               to parse $var lines and pre-record each signal, so the
               metadata map needs to be in place by then. Look for
               design.xml next to this .so (dladdr on any header symbol
               gives the .so path). Silent fallback to VCD-only when the
               XML is missing or unreadable. */
            Dl_info info{};
            if (dladdr(reinterpret_cast<void *>(&vl_xml_load), &info)
                && info.dli_fname)
            {
                std::string so_path = info.dli_fname;
                size_t slash = so_path.rfind('/');
                std::string xml_path = (slash == std::string::npos)
                    ? std::string("design.xml")
                    : so_path.substr(0, slash + 1) + "design.xml";
                VlXmlMetadata meta;
                if (vl_xml_load(xml_path, meta))
                {
                    std::fprintf(stderr,
                        "[SIM] VCD trace metadata loaded from %s "
                        "(%zu paths, %zu param names)\n",
                        xml_path.c_str(), meta.by_path.size(),
                        meta.param_by_name.size());
                    vcd_sink->xml_meta_set(std::move(meta));
                }
            }
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
