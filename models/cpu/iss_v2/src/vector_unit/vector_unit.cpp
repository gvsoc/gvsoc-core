/*
 * Copyright (C) 2020 SAS, ETH Zurich and University of Bologna
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
 */

 #include <cstdint>
 #include <cstring>
 #include <vp/stats/stats_engine.hpp>
 #include <cpu/iss_v2/include/cores/vector_unit/vector_unit.hpp>


// Vector registers this instruction reads, as the scoreboard must see them.
// A masked instruction (vm bit clear) also reads v0, which is implicit in
// the encoding and therefore absent from the decoded register arguments:
// without it a masked operation can be issued -- and read its mask -- before
// the instruction writing v0 has committed, which makes the result depend on
// the schedule. Both the dependency setup and the release path must use this
// same effective mask, or v0's reader bit would never be cleared.
static inline uint32_t vu_in_vreg_mask(iss_insn_t *insn)
{
    uint32_t mask = insn->sb_in_vreg_mask;
    if (insn->desc->has_vm && insn->uim[0] == 0)
    {
        mask |= 1;
    }
    return mask;
}


Vu::Vu(Iss &iss)
    : Block(&iss, "ara"), iss(iss),
    fsm_event(this, &Vu::fsm_handler),
    nb_pending_insn(*this, "nb_pending_insn", 8, true),
    queue_full(*this, "queue_full", 1, true)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event_string("label", &this->event_label);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event("queue", &this->event_queue, 64);

    this->nb_lanes = iss.get_js_config()->get_int("vu/nb_lanes");
    // Number of integer units. Defaults to the number of lanes when the
    // configuration does not specify it.
    this->nb_ipus = iss.get_js_config()->get_int("vu/nb_ipus");
    if (this->nb_ipus == 0)
    {
        this->nb_ipus = this->nb_lanes;
    }
    this->lane_width = iss.get_js_config()->get_child_int("vu/lane_width");
    this->blocks.resize(Vu::nb_blocks);
    this->blocks[Vu::vlsu_id] = new VuLsu(*this, iss);
    this->blocks[Vu::vfpu_id] = new VuCompute(*this, "vfpu");
    this->blocks[Vu::vslide_id] = new VuCompute(*this, "vslide");

    this->pending_insns.resize(this->queue_size);
    this->insns_in_deps.resize(this->queue_size);
    this->insns_out_deps.resize(this->queue_size);

    for (int i=0; i<32; i++)
    {
        this->reading_insns[i] = 0;
        this->writing_insns[i] = 0;
    }

    if (!__iss_isa_set.initialized)
    {
        __iss_isa_set.initialized = true;

        // Make sure we track snitch load and stores to synchronize with spatz VLSU
        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("load"))
        {
            insn->u.insn.stub_handler = &Vu::load_store_handler<false>;
            insn->u.insn.stub_fast_handler = &Vu::load_store_handler<true>;
        }

        for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("store"))
        {
            insn->u.insn.stub_handler = &Vu::load_store_handler<false>;
            insn->u.insn.stub_fast_handler = &Vu::load_store_handler<true>;
        }

        this->isa_init();
    }

#ifdef CONFIG_GVSOC_STATS_ACTIVE
    // Cache whether stats are enabled; per-label entries register lazily under
    // the "vinsn_duration" group the first time each label completes.
    vp::StatsEngine *stats_engine = this->iss.stats.get_engine();
    this->stats_enabled = stats_engine != nullptr && stats_engine->is_enabled();
    this->insn_durations.init(&this->iss.stats, "vinsn_duration");
#endif
}

template<bool fast>
iss_reg_t Vu::load_store_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We need to stall snitch if:
    // - snitch wants to do a store while spatz is having pending memory accesses
    // - snitch wants to do a load while spatz is having pending loads
    if (insn->decoder_item->u.insn.tags[ISA_TAG_STORE_ID] && iss->arch.vu.nb_pending_vaccess ||
        insn->decoder_item->u.insn.tags[ISA_TAG_LOAD_ID] && iss->arch.vu.nb_pending_vstore)
    {
        iss->arch.vu.trace.msg(vp::Trace::LEVEL_TRACE, "Stalling due to on-going vector access (is_store: %d, pending_vaccess: %d, pending_vstore: %d\n",
            insn->decoder_item->u.insn.tags[ISA_TAG_STORE_ID], iss->arch.vu.nb_pending_vaccess, iss->arch.vu.nb_pending_vstore);
        iss->exec.insn_stall();
        return pc;
    }
    return iss_stub_handler<fast>(insn)(iss, insn, pc);
}

