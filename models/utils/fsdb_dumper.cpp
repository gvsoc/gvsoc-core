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
 * FSDB dumper: loads an FSDB waveform file at startup, creates a
 * vp::Signal per variable, and injects value changes at their FSDB timestamps
 * so the GVSoC GUI can display them as a general-purpose FSDB viewer. Same
 * idea as utils/vcd_dumper.cpp.
 */

#include <vp/vp.hpp>
#include <vp/signal.hpp>

#ifdef NOVAS_FSDB
#undef NOVAS_FSDB
#endif
#include "ffrAPI.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>


class FsdbDumper;

class FsdbSignal
{
public:
    fsdbVarIdcode idcode = 0;
    std::string full_name;
    int bit_size = 0;
    int bytes_per_bit = FSDB_BYTES_PER_BIT_1B;
    // Split choice decided at tree-walk time, consumed by start() when the
    // actual vp::Signal<uint64_t> objects are allocated:
    //   element_size == 0            -> single scalar signal
    //   element_size > 0, user_split -> bit_size/element_size children named
    //                                    '/[i]' (WaveLayout element_size)
    //   element_size == 64, !user_split -> ceil(bit_size/64) children named
    //                                    '/[hi:lo]' (auto 64-bit chunks)
    int element_size = 0;
    bool user_split = false;
    vp::Signal<uint64_t> *signal = nullptr;
    std::vector<vp::Signal<uint64_t> *> signal_array;
    ffrVCTrvsHdl hdl = nullptr;

    // Cache of the last (value, flags) pair emitted to each (sub-)signal so
    // inject_value() can skip redundant vp::Signal::set() calls. FSDB
    // frequently emits a value change for the entire wide variable even
    // when only a few bits flipped; without this cache every element of a
    // 128-child split would call set() on every transition. Initialised to
    // (0, 0) which matches the default vp::Signal reset value, so the
    // first real transition to (0, 0) is also elided correctly.
    uint64_t last_value = 0;
    uint64_t last_flags = 0;
    std::vector<uint64_t> last_elem_values;
    std::vector<uint64_t> last_elem_flags;
};

class FsdbDumper : public vp::Component
{
public:
    FsdbDumper(vp::ComponentConf &config);
    ~FsdbDumper();

    void reset(bool active) override;

private:
    static bool_T tree_cb(fsdbTreeCBType cb_type, void *client_data, void *tree_cb_data);
    void handle_scope(fsdbTreeCBDataScope *scope);
    void handle_upscope();
    void handle_var(fsdbTreeCBDataVar *var);

    // Allocate the vp::Signal<uint64_t> objects for every FSDB variable.
    // Runs once, from reset(active=false), so the GUI's Vcd_user is already
    // bound by the time signal.enable() registers each event. The heavy
    // FSDB value-change load already happened in the constructor.
    void materialize_signals();

    static void event_handler(vp::Block *__this, vp::TimeEvent *event);
    // time_delay is added to the current simulated time so one handler call
    // can emit VCs that belong to multiple future timestamps; this keeps the
    // number of TimeEvent schedulings bounded even for densely-sampled FSDBs.
    void inject_value(FsdbSignal *sig, int64_t time_delay);
    bool advance_signal(FsdbSignal *sig, int64_t &next_ps);
    void enqueue_next();

    int64_t xtag_to_ps(const fsdbTag64 &tag) const;
    int64_t parse_scale_unit(const char *scale_unit);
    // Decode an FSDB per-bit value buffer into one or more 64-bit (value,
    // flags) chunks under the 4-state convention:
    //   (flag=0,val=0/1) -> 0/1, (flag=1,val=0) -> X, (flag=1,val=1) -> Z.
    // Chunk i (0-indexed) covers bits [(i+1)*64-1 : i*64] of the original
    // signal (low-order chunk first). Signals wider than 64 bits therefore
    // produce multiple chunks rather than being truncated.
    void decode_logic_value(const byte_T *vc_ptr, int bit_size,
        std::vector<uint64_t> &values, std::vector<uint64_t> &flags) const;

