/*
 * Copyright (C) 2026 ETH Zurich, University of Bologna
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
 *
 * FST dumper: loads a GTKWave FST waveform file at startup, creates a
 * vp::Signal per variable, and injects value changes at their FST timestamps
 * so the GVSoC GUI can display them as a general-purpose FST viewer. Same
 * idea as utils/fsdb_dumper.cpp; the main shape difference is that the FST
 * reader API delivers a single, globally time-sorted value-change stream via
 * fstReaderIterBlocks2, so the per-signal traversal handles + heap that
 * FSDB needs collapse into one flat sorted vector here.
 */

#include <vp/vp.hpp>
#include <vp/signal.hpp>

#include "fstapi.h"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>


class FstDumper;

class FstSignal
{
public:
    fstHandle handle = 0;
    std::string full_name;
    int bit_size = 0;
    // Split choice decided at hierarchy-walk time, consumed by
    // materialize_signals() when the actual vp::Signal<uint64_t> objects are
    // allocated:
    //   element_size == 0            -> single scalar signal
    //   element_size > 0, user_split -> bit_size/element_size children named
    //                                    '/[i]' (WaveLayout element_size)
    //   element_size == 64, !user_split -> ceil(bit_size/64) children named
    //                                    '/[hi:lo]' (auto 64-bit chunks)
    int element_size = 0;
    bool user_split = false;
    vp::Signal<uint64_t> *signal = nullptr;
    std::vector<vp::Signal<uint64_t> *> signal_array;

    // Cache of the last (value, flags) pair emitted to each (sub-)signal so
    // inject_value() can skip redundant vp::Signal::set() calls. FST already
    // only emits VCs when the variable changes, but a wide variable that has
    // been split into element_size chunks will have most chunks unchanged on
    // any given transition; this cache prevents a 128-child split from
    // calling set() on every element on every transition. Initialised to
    // (0, 0) which matches the default vp::Signal reset value, so the first
    // real transition to (0, 0) is also elided correctly.
    uint64_t last_value = 0;
    uint64_t last_flags = 0;
    std::vector<uint64_t> last_elem_values;
    std::vector<uint64_t> last_elem_flags;
};

class FstDumper : public vp::Component
{
public:
    FstDumper(vp::ComponentConf &config);
    ~FstDumper();

    void reset(bool active) override;

private:
    // Walk the FST scope/var hierarchy via fstReaderIterateHier, populating
    // this->signals with metadata only. vp::Signal objects are not allocated
    // here because their enable() call would run before the GUI has bound a
    // vcd_user to the trace engine; the allocation happens in reset(false).
    void walk_hierarchy();

    // Pre-load every value change in time order via fstReaderIterBlocks2.
    // The reader streams VCs through value_change_cb (and the varlen variant
    // for strings, which we ignore); each callback push_back's into all_vcs.
    // This is the heaviest step (O(file size) I/O), so it runs in the
    // constructor and overlaps with GUI startup.
    void load_value_changes();

    static void value_change_cb(void *ud, uint64_t time, fstHandle h,
        const unsigned char *value);
    static void value_change_cb_varlen(void *ud, uint64_t time, fstHandle h,
        const unsigned char *value, uint32_t len);

    // Allocate the vp::Signal<uint64_t> objects for every FST variable.
    // Runs once, from reset(active=false), so the GUI's Vcd_user is already
    // bound by the time signal.enable() registers each event.
    void materialize_signals();

    static void event_handler(vp::Block *__this, vp::TimeEvent *event);
    // time_delay is added to the current simulated time so one handler call
    // can emit VCs that belong to multiple future timestamps; this keeps the
    // number of TimeEvent schedulings bounded even for densely-sampled FSTs.
    void inject_value(FstSignal *sig, const std::string &raw, int64_t time_delay);
    void enqueue_next();

    // Decode an FST per-bit value buffer into one or more 64-bit (value,
    // flags) chunks under the 4-state convention:
    //   (flag=0,val=0/1) -> 0/1, (flag=1,val=0) -> X, (flag=1,val=1) -> Z.
    // Chunk i (0-indexed) covers bits [(i+1)*64-1 : i*64] of the original
    // signal (low-order chunk first). Signals wider than 64 bits therefore
    // produce multiple chunks rather than being truncated. FST encodes one
    // ASCII character per bit, MSB-first; the 9-state SystemVerilog letters
    // (h/l/u/w/-) are folded into 0/1/X.
    void decode_logic_value(const unsigned char *vc_ptr, int bit_size,
        std::vector<uint64_t> &values, std::vector<uint64_t> &flags) const;