void Vu::reset(bool active)
{
    if (active)
    {
        this->nb_pending_vaccess = 0;
        this->nb_pending_vstore = 0;

        this->insn_id_table = (1 << this->queue_size) - 1;

        int index = 0;
        for (PendingInsn &pending_insn: this->pending_insns)
        {
            pending_insn.valid = false;
            pending_insn.id = index++;
        }
    }
}

PendingInsn *Vu::pending_insn_alloc(InsnEntry *entry)
{
    // The new instruction is marked as pending and also waiting for dependecy resolution
    this->nb_pending_insn.inc(1);
    if (this->nb_pending_insn.get() == this->queue_size)
    {
        this->queue_full.set(true);
    }
    int insn_id = this->alloc_id();

    // Copy the pending instruction coming from CVA6 since the commit will free it.
    PendingInsn *pending_insn = &this->pending_insns[insn_id];

    pending_insn->valid = true;
    pending_insn->entry = entry;
    pending_insn->nb_bytes_done = 0;

    iss_insn_t *insn = this->iss.exec.get_insn(entry);
    // Chaining is controlled per instruction through the chaining factors:
    // the RTL prevents chaining only for the slide-up family and the
    // strided/indexed memory accesses (their factors are set to 0 by the
    // ISA setup); slide-down and vmv chain like any other instruction.
    pending_insn->in_can_be_chained = insn->desc->chaining_factor != 0.0f;
    pending_insn->out_can_be_chained = insn->desc->out_chaining_factor != 0.0f;

    return pending_insn;
}