    ffrObject *fsdb_obj = nullptr;
    std::vector<std::string> scope_stack;
    std::unordered_map<fsdbVarIdcode, FsdbSignal *> signals;
    std::unordered_map<std::string, int> element_size_map;

    struct HeapEntry
    {
        int64_t time_ps;
        FsdbSignal *sig;
    };
    struct HeapCmp
    {
        bool operator()(const HeapEntry &a, const HeapEntry &b) const
        {
            return a.time_ps > b.time_ps;
        }
    };
    std::priority_queue<HeapEntry, std::vector<HeapEntry>, HeapCmp> next_vc_heap;

    int64_t ps_per_tick = 1;
    int64_t current_time_ps = -1;
    bool signals_materialized = false;
    vp::TimeEvent event;
    vp::Trace trace;
};


FsdbDumper::FsdbDumper(vp::ComponentConf &config)
    : vp::Component(config), event(this, &FsdbDumper::event_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    js::Config *es_cfg = this->get_js_config()->get("element_size");
    if (es_cfg != NULL)
    {
        for (auto it : es_cfg->get_childs())
        {
            // Accept keys with or without a leading '/'. The full_name we
            // build from the FSDB scope stack has no leading '/' but the
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

    std::string fsdb_path = this->get_js_config()->get_child_str("fsdb_file");
    if (fsdb_path == "")
    {
        return;
    }

    char fsdb_name[FSDB_MAX_PATH + 1] = {0};
    strncpy(fsdb_name, fsdb_path.c_str(), FSDB_MAX_PATH);

    if (FALSE == ffrObject::ffrIsFSDB(fsdb_name))
    {
        this->trace.fatal("%s is not an FSDB file\n", fsdb_name);
        return;
    }

    this->fsdb_obj = ffrObject::ffrOpen3(fsdb_name);
    if (this->fsdb_obj == NULL)
    {
        this->trace.fatal("ffrOpen3 failed for %s\n", fsdb_name);
        return;
    }

    if (FSDB_FT_VERILOG != this->fsdb_obj->ffrGetFileType())
    {
        this->trace.msg(vp::Trace::LEVEL_WARNING,
            "FSDB file is not of verilog type; only verilog FSDBs are supported\n");
    }

    const char *scale_unit = this->fsdb_obj->ffrGetScaleUnit();
    this->ps_per_tick = this->parse_scale_unit(scale_unit);
    this->trace.msg(vp::Trace::LEVEL_INFO,
        "FSDB timescale: %s (%ld ps/tick)\n",
        scale_unit ? scale_unit : "(null)", (long)this->ps_per_tick);

    // Walk the FSDB var/scope tree: populates this->signals with metadata
    // only. vp::Signal objects are not allocated here because their enable()
    // call would run before the GUI has bound a vcd_user to the trace engine;
    // the allocation happens in reset(false) instead.
    this->fsdb_obj->ffrSetTreeCBFunc(&FsdbDumper::tree_cb, this);
    this->fsdb_obj->ffrReadScopeVarTree();

    // The FSDB value-change load is the heaviest step (O(file size) I/O).
    // Do it now so it overlaps with GUI startup; then reset(false) only
    // does the cheap vp::Signal allocation + register pass.
    for (auto &kv : this->signals)
    {
        this->fsdb_obj->ffrAddToSignalList(kv.first);
    }
    this->fsdb_obj->ffrLoadSignals();

    for (auto &kv : this->signals)
    {
        FsdbSignal *sig = kv.second;
        sig->hdl = this->fsdb_obj->ffrCreateVCTraverseHandle(sig->idcode);
        if (sig->hdl == NULL)
        {
            continue;
        }
        if (FALSE == sig->hdl->ffrHasIncoreVC())
        {
            sig->hdl->ffrFree();
            sig->hdl = NULL;
            continue;
        }
        fsdbTag64 tag;
        sig->hdl->ffrGetMinXTag((void *)&tag);
        if (FSDB_RC_SUCCESS != sig->hdl->ffrGotoXTag((void *)&tag))
        {
            continue;
        }
        int64_t ts = this->xtag_to_ps(tag);
        this->next_vc_heap.push({ts, sig});
    }
}

void FsdbDumper::materialize_signals()
{
    if (this->fsdb_obj == nullptr)
    {
        return;
    }

    // Construct the actual vp::Signal<uint64_t> objects now that Db::bind
    // has attached vcd_user. signal.enable() registers the underlying event
    // synchronously with the GUI's Db; doing this in the constructor (or
    // from start(), which runs inside gvsoc->open() before Db::bind) would
    // take the vcd_user == NULL branch and the events would never appear.
    // The FSDB value-change load already ran in the constructor, so this
    // pass is only the cheap allocation + register step and runs fast.
    for (auto &kv : this->signals)
    {
        FsdbSignal *sig = kv.second;
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

FsdbDumper::~FsdbDumper()
{
    for (auto &kv : this->signals)
    {
        if (kv.second->hdl)
        {
            kv.second->hdl->ffrFree();
        }
    }
    if (this->fsdb_obj)
    {
        this->fsdb_obj->ffrClose();
        this->fsdb_obj = nullptr;
    }
}

void FsdbDumper::reset(bool active)
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

bool_T FsdbDumper::tree_cb(fsdbTreeCBType cb_type, void *client_data, void *tree_cb_data)
{
    FsdbDumper *_this = (FsdbDumper *)client_data;
    switch (cb_type)
    {
    case FSDB_TREE_CBT_SCOPE:
        _this->handle_scope((fsdbTreeCBDataScope *)tree_cb_data);
        break;
    case FSDB_TREE_CBT_UPSCOPE:
        _this->handle_upscope();
        break;

    // Regular and extended var callbacks. Each of these carries a
    // `fsdbTreeCBDataVar *` payload (fsdbShr.h:1797), so the existing
    // handle_var() can absorb them. Without this, packed multi-dim arrays
    // like `logic [N-1:0][M-1:0] foo` (which FSDB emits as PACKED_VAR
    // rather than plain VAR) silently disappear.
    case FSDB_TREE_CBT_VAR:
    case FSDB_TREE_CBT_PACKED_VAR:
    case FSDB_TREE_CBT_PACKED_COMP_VAR:
    case FSDB_TREE_CBT_UNEXPANDED_MEMORY_VAR:
    case FSDB_TREE_CBT_MDF_VAR:
        _this->handle_var((fsdbTreeCBDataVar *)tree_cb_data);
        break;

    // Structural markers with no hierarchy contribution we can use today.
    // Quietly acknowledged so they don't pollute the unhandled log below.
    case FSDB_TREE_CBT_BEGIN_TREE:
    case FSDB_TREE_CBT_END_TREE:
    case FSDB_TREE_CBT_END_ALL_TREE:
    case FSDB_TREE_CBT_ARRAY_BEGIN:
    case FSDB_TREE_CBT_ARRAY_END:
    case FSDB_TREE_CBT_RECORD_BEGIN:
    case FSDB_TREE_CBT_RECORD_END:
    case FSDB_TREE_CBT_STRUCT_BEGIN:
    case FSDB_TREE_CBT_STRUCT_END:
    case FSDB_TREE_CBT_INTERCONNECT_ARRAY_BEGIN:
    case FSDB_TREE_CBT_INTERCONNECT_ARRAY_END:
    case FSDB_TREE_CBT_FILE_TYPE:
    case FSDB_TREE_CBT_SIMULATOR_VERSION:
    case FSDB_TREE_CBT_SIMULATION_DATE:
    case FSDB_TREE_CBT_X_AXIS_SCALE:
        break;

    default:
        // Surface anything we don't explicitly handle at debug level so it
        // is easy to spot signals we might still be dropping.
        _this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "fsdb_dumper: unhandled tree callback type %d\n", (int)cb_type);
        return FALSE;
    }
    return TRUE;
}

void FsdbDumper::handle_scope(fsdbTreeCBDataScope *scope)
{
    this->scope_stack.push_back(scope->name ? std::string(scope->name) : std::string("?"));
}

void FsdbDumper::handle_upscope()
{
    if (!this->scope_stack.empty())
    {
        this->scope_stack.pop_back();
    }
}

void FsdbDumper::handle_var(fsdbTreeCBDataVar *var)
{
    // Use '/' as the hierarchy separator (GVSoC trace-engine convention) so
    // the Db path hierarchy is proper and every consumer (Timeline tree,
    // SignalBrowser, etc.) sees a structured tree instead of one flat name.
    std::string full_name;
    for (const auto &s : this->scope_stack)
    {
        full_name += s;
        full_name += "/";
    }
    full_name += var->name ? var->name : "?";

    int bit_size = std::abs((int)var->lbitnum - (int)var->rbitnum) + 1;

    // Non-1B-per-bit vars (real/float/memory) aren't decoded by this prototype.
    if (var->bytes_per_bit != FSDB_BYTES_PER_BIT_1B)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Skipping non-logic signal %s (bytes_per_bit=%d)\n",
            full_name.c_str(), (int)var->bytes_per_bit);
        return;
    }

    FsdbSignal *sig = new FsdbSignal();
    sig->idcode = var->u.idcode;
    sig->full_name = full_name;
    sig->bit_size = bit_size;
    sig->bytes_per_bit = var->bytes_per_bit;

    // Decide the split strategy now so start() can just allocate. The actual
    // vp::Signal<uint64_t> objects are created post-bind in start() so that
    // signal.enable() registers with a vcd_user that's already attached.
    auto it = this->element_size_map.find(full_name);
    if (it != this->element_size_map.end())
    {
        // User-supplied element width: N elements of M bits each.
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
        // Auto 64-bit chunking for wide signals with no user override.
        sig->element_size = 64;
        sig->user_split = false;
    }
    // else: element_size stays 0 -> scalar signal in start().

    this->signals[sig->idcode] = sig;
}

void FsdbDumper::event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    FsdbDumper *_this = (FsdbDumper *)__this;
    int64_t now = _this->time.get_time();

    // Drain a batch of value changes even if they span multiple future FSDB
    // timestamps: each VC is injected with a `time_delay` so the trace
    // engine records it at its correct simulated time without us having to
    // re-enter the time engine for every distinct timestamp. For RTL dumps
    // with timestamps at every clock tick this cuts the number of TimeEvent
    // scheduling round-trips from millions down to a handful per second.
    constexpr int BATCH_LIMIT = 16384;
    int processed = 0;
    while (!_this->next_vc_heap.empty() && processed < BATCH_LIMIT)
    {
        HeapEntry top = _this->next_vc_heap.top();
        int64_t delay = top.time_ps - now;
        if (delay < 0)
        {
            delay = 0;
        }
        _this->next_vc_heap.pop();
        _this->inject_value(top.sig, delay);

        int64_t next_ps;
        if (_this->advance_signal(top.sig, next_ps))
        {
            _this->next_vc_heap.push({next_ps, top.sig});
        }
        processed++;
    }

    _this->enqueue_next();
}