    // FST stores its timescale as a base-10 exponent (e.g. -9 = 1 ns,
    // -12 = 1 ps, -15 = 1 fs). Convert to picoseconds-per-tick, the unit
    // GVSoC's trace engine works in.
    int64_t exponent_to_ps_per_tick(signed char exponent);

    fstReaderContext *fst_ctx = nullptr;
    std::vector<std::string> scope_stack;
    std::unordered_map<fstHandle, FstSignal *> signals;
    std::unordered_map<std::string, int> element_size_map;

    // One entry per value change in the file, in monotonically increasing
    // time order (fstReaderIterBlocks2 guarantees this). Pre-allocated in
    // the constructor so the event-driven drain in reset(false) just walks
    // the index. Storing the raw bytes rather than the decoded chunks keeps
    // memory usage in line with the on-disk format and lets decode happen
    // lazily at injection time when the dedup cache also lives.
    struct VcEntry
    {
        uint64_t time_ps;
        fstHandle handle;
        std::string value;
    };
    std::vector<VcEntry> all_vcs;
    size_t vc_index = 0;

    int64_t ps_per_tick = 1;
    bool signals_materialized = false;
    vp::TimeEvent event;
    vp::Trace trace;
};


FstDumper::FstDumper(vp::ComponentConf &config)
    : vp::Component(config), event(this, &FstDumper::event_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    js::Config *es_cfg = this->get_js_config()->get("element_size");
    if (es_cfg != NULL)
    {
        for (auto it : es_cfg->get_childs())
        {
            // Accept keys with or without a leading '/'. The full_name we
            // build from the FST scope stack has no leading '/' but the
            // natural form a user would write (matching GVSoC trace paths)
            // has one, so normalise on lookup.
            std::string key = it.first;
            if (!key.empty() && key.front() == '/')
            {
                key.erase(0, 1);
            }
            this->element_size_map[key] = it.second->get_int();
        }
    }

    std::string fst_path = this->get_js_config()->get_child_str("fst_file");
    if (fst_path == "")
    {
        return;
    }

    this->fst_ctx = fstReaderOpen(fst_path.c_str());
    if (this->fst_ctx == NULL)
    {
        this->trace.fatal("fstReaderOpen failed for %s\n", fst_path.c_str());
        return;
    }

    signed char exponent = fstReaderGetTimescale(this->fst_ctx);
    this->ps_per_tick = this->exponent_to_ps_per_tick(exponent);
    this->trace.msg(vp::Trace::LEVEL_INFO,
        "FST timescale: 10^%d s (%ld ps/tick), variables: %lu\n",
        (int)exponent, (long)this->ps_per_tick,
        (unsigned long)fstReaderGetVarCount(this->fst_ctx));

    this->walk_hierarchy();

    // Mark every variable as wanted, then iterate. This is the heavy step;
    // it overlaps with GUI startup so reset(false) only does cheap signal
    // allocation + register.
    fstReaderSetFacProcessMaskAll(this->fst_ctx);
    this->load_value_changes();
}

FstDumper::~FstDumper()
{
    for (auto &kv : this->signals)
    {
        delete kv.second;
    }
    if (this->fst_ctx)
    {
        fstReaderClose(this->fst_ctx);
        this->fst_ctx = nullptr;
    }
}

void FstDumper::reset(bool active)
{
    if (!active)
    {
        // reset(false) is the earliest component-level hook that runs after
        // Db::bind (from Controller::start -> reset_all(false)), which is
        // the first moment vcd_user is attached to the trace engine. Create
        // the vp::Signal objects here so their enable() registers with the
        // GUI's Db synchronously; do it once even if reset fires again later.
        if (!this->signals_materialized)
        {
            this->materialize_signals();
            this->signals_materialized = true;
        }
        if (this->time.get_time() == 0)
        {
            this->enqueue_next();
        }
    }
}