void Vu::insn_enqueue(InsnEntry *entry, iss_reg_t (*handler)(Iss *, iss_insn_t *, iss_reg_t))
{
    PendingInsn *pending_insn = this->pending_insn_alloc(entry);
    pending_insn->handler = handler;
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx, id: %d)\n",
        entry->addr, pending_insn->id);

	iss_insn_t *insn = this->iss.exec.get_insn(pending_insn->entry);

    uint8_t one = 1;
    this->event_active.event(&one);
    this->event_queue.event((uint8_t *)&insn->addr);
    this->event_label.event_string(insn->desc->label, false);

    pending_insn->chaining_factor = insn->desc->chaining_factor;
    pending_insn->out_chaining_factor = insn->desc->out_chaining_factor;

    // Derive the FPU pipeline depth from the instruction class and the
    // effective element width (the fpnew per-format register stages of the
    // spatz timing configuration: fp64: 2, fp32: 1, fp16/fp8: 0,
    // non-computational: 1, conversions: 2). Widening instructions compute
    // at the destination width (elem_rate_shift doubles it).
    pending_insn->pipeline_latency = 0;
    switch (insn->desc->fpu_lat_class)
    {
        case 1:
        {
            int sewb_eff = this->iss.vector.sewb << insn->desc->elem_rate_shift;
            pending_insn->pipeline_latency = sewb_eff >= 8 ? 2 : sewb_eff == 4 ? 1 : 0;
            break;
        }
        case 2:
            pending_insn->pipeline_latency = 1;
            break;
        case 3:
            pending_insn->pipeline_latency = 2;
            break;
    }

    // Mark the instruction to be handled in the next cycle in case the FSM is already active
    // to prevent it from handling it in the next cycle
    pending_insn->timestamp = this->iss.clock.get_cycles() + 1;

    // Not yet executing in any block; set by the block at its real start.
    pending_insn->exec_start_cycle = -1;
    pending_insn->commit_done_cycle = -1;
    pending_insn->chain_release_cycle = 0;

    // Copy the CVA6 register since we will use it later and it will probably change before that
    int reg = insn->in_regs[0];
    int reg_2 = insn->in_regs[1];
    int reg_3 = insn->in_regs[2];
    uint64_t reg_value, reg_value_2, reg_value_3;

    if (insn->nb_in_reg > 0)
    {
        reg_value = this->iss.regfile.get_reg_untimed(reg);
        pending_insn->reg = reg_value;
    }

    if (insn->nb_in_reg > 1)
    {
        reg_value_2 = this->iss.regfile.get_reg_untimed(reg_2);
        pending_insn->reg_2 = reg_value_2;
    }

    if (insn->nb_in_reg > 2)
    {
        reg_value_3 = this->iss.regfile.get_reg_untimed(reg_3);
        pending_insn->reg_3 = reg_value_3;
    }

    pending_insn->inreg0_index = reg;
    pending_insn->inreg1_index = reg_2;
    pending_insn->inreg2_index = reg_3;

    int block_id = insn->decoder_item->u.insn.block_id;

    // Some instructions like vsetvli have no associated block and must be execute by
    // the core
    if (block_id == -1)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction (pc: 0x%lx, id: %d)\n",
            insn->addr, pending_insn->id);

        this->event_pc.event((uint8_t *)&insn->addr);

        // Now that the instruction is over, execute the handler to functionally model it. This will
        // write the output register.
        // Store the instruction register as it will be used by the handler.
        this->current_insn_reg = pending_insn->reg;
        this->current_insn_reg_2 = pending_insn->reg_2;
        // Force trace dump since the core may be stalled which would skip trace
        pending_insn->handler(&this->iss, insn, insn->addr);

        this->insn_end(pending_insn);
    }
    else
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Handling instruction (pc: 0x%lx, id: %d)\n",
           insn->addr, pending_insn->id);

        this->event_pc.event((uint8_t *)&insn->addr);

        uint32_t mask = vu_in_vreg_mask(insn);
        this->insns_in_deps[pending_insn->id] = 0;
        while (mask)
        {
            int id = __builtin_ctz(mask);

            uint64_t insn_mask = this->writing_insns[id];
            while (insn_mask)
            {
                int insn_id = __builtin_ctzll(insn_mask);
                this->insns_in_deps[pending_insn->id] |= 1 << insn_id;
                insn_mask &= ~(1 << insn_id);
            }
            this->reading_insns[id] |= 1 << pending_insn->id;

            mask &= ~(1 << id);
        }
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Init instruction input deps (pc: 0x%lx, id: %d, deps: 0x%x)\n",
           insn->addr, pending_insn->id, this->insns_in_deps[pending_insn->id]);

        mask = insn->sb_out_vreg_mask;
        this->insns_out_deps[pending_insn->id] = 0;
        while (mask)
        {
            int id = __builtin_ctz(mask);

            uint64_t insn_mask = this->writing_insns[id];
            while (insn_mask)
            {
                int insn_id = __builtin_ctzll(insn_mask);
                this->insns_out_deps[pending_insn->id] |= 1 << insn_id;
                insn_mask &= ~(1 << insn_id);
            }

            // Write-after-read: the readers of the destination land in the
            // same mask as its writers, and both hazards block through the
            // same gate in insn_ready.
            insn_mask = this->reading_insns[id];
            while (insn_mask)
            {
                int insn_id = __builtin_ctzll(insn_mask);
                if (insn_id != pending_insn->id)
                {
                    this->insns_out_deps[pending_insn->id] |= 1 << insn_id;
                }
                insn_mask &= ~(1 << insn_id);
            }

            this->writing_insns[id] |= 1 << pending_insn->id;

            mask &= ~(1 << id);
        }
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Init instruction output deps (pc: 0x%lx, id: %d, deps: 0x%x)\n",
           insn->addr, pending_insn->id, this->insns_out_deps[pending_insn->id]);

        VuBlock *block = this->blocks[block_id];
        if (this->stalled_insns.empty() && !block->is_full())
        {
            // Enqueue the instruction to the processing block
            block->enqueue_insn(pending_insn);
        }
        else
        {
            this->stalled_insns.push(pending_insn);
        }
    }
}

void Vu::insn_commit(PendingInsn *pending_insn, int size)
{
    pending_insn->nb_bytes_done += size;
}