void FsdbDumper::enqueue_next()
{
    if (this->next_vc_heap.empty())
    {
        this->time.get_engine()->quit(0);
        return;
    }
    int64_t next = this->next_vc_heap.top().time_ps;
    this->current_time_ps = next;
    int64_t now = this->time.get_time();
    int64_t delta = next - now;
    if (delta < 0)
    {
        delta = 0;
    }
    this->event.enqueue(delta);
}

bool FsdbDumper::advance_signal(FsdbSignal *sig, int64_t &next_ps)
{
    if (sig->hdl == NULL)
    {
        return false;
    }
    if (FSDB_RC_SUCCESS != sig->hdl->ffrGotoNextVC())
    {
        sig->hdl->ffrFree();
        sig->hdl = NULL;
        return false;
    }
    fsdbTag64 tag;
    sig->hdl->ffrGetXTag(&tag);
    next_ps = this->xtag_to_ps(tag);
    return true;
}

void FsdbDumper::inject_value(FsdbSignal *sig, int64_t time_delay)
{
    if (sig->hdl == NULL || sig->bytes_per_bit != FSDB_BYTES_PER_BIT_1B)
    {
        return;
    }

    byte_T *vc_ptr = nullptr;
    if (FSDB_RC_SUCCESS != sig->hdl->ffrGetVC(&vc_ptr) || vc_ptr == nullptr)
    {
        return;
    }

    std::vector<uint64_t> values;
    std::vector<uint64_t> flags;
    this->decode_logic_value(vc_ptr, sig->bit_size, values, flags);

    if (sig->signal_array.size() > 0)
    {
        // Distribute the decoded bits across element_size-wide sub-signals.
        // Skip sub-signals whose (value, flags) pair didn't change since the
        // last injection -- FSDB emits a single VC for the whole wide var
        // even when only a handful of bits flip, so most elements of a 128-
        // child split are typically no-ops.
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

void FsdbDumper::decode_logic_value(const byte_T *vc_ptr, int bit_size,
    std::vector<uint64_t> &values, std::vector<uint64_t> &flags) const
{
    // vc_ptr[0] is MSB, vc_ptr[bit_size-1] is LSB (matches Verdi display order).
    // 4-state mapping per bit: (flag=0,val=0/1) -> 0/1, (flag=1,val=0) -> X,
    // (flag=1,val=1) -> Z.
    // Produces one 64-bit chunk per 64 bits of the source; chunk 0 is the
    // low-order 64 bits (bits 63..0 of the signal), chunk 1 is bits
    // 127..64, etc.
    int nb_chunks = bit_size > 0 ? (bit_size + 63) / 64 : 0;
    values.assign(nb_chunks, 0);
    flags.assign(nb_chunks, 0);

    for (int i = 0; i < bit_size; i++)
    {
        int bit_from_lsb = bit_size - 1 - i;
        int chunk_idx = bit_from_lsb / 64;
        int bit_in_chunk = bit_from_lsb % 64;
        uint64_t mask = (uint64_t)1 << bit_in_chunk;
        switch (vc_ptr[i])
        {
        case FSDB_BT_VCD_0:
            break;
        case FSDB_BT_VCD_1:
            values[chunk_idx] |= mask;
            break;
        case FSDB_BT_VCD_X:
            flags[chunk_idx] |= mask;
            break;
        case FSDB_BT_VCD_Z:
            values[chunk_idx] |= mask;
            flags[chunk_idx] |= mask;
            break;
        default:
            // Unknown FSDB value byte -- treat as X.
            flags[chunk_idx] |= mask;
            break;
        }
    }
}

int64_t FsdbDumper::xtag_to_ps(const fsdbTag64 &tag) const
{
    uint64_t ticks = ((uint64_t)tag.H << 32) | (uint64_t)tag.L;
    return (int64_t)(ticks * (uint64_t)this->ps_per_tick);
}

int64_t FsdbDumper::parse_scale_unit(const char *scale_unit)
{
    if (scale_unit == nullptr || *scale_unit == '\0')
    {
        return 1;
    }
    const char *p = scale_unit;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    int64_t digit = 0;
    while (*p >= '0' && *p <= '9')
    {
        digit = digit * 10 + (*p - '0');
        p++;
    }
    if (digit == 0)
    {
        digit = 1;
    }
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    int64_t unit_ps = 1;
    if (*p == 'f')
    {
        // femtoseconds — below GVSoC's picosecond resolution
        this->trace.msg(vp::Trace::LEVEL_WARNING,
            "FSDB uses fs timescale; truncating to 1 ps\n");
        unit_ps = 1;
    }
    else if (*p == 'p')
    {
        unit_ps = 1;
    }
    else if (*p == 'n')
    {
        unit_ps = 1000;
    }
    else if (*p == 'u')
    {
        unit_ps = 1000000LL;
    }
    else if (*p == 'm')
    {
        unit_ps = 1000000000LL;
    }
    else if (*p == 's')
    {
        unit_ps = 1000000000000LL;
    }
    return digit * unit_ps;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new FsdbDumper(config);
}