void FstDumper::walk_hierarchy()
{
    fstReaderIterateHierRewind(this->fst_ctx);
    struct fstHier *h;
    while ((h = fstReaderIterateHier(this->fst_ctx)) != NULL)
    {
        switch (h->htyp)
        {
        case FST_HT_SCOPE:
            this->scope_stack.push_back(
                h->u.scope.name ? std::string(h->u.scope.name) : std::string("?"));
            break;

        case FST_HT_UPSCOPE:
            if (!this->scope_stack.empty())
            {
                this->scope_stack.pop_back();
            }
            break;

        case FST_HT_VAR:
        {
            // Skip alias declarations: FST emits one VAR entry per source
            // declaration but multiple of them can share the same handle
            // (Verilog `wire foo = bar;` etc.). Aliases would create
            // duplicate FstSignal entries that overwrite the primary.
            if (h->u.var.is_alias)
            {
                break;
            }

            // Skip non-logic types this prototype doesn't decode. Real and
            // string variables go through the value-change stream too but
            // their value buffers are not bit-per-character ASCII.
            unsigned char typ = h->u.var.typ;
            if (typ == FST_VT_VCD_REAL || typ == FST_VT_VCD_REAL_PARAMETER ||
                typ == FST_VT_VCD_REALTIME || typ == FST_VT_SV_SHORTREAL ||
                typ == FST_VT_GEN_STRING)
            {
                this->trace.msg(vp::Trace::LEVEL_DEBUG,
                    "Skipping non-logic signal %s (type=%d)\n",
                    h->u.var.name ? h->u.var.name : "?", (int)typ);
                break;
            }

            // Use '/' as the hierarchy separator (GVSoC trace-engine
            // convention) so the Db path hierarchy is proper and every
            // consumer (Timeline tree, SignalBrowser, etc.) sees a structured
            // tree instead of one flat name.
            std::string full_name;
            for (const auto &s : this->scope_stack)
            {
                full_name += s;
                full_name += "/";
            }
            full_name += h->u.var.name ? h->u.var.name : "?";

            int bit_size = (int)h->u.var.length;
            if (bit_size <= 0)
            {
                this->trace.msg(vp::Trace::LEVEL_DEBUG,
                    "Skipping zero-width signal %s\n", full_name.c_str());
                break;
            }

            FstSignal *sig = new FstSignal();
            sig->handle = h->u.var.handle;
            sig->full_name = full_name;
            sig->bit_size = bit_size;

            // Decide the split strategy now so materialize_signals() can just
            // allocate. The actual vp::Signal<uint64_t> objects are created
            // post-bind so signal.enable() registers with a vcd_user that's
            // already attached.
            auto it = this->element_size_map.find(full_name);
            if (it != this->element_size_map.end())
            {
                int elem_size = it->second;
                if (elem_size > 64)
                {
                    this->trace.msg(vp::Trace::LEVEL_WARNING,
                        "element_size=%d for %s exceeds 64; truncating elements\n",
                        elem_size, full_name.c_str());
                    elem_size = 64;
                }
                sig->element_size = elem_size;
                sig->user_split = true;
            }
            else if (bit_size > 64)
            {
                sig->element_size = 64;
                sig->user_split = false;
            }
            // else: element_size stays 0 -> scalar signal in materialize.

            this->signals[sig->handle] = sig;
            break;
        }

        // Hierarchy markers we don't act on (attributes, tree begin/end).
        // Quietly acknowledged so they don't pollute the unhandled log.
        case FST_HT_ATTRBEGIN:
        case FST_HT_ATTREND:
        case FST_HT_TREEBEGIN:
        case FST_HT_TREEEND:
            break;

        default:
            this->trace.msg(vp::Trace::LEVEL_DEBUG,
                "fst_dumper: unhandled hier callback type %d\n", (int)h->htyp);
            break;
        }
    }
}

void FstDumper::load_value_changes()
{
    fstReaderIterBlocks2(this->fst_ctx,
        &FstDumper::value_change_cb,
        &FstDumper::value_change_cb_varlen,
        this,
        nullptr);
    this->trace.msg(vp::Trace::LEVEL_INFO,
        "FST loaded %lu value changes across %lu signals\n",
        (unsigned long)this->all_vcs.size(),
        (unsigned long)this->signals.size());
}

void FstDumper::value_change_cb(void *ud, uint64_t time, fstHandle h,
    const unsigned char *value)
{
    FstDumper *_this = (FstDumper *)ud;
    auto it = _this->signals.find(h);
    if (it == _this->signals.end())
    {
        // Unknown handle (skipped at hierarchy walk, e.g. real type) -- drop.
        return;
    }
    int bit_size = it->second->bit_size;
    // The reader owns `value` only for the duration of this callback, so we
    // must copy. std::string is a convenient byte container with SSO for
    // narrow signals (the typical case).
    _this->all_vcs.push_back({
        (uint64_t)time * (uint64_t)_this->ps_per_tick,
        h,
        std::string((const char *)value, (size_t)bit_size)
    });
}

void FstDumper::value_change_cb_varlen(void *ud, uint64_t time, fstHandle h,
    const unsigned char *value, uint32_t len)
{
    (void)ud; (void)time; (void)h; (void)value; (void)len;
    // Variable-length values (FST_VT_GEN_STRING) are skipped at the
    // hierarchy walk; their VCs still arrive here but have nowhere to go.
}