void Vu::insn_end(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->iss.exec.get_insn(pending_insn->entry);

    this->trace.msg(vp::Trace::LEVEL_TRACE, "End of instruction (pc: 0x%lx, id: %d)\n",
        insn->addr, pending_insn->id);

#ifdef CONFIG_GVSOC_STATS_ACTIVE
    // Account the per-label execution duration: from the block's real start to
    // now. Common to all blocks (VLSU / VFPU / VSLIDE).
    if (this->stats_enabled && pending_insn->exec_start_cycle >= 0)
    {
        this->insn_durations.account(insn->desc->label,
            this->iss.clock.get_cycles() - pending_insn->exec_start_cycle);
    }
#endif

    // If the ended instruction is a load or store, decrement associated counters used for
    // for synchronizing snitch and spatz memory accesses
    if (insn->decoder_item->u.insn.tags[ISA_TAG_VLOAD_ID])
    {
        this->nb_pending_vaccess--;
    }

    if (insn->decoder_item->u.insn.tags[ISA_TAG_VSTORE_ID])
    {
        this->nb_pending_vaccess--;
        this->nb_pending_vstore--;
    }

    // Mark the instruction as done. THe FSM will remove it when it is at the head of the queue
    pending_insn->done = true;

    // Unmark this instruction in the reading instructions
    uint32_t mask = vu_in_vreg_mask(insn);
    while (mask)
    {
        int id = __builtin_ctz(mask);
        this->reading_insns[id] &= ~(1 << pending_insn->id);
        mask &= ~(1 << id);
    }

    // Unmark this instruction in the writing instructions
    mask = insn->sb_out_vreg_mask;
    while (mask)
    {
        int id = __builtin_ctz(mask);
        this->writing_insns[id] &= ~(1 << pending_insn->id);
        mask &= ~(1 << id);
    }

    // Clear this instruction in other instructions dependencies
    for (int i=0; i<this->queue_size; i++)
    {
        this->insns_in_deps[i] &= ~(1 << pending_insn->id);
        this->insns_out_deps[i] &= ~(1 << pending_insn->id);
    }

    // Commit the instruction. This may unblock other instructions waiting for float registers
    this->insn_commit(pending_insn);

    // Enable the FSM to let it handle the pending instructions
    this->fsm_event.enable();
}

void Vu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Vu *_this = (Vu *)__this;

    // Check if the pending instruction at the head must be terminated
    if (_this->nb_pending_insn.get() > 0)
    {
        for (PendingInsn &pending_insn: _this->pending_insns)
        {
            if (pending_insn.valid && pending_insn.done &&
                pending_insn.timestamp <= _this->iss.clock.get_cycles())
            {
                pending_insn.valid = false;
                pending_insn.done = false;
                _this->nb_pending_insn.dec(1);
                _this->free_id(pending_insn.id);
                if (_this->nb_pending_insn.get() == 0)
                {
                    uint8_t zero = 0;
                    _this->event_active.event(&zero);
                    _this->event_label.event_string((char *)1, false);
                }
                _this->queue_full.set(false);
            }
        }
    }

    if (!_this->stalled_insns.empty())
    {
        PendingInsn *pending_insn = _this->stalled_insns.front();
        iss_insn_t *insn = _this->iss.exec.get_insn(pending_insn->entry);
        int block_id = insn->decoder_item->u.insn.block_id;
        VuBlock *block = _this->blocks[block_id];
        if (!block->is_full())
        {
            _this->stalled_insns.pop();
            block->enqueue_insn(pending_insn);
        }
    }

    if (_this->nb_pending_insn.get() == 0)
    {
        _this->event_queue.event_highz();
        _this->event_pc.event_highz();
        _this->fsm_event.disable();
    }
}

void Vu::isa_init()
{
    // Make sure all vector instructions are handled with dedicated handler so that they can be
    // offloaded to ara
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_isa("v"))
    {
        insn->u.insn.stub_handler = &Vu::vector_insn_stub_handler<false>;
        insn->u.insn.stub_fast_handler = &Vu::vector_insn_stub_handler<true>;
        // Vector instructions need to be handled differently in cva6
        insn->u.insn.flags |= ISS_INSN_FLAGS_VECTOR;
    }
    // Associate instruction to processing blocks
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vload"))
    {
        insn->u.insn.block_id = Vu::vlsu_id;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vstore"))
    {
        insn->u.insn.block_id = Vu::vlsu_id;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vothers"))
    {
        insn->u.insn.block_id = Vu::vfpu_id;
    }
    for (iss_decoder_item_t *insn: *iss.decode.get_insns_from_tag("vslide"))
    {
        insn->u.insn.block_id = Vu::vslide_id;
    }

    for (VuBlock *block: this->blocks)
    {
        block->isa_init();
    }
}

bool Vu::insn_ready(PendingInsn *insn)
{
    // Output hazards -- write-after-write and write-after-read alike -- block
    // until the dependency retires: insns_out_deps holds both the writers and
    // the readers of this instruction's destination registers. This is
    // pessimistic against the RTL, whose scoreboard makes no distinction
    // between hazard kinds at all: RAW, WAR and WAW deps land in one
    // per-instruction bitmask and a single per-cycle gate lets any dependent
    // port proceed whenever every dep accessed the VRF the previous cycle
    // (spatz_controller.sv, sb_enable_o: &(~deps | wrote_result_q)), so both
    // hazards chain for free there. Trailing rules approximating that (word
    // trailing behind a memory producer, and behind a reader) were tried and
    // removed: a live-code write-after-write always comes glued to the
    // write-after-read on the value's consumer -- a pure rewrite means the
    // first write was dead code -- so across the benchmark suite the rules
    // only moved kernels below noise, while their calibration probes had to
    // be written with artificial dead-write patterns. Better to implement
    // the same chaining as the RTL -- one dep bitmask and a per-cycle
    // accessed-last-cycle credit -- than to approximate hazard kinds
    // separately.
    if (this->insns_out_deps[insn->id] != 0)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE,
            "Instruction output-hazard dependency (id: %d, deps: 0x%lx)\n",
            insn->id, this->insns_out_deps[insn->id]);
        return false;
    }

    // Trailing drain of already-released input dependencies: their
    // scoreboard entry is gone but their results were only written
    // pipeline_latency cycles after the last operand word entered the unit.
    if (this->iss.clock.get_cycles() < insn->chain_release_cycle)
    {
        return false;
    }

    uint64_t deps_mask = this->insns_in_deps[insn->id];

    if (deps_mask == 0) return true;

    iss_insn_t *insn2 = this->iss.exec.get_insn(insn->entry);

    if (insn->in_can_be_chained)
    {
        while (deps_mask)
        {
            int dep_insn_id = __builtin_ctzll(deps_mask);
            PendingInsn *dep_insn = &this->pending_insns[dep_insn_id];

            int chunk_size = this->nb_lanes * this->lane_width;

            if (!dep_insn->out_can_be_chained)
                return false;

            // A chained consumer trails the producer by one chunk plus the
            // producer's FPU pipeline depth: the producer's result word is
            // only written pipeline_latency cycles after its operands
            // entered the unit (one chunk is processed per cycle, so the
            // extra trailing distance is expressed in chunks).
            if (dep_insn->nb_bytes_done * dep_insn->out_chaining_factor <
                    (insn->nb_bytes_done + chunk_size * (1 + dep_insn->pipeline_latency)) * insn->chaining_factor)
            {
                // The producer cannot progress any further once fully
                // committed; its remaining results become available as it
                // drains through the pipeline, one word per cycle counted
                // from the commit time (plus one cycle to consume the
                // written word). Without this the consumer's trailing
                // chunks would wait for the producer's scoreboard release,
                // overcharging tight accumulation chains where the RTL
                // trails word by word.
                if (dep_insn->pipeline_latency == 0 || dep_insn->commit_done_cycle < 0)
                    return false;

                float out_factor = dep_insn->out_chaining_factor != 0.0f ?
                    dep_insn->out_chaining_factor : 1.0f;
                float base_needed = (insn->nb_bytes_done + chunk_size) *
                    insn->chaining_factor / out_factor;
                float avail = dep_insn->nb_bytes_done;
                if (base_needed > avail)
                    base_needed = avail;
                int64_t t_needed = dep_insn->commit_done_cycle -
                    (int64_t)((avail - base_needed) / chunk_size);
                if (this->iss.clock.get_cycles() < t_needed + dep_insn->pipeline_latency + 1)
                    return false;
            }

            deps_mask &= ~(1 << dep_insn_id);
        }
        return true;
    }

    // Non-chainable consumer: blocked until its dependencies are released
    // from the scoreboard. The producer's pipeline drain outlives that
    // release, so remember the drain horizon on the consumer — it keeps
    // gating it (chain_release_cycle above) after the mask has cleared.
    while (deps_mask)
    {
        int dep_insn_id = __builtin_ctzll(deps_mask);
        PendingInsn *dep_insn = &this->pending_insns[dep_insn_id];

        if (dep_insn->pipeline_latency > 0 && dep_insn->commit_done_cycle >= 0)
        {
            int64_t release = dep_insn->commit_done_cycle + dep_insn->pipeline_latency + 1;
            if (release > insn->chain_release_cycle)
            {
                insn->chain_release_cycle = release;
            }
        }

        deps_mask &= ~(1 << dep_insn_id);
    }

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Instruction input dependency (id: %d, deps: 0x%x)\n",
        insn->id, this->insns_in_deps[insn->id]);

    return false;
}