void FstDumper::materialize_signals()
{
    if (this->fst_ctx == nullptr)
    {
        return;
    }

    // Construct the actual vp::Signal<uint64_t> objects now that Db::bind
    // has attached vcd_user. signal.enable() registers the underlying event
    // synchronously with the GUI's Db; doing this in the constructor (or
    // from start(), which runs inside gvsoc->open() before Db::bind) would
    // take the vcd_user == NULL branch and the events would never appear.
    // The FST value-change load already ran in the constructor, so this
    // pass is only the cheap allocation + register step and runs fast.
    for (auto &kv : this->signals)
    {
        FstSignal *sig = kv.second;
        if (sig->element_size == 0)
        {
            // Scalar (<= 64 bits).
            sig->signal = new vp::Signal<uint64_t>(*this, sig->full_name, sig->bit_size);
            sig->signal->enable();
        }
        else if (sig->user_split)
        {
            // Fixed-width element split (WaveLayout.element_size override).
            // Children named "<full_name>/[i]" so the GUI tree-builder nests
            // them under the wide-signal parent node.
            int nb_elements = sig->bit_size / sig->element_size;
            sig->last_elem_values.assign(nb_elements, 0);
            sig->last_elem_flags.assign(nb_elements, 0);
            for (int i = 0; i < nb_elements; i++)
            {
                auto *s = new vp::Signal<uint64_t>(
                    *this, sig->full_name + "/[" + std::to_string(i) + "]",
                    sig->element_size);
                s->enable();
                sig->signal_array.push_back(s);
            }
        }
        else
        {
            // Auto 64-bit chunks for wide signals. Chunk i covers bit range
            // [(i+1)*64-1 : i*64]; named with the Verilog bit-range syntax.
            int nb_chunks = (sig->bit_size + 63) / 64;
            sig->last_elem_values.assign(nb_chunks, 0);
            sig->last_elem_flags.assign(nb_chunks, 0);
            for (int i = 0; i < nb_chunks; i++)
            {
                int lo = i * 64;
                int hi = std::min(lo + 63, sig->bit_size - 1);
                int width = hi - lo + 1;
                std::string elem_name = sig->full_name + "/[" +
                    std::to_string(hi) + ":" + std::to_string(lo) + "]";
                auto *s = new vp::Signal<uint64_t>(*this, elem_name, width);
                s->enable();
                sig->signal_array.push_back(s);
            }
        }
    }
}

void FstDumper::event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    FstDumper *_this = (FstDumper *)__this;
    int64_t now = _this->time.get_time();

    // Drain a batch of value changes even if they span multiple future FST
    // timestamps: each VC is injected with a `time_delay` so the trace engine
    // records it at its correct simulated time without us having to re-enter
    // the time engine for every distinct timestamp. For RTL dumps with
    // timestamps at every clock tick this cuts the number of TimeEvent
    // scheduling round-trips from millions down to a handful per second.
    constexpr int BATCH_LIMIT = 16384;
    int processed = 0;
    while (_this->vc_index < _this->all_vcs.size() && processed < BATCH_LIMIT)
    {
        VcEntry &e = _this->all_vcs[_this->vc_index];
        int64_t delay = (int64_t)e.time_ps - now;
        if (delay < 0)
        {
            delay = 0;
        }
        auto it = _this->signals.find(e.handle);
        if (it != _this->signals.end())
        {
            _this->inject_value(it->second, e.value, delay);
        }
        _this->vc_index++;
        processed++;
    }

    _this->enqueue_next();
}

void FstDumper::enqueue_next()
{
    if (this->vc_index >= this->all_vcs.size())
    {
        this->time.get_engine()->quit(0);
        return;
    }
    int64_t next = (int64_t)this->all_vcs[this->vc_index].time_ps;
    int64_t now = this->time.get_time();
    int64_t delta = next - now;
    if (delta < 0)
    {
        delta = 0;
    }
    this->event.enqueue(delta);
}

void FstDumper::inject_value(FstSignal *sig, const std::string &raw, int64_t time_delay)
{
    if ((int)raw.size() != sig->bit_size)
    {
        // Defensive: load_value_changes captured `bit_size` bytes per VC
        // because that's what FST advertises for the var. Anything else is
        // a reader-side bug we'd rather not silently propagate.
        this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "VC for %s has %zu bytes but bit_size=%d; skipping\n",
            sig->full_name.c_str(), raw.size(), sig->bit_size);
        return;
    }

    std::vector<uint64_t> values;
    std::vector<uint64_t> flags;
    this->decode_logic_value((const unsigned char *)raw.data(), sig->bit_size,
        values, flags);

    if (sig->signal_array.size() > 0)
    {
        // Distribute the decoded bits across element_size-wide sub-signals.
        // Skip sub-signals whose (value, flags) pair didn't change since the
        // last injection -- a wide signal that has been split into 128
        // chunks would otherwise call set() on every element on every
        // transition even when only a few bits flipped.
        int elem_size = sig->element_size;
        uint64_t mask = elem_size >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << elem_size) - 1;
        int nb_elements = (int)sig->signal_array.size();
        for (int i = 0; i < nb_elements; i++)
        {
            int bit_lo = i * elem_size;
            int chunk_idx = bit_lo / 64;
            int bit_in_chunk = bit_lo % 64;
            uint64_t v_slice = 0;
            uint64_t f_slice = 0;
            if (chunk_idx < (int)values.size())
            {
                v_slice = (values[chunk_idx] >> bit_in_chunk) & mask;
                f_slice = (flags[chunk_idx] >> bit_in_chunk) & mask;
            }
            if (v_slice == sig->last_elem_values[i] &&
                f_slice == sig->last_elem_flags[i])
            {
                continue;
            }
            sig->last_elem_values[i] = v_slice;
            sig->last_elem_flags[i] = f_slice;
            sig->signal_array[i]->set(v_slice, f_slice, 0, time_delay);
        }
    }
    else if (sig->signal)
    {
        // Scalar (<=64 bits) -- decoder produced exactly one chunk.
        uint64_t v = values.empty() ? 0 : values[0];
        uint64_t f = flags.empty() ? 0 : flags[0];
        if (v != sig->last_value || f != sig->last_flags)
        {
            sig->last_value = v;
            sig->last_flags = f;
            sig->signal->set(v, f, 0, time_delay);
        }
    }
}

void FstDumper::decode_logic_value(const unsigned char *vc_ptr, int bit_size,
    std::vector<uint64_t> &values, std::vector<uint64_t> &flags) const
{
    // vc_ptr[0] is MSB, vc_ptr[bit_size-1] is LSB (matches GTKWave's display
    // order). Each byte is one ASCII character: '0'/'1'/'x'/'z' for the
    // 4-state model, plus the SystemVerilog 9-state extras h/l/u/w/-, which
    // we fold into the 4-state encoding the GUI consumes.
    int nb_chunks = bit_size > 0 ? (bit_size + 63) / 64 : 0;
    values.assign(nb_chunks, 0);
    flags.assign(nb_chunks, 0);

    for (int i = 0; i < bit_size; i++)
    {
        int bit_from_lsb = bit_size - 1 - i;
        int chunk_idx = bit_from_lsb / 64;
        int bit_in_chunk = bit_from_lsb % 64;
        uint64_t mask = (uint64_t)1 << bit_in_chunk;
        unsigned char c = vc_ptr[i];
        switch (c)
        {
        case '0':
            break;
        case '1':
            values[chunk_idx] |= mask;
            break;
        case 'x': case 'X':
            flags[chunk_idx] |= mask;
            break;
        case 'z': case 'Z':
            values[chunk_idx] |= mask;
            flags[chunk_idx] |= mask;
            break;
        case 'h': case 'H':
            // weak high -> 1
            values[chunk_idx] |= mask;
            break;
        case 'l': case 'L':
            // weak low -> 0
            break;
        case 'u': case 'U':
        case 'w': case 'W':
        case '-':
            // uninit / weak unknown / dontcare -> X
            flags[chunk_idx] |= mask;
            break;
        default:
            // Unknown FST value byte -- treat as X so it's visible.
            flags[chunk_idx] |= mask;
            break;
        }
    }
}

int64_t FstDumper::exponent_to_ps_per_tick(signed char exponent)
{
    // FST exponent: 0 = 1 s, -3 = 1 ms, -6 = 1 us, -9 = 1 ns, -12 = 1 ps,
    // -15 = 1 fs. We round towards 1 ps for sub-ps timescales since that's
    // the GVSoC trace-engine resolution.
    int diff = (int)exponent + 12; // diff == 0 -> 1 ps
    if (diff < 0)
    {
        this->trace.msg(vp::Trace::LEVEL_WARNING,
            "FST timescale 10^%d s is below the 1 ps GVSoC resolution; "
            "rounding ticks to 1 ps each\n", (int)exponent);
        return 1;
    }
    int64_t r = 1;
    for (int i = 0; i < diff; i++)
    {
        r *= 10;
    }
    return r;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new FstDumper(config);
}