void Vu::dump_regs_to_trace(iss_insn_t *insn, PendingInsn *pending_insn, int nb_elem, bool is_out)
{
    if (this->iss.trace.insn_trace.get_active())
    {
        for (int i = 0; i < insn->decoder_item->u.insn.nb_args; i++)
        {
            iss_insn_arg_t *arg = &insn->args[i];
            if ((arg->flags & ISS_DECODER_ARG_FLAG_VREG) &&
                ((is_out && (arg->type & ISS_DECODER_ARG_TYPE_OUT_REG)) ||
                    (!is_out && (arg->type & ISS_DECODER_ARG_TYPE_IN_REG))))
            {
                int offset = this->vstart * this->iss.vector.sewb;
                memcpy(&pending_insn->entry->trace->saved_vargs[i][offset],
                    &this->iss.vector.vregs[arg->u.reg.index][offset],
                    this->iss.vector.sewb * nb_elem);
            }
        }
    }
}

void Vu::insn_commit(PendingInsn *pending_insn)
{
    iss_insn_t *insn = this->iss.exec.get_insn(pending_insn->entry);

    this->iss.exec.trace.msg(vp::Trace::LEVEL_TRACE, "End of instruction (pc: 0x%lx)\n", insn->addr);

    this->iss.exec.insn_terminate(pending_insn->entry);
}

// A vsetvli executes its functional effect (csr.vtype / csr.vl update)
// immediately at issue, before the instructions still queued ahead of it
// are handled. Those queued ops read the vector configuration live when
// they run, so a vsetvli that changes vtype or vl must wait for the queue
// to drain first, or it would corrupt them. A vsetvli that re-selects the
// SAME configuration is harmless and, like the RTL, must not stall — this
// is the common case in stripmined loops (a constant `vsetvli` per
// iteration) and dominates the dp-fdotp cycle-count gap.
//
// Returns true if the queue must be drained before this instruction. Only
// the immediate `vsetvli` form is analysed; any other config-setting
// instruction conservatively drains.
static bool vsetvli_needs_drain(Iss *iss, iss_insn_t *insn)
{
    if (strcmp(insn->desc->label, "vsetvli") != 0)
    {
        return true;
    }

    // uim[2] is the vtype immediate. A different vtype changes SEW/LMUL
    // (and hence VLMAX), so the queued ops must drain first.
    if ((iss_reg_t)insn->uim[2] != iss->csr.vtype.value)
    {
        return true;
    }

    // vtype unchanged => SEW/LMUL/VLMAX unchanged. Compute the vl this
    // vsetvli would set and drain only if it differs from the current vl.
    int vlmax = (int)((float)CONFIG_ISS_VLEN / (iss->vector.sewb * 8)
                      * iss->vector.lmul);
    int idx_rs1 = insn->in_regs[0];
    int idx_rd  = insn->out_regs[0];
    iss_reg_t new_vl;
    if (idx_rs1)
    {
        iss_reg_t avl = iss->regfile.get_reg_untimed(idx_rs1);
        new_vl = avl < (iss_reg_t)vlmax ? avl : (iss_reg_t)vlmax;
    }
    else if (idx_rd)
    {
        new_vl = vlmax;
    }
    else
    {
        new_vl = iss->csr.vl.value;
    }

    return new_vl != iss->csr.vl.value;
}

template<bool fast>
iss_reg_t Vu::vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // We stall the instruction if the ara queue is full, or if it is a
    // config-changing vsetvli and the queue is not empty (see
    // vsetvli_needs_drain): the vsetvli's csr update lands immediately and
    // would otherwise corrupt the queued ops that still read the old
    // configuration.
    if (iss->arch.vu.queue_is_full() ||
        (insn->decoder_item->u.insn.block_id == -1
         && !iss->arch.vu.queue_is_empty()
         && vsetvli_needs_drain(iss, insn)))
    {
        iss->exec.trace.msg(vp::Trace::LEVEL_TRACE, "%s queue is full (pc: 0x%lx)\n",
            iss->arch.vu.queue_is_full() ? "Ara" : "Core", pc);

        iss->exec.insn_stall();
        return pc;
    }

    InsnEntry *entry = iss->exec.insn_hold(insn);

    // Account vector loads and stores to synchronize with snitch
    if (insn->decoder_item->u.insn.tags[ISA_TAG_VLOAD_ID])
    {
        iss->arch.vu.nb_pending_vaccess++;
    }

    if (insn->decoder_item->u.insn.tags[ISA_TAG_VSTORE_ID])
    {
        iss->arch.vu.nb_pending_vaccess++;
        iss->arch.vu.nb_pending_vstore++;
    }

    iss->arch.vu.insn_enqueue(entry, iss_stub_handler<fast>(insn));

    return iss_insn_next(iss, insn, pc);
}
