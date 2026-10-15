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

#include "cpu/iss/include/iss.hpp"
#include "cpu/iss/include/cores/ara/ara.hpp"

AraVlsu::AraVlsu(Ara &ara, IssWrapper &top)
: AraBlock(&ara, "vlsu"), ara(ara),
nb_pending_insn(*this, "nb_pending_insn", 8, true),
fsm_event(this, &AraVlsu::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->traces.new_trace_event("active", &this->event_active, 1);
    this->traces.new_trace_event("pc", &this->event_pc, 64);
    this->traces.new_trace_event("queue", &this->event_queue, 64);
    this->traces.new_trace_event_string("label", &this->event_label);

    this->insns.resize(AraVlsu::queue_size);

    int nb_ports = top.get_js_config()->get_child_int("vu/nb_ports");
    this->nb_ports = nb_ports;

    this->event_addr.resize(nb_ports);
    this->event_size.resize(nb_ports);
    this->event_is_write.resize(nb_ports);

    for (int i=0; i<nb_ports; i++)
    {
        this->traces.new_trace_event("port_" + std::to_string(i) + "/addr",
            &this->event_addr[i], 64);
        this->traces.new_trace_event("port_" + std::to_string(i) + "/size",
            &this->event_size[i], 64);
        this->traces.new_trace_event("port_" + std::to_string(i) + "/is_write",
            &this->event_is_write[i], 1);
    }

    this->ports.resize(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        top.new_master_port("vlsu_" + std::to_string(i), &this->ports[i], this);
    }

    this->width = top.get_js_config()->get_child_int("vu/lsu_width");
    this->tile_width = top.get_js_config()->get_child_int("vu/tile_lsu_width");

    int nb_outstanding_reqs = top.get_js_config()->get_child_int("vu/nb_outstanding_reqs");
    this->req_queues.resize(nb_ports);
    for (int i=0; i<nb_ports; i++)
    {
        this->req_queues[i] = new vp::Queue(this, "port_" + std::to_string(i) + "_reqs");
    }

    this->requests.resize(nb_ports * nb_outstanding_reqs);
}

void AraVlsu::start()
{
}

void AraVlsu::reset(bool active)
{
    if (active)
    {
        uint8_t zero = 0;
        this->event_active.event(&zero);
        for (int i=0; i<nb_ports; i++)
        {
            this->event_addr[i].event(&zero);
            this->event_size[i].event(&zero);
            this->event_is_write[i].event(&zero);
        }
        this->insn_first = 0;
        this->insn_first_waiting = 0;
        this->insn_last = 0;
        this->nb_waiting_insn = 0;
        this->pending_size = 0;
        this->pending_is_tile = false;

        // Since the request queues are cleared with the reset, we need to put back requests
        // in each queue
        int nb_ports = this->ara.iss.top.get_js_config()->get_child_int("vu/nb_ports");
        int nb_outstanding_reqs = this->ara.iss.top.get_js_config()->get_child_int("vu/nb_outstanding_reqs");
        int req_id = 0;
        for (int i=0; i<nb_ports; i++)
        {
            for (int j=0; j<nb_outstanding_reqs; j++)
            {
                this->req_queues[i]->push_back(&this->requests[req_id++]);
            }
        }
    }
}

void AraVlsu::enqueue_insn(PendingInsn *pending_insn)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Enqueue instruction (pc: 0x%lx)\n", pending_insn->pc);
    uint8_t one = 1;
    this->event_active.event(&one);
    this->event_queue.event((uint8_t *)&pending_insn->pc);

    // Just push the instruction and let the FSM handle it if needed.
    // A delay is added to take into account the time needed on RTL to start the instruction
    pending_insn->timestamp = this->ara.iss.top.clock.get_cycles() + 5;
    AraVlsuPendingInsn &slot = this->insns[this->insn_last];
    this->insn_last = (this->insn_last + 1) % AraVlsu::queue_size;
    slot.insn = pending_insn;
    slot.nb_pending_bursts = 0;
    this->nb_pending_insn.inc(1);
    this->nb_waiting_insn++;

    this->fsm_event.enable();
}

void AraVlsu::isa_init()
{
    // Attach handlers to instructions so that we can quickly handle load and stores differently
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vload"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_load;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vstore"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_store;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vload_strided"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_load_strided;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vstore_strided"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_store_strided;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vload_indexed"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_load_indexed;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vstore_indexed"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_store_indexed;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vtload"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_tile_load;
    }
    for (iss_decoder_item_t *insn: *this->ara.iss.decode.get_insns_from_tag("vtstore"))
    {
        insn->u.insn.block_handler = (void *)&AraVlsu::handle_insn_tile_store;
    }
}

void AraVlsu::handle_insn_load_strided(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0], true, _this->insns[_this->insn_first_waiting].insn->reg_2);
}

void AraVlsu::handle_insn_store_strided(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1], true, _this->insns[_this->insn_first_waiting].insn->reg_3);
}

void AraVlsu::handle_insn_load_indexed(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0], false, 0, insn->in_regs[1]);
}

void AraVlsu::handle_insn_store_indexed(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1], false, 0, insn->in_regs[2]);
}

void AraVlsu::handle_access(iss_insn_t *insn, bool is_write, int reg, bool do_stride, iss_reg_t stride, int reg_indexed)
{
    // A load or store instruction is starting, just store information about the first burst and let
    // the FSM handle all the bursts.
    this->pending_is_tile = false;
    unsigned int sewb = this->ara.iss.vector.sewb;
    unsigned int lmul = this->ara.iss.vector.lmul;
    this->pending_vreg = reg;
    this->pending_velem = velem_get(&this->ara.iss, reg, 0, sewb, lmul);
    this->pending_addr = this->insns[this->insn_first_waiting].insn->reg;
    this->pending_is_write = is_write;
    int inst_elem_size = insn->uim[1] >= 5 ? 1 << (insn->uim[1] - 4) : 1 << 0;
    int elem_size = reg_indexed != -1 ? sewb : inst_elem_size;
    this->pending_size = (this->ara.iss.csr.vl.value - this->ara.iss.csr.vstart.value) * elem_size;
    this->stride = stride;
    this->strided = do_stride;
    this->elem_size = elem_size;
    this->inst_elem_size = inst_elem_size;
    this->reg_indexed = reg_indexed;
    this->pending_elem = 0;
}

void AraVlsu::handle_insn_load(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, false, insn->out_regs[0]);
}

void AraVlsu::handle_insn_store(AraVlsu *_this, iss_insn_t *insn)
{
    _this->handle_access(insn, true, insn->in_regs[1]);
}

// ---------------------------------------------------------------------------
// Tile load (vtle8/16/32/64)
//
// Operand mapping (from isa_rvv_timed.py InReg order):
//   pending_insn->reg   = InReg(0) = rs2 value = TSS
//   pending_insn->reg_2 = InReg(1) = rs1 value = base address
//   insn->uim[0]        = nf_eew bits[31:29]:  0→1B  1→2B  2→4B  3→8B
//
// Strategy: always load raw bytes into tile_staging via the FSM, then call
// tile_load_commit() to zero-extend and scatter into tile.mregs.
// vtle64 is not supported (TEW=32 accumulator cannot hold 64-bit values).
// ---------------------------------------------------------------------------
void AraVlsu::handle_insn_tile_load(AraVlsu *_this, iss_insn_t *insn)
{
    PendingInsn *pending_insn = _this->insns[_this->insn_first_waiting].insn;

    const uint32_t tss      = (uint32_t)pending_insn->reg;
    const iss_addr_t base   = (iss_addr_t)pending_insn->reg_2;

    // Decode TSS (ZVT spec section 1.5)
    _this->pending_tile_id      = (tss >> 27) & 0xF;
    _this->pending_tile_pattern = (tss >> 24) & 0x7;
    _this->pending_tile_index   = (tss >>  0) & 0xFFFFFF;

    // Tile dimensions from mtype / vl CSRs
    const uint32_t mtype = (uint32_t)_this->ara.iss.csr.mtype.value;
    const uint32_t tm    = (mtype >> 10) & 0x3FFF;
    const uint32_t vl    = (uint32_t)_this->ara.iss.csr.vl.value;
    const uint32_t ETE   = CONFIG_ISS_TE;
    const uint32_t tn    = (vl < ETE) ? vl : ETE;
    _this->pending_tile_count = (_this->pending_tile_pattern == 0) ? tn : tm;

    // Element size: 1 << uim[0]  (0→1B, 1→2B, 2→4B; 3 = 8B = illegal for TEW=32)
    const uint32_t esize = 1u << (uint32_t)insn->uim[0];

    // FSM state
    _this->pending_is_tile  = true;
    _this->pending_is_write = false;
    _this->inst_elem_size   = (int)esize;
    _this->elem_size        = (int)esize;
    _this->pending_addr     = base;
    _this->pending_size     = _this->pending_tile_count * esize;
    _this->strided          = false;
    _this->reg_indexed      = -1;
    _this->pending_elem     = 0;
    _this->pending_vreg     = 0;  // unused for tiles; set to avoid stale value

    // Point the FSM data pointer at the staging buffer.
    // No zero-init needed: the FSM writes exactly pending_size bytes and
    // tile_load_commit() reads only those bytes, zero-extending element-wise.
    _this->pending_velem = _this->tile_staging;
}

// ---------------------------------------------------------------------------
// Tile store (vtse8/16/32/64)
//
// Same operand mapping as tile load.
// Strategy: pre-pack tile.mregs → tile_staging (with truncation), then let
// the FSM drain tile_staging to memory.
// ---------------------------------------------------------------------------
void AraVlsu::handle_insn_tile_store(AraVlsu *_this, iss_insn_t *insn)
{
    PendingInsn *pending_insn = _this->insns[_this->insn_first_waiting].insn;

    const uint32_t tss      = (uint32_t)pending_insn->reg;
    const iss_addr_t base   = (iss_addr_t)pending_insn->reg_2;

    const uint32_t tile_id  = (tss >> 27) & 0xF;
    const uint32_t pattern  = (tss >> 24) & 0x7;
    const uint32_t index    = (tss >>  0) & 0xFFFFFF;

    const uint32_t mtype = (uint32_t)_this->ara.iss.csr.mtype.value;
    const uint32_t tm    = (mtype >> 10) & 0x3FFF;
    const uint32_t vl    = (uint32_t)_this->ara.iss.csr.vl.value;
    const uint32_t ETE   = CONFIG_ISS_TE;
    const uint32_t tn    = (vl < ETE) ? vl : ETE;
    const uint32_t count = (pattern == 0) ? tn : tm;

    const uint32_t esize = 1u << (uint32_t)insn->uim[0];

    // Pack mregs into staging buffer (truncate int32 → esize bytes).
    // esize/pattern switches are hoisted outside the loop to avoid per-element branching.
    // Fast path for esize==4 row: mregs[tile_id][index][*] is contiguous → single memcpy.
    uint8_t *dst = _this->tile_staging;
    if (esize == 4 && pattern == 0)
    {
        memcpy(dst, &_this->ara.iss.tile.mregs[tile_id][index][0], count * 4);
    }
    else
    {
        switch (esize)
        {
        case 1:
            if (pattern == 0)
                for (uint32_t k = 0; k < count; ++k)
                    dst[k] = (uint8_t)(_this->ara.iss.tile.mregs[tile_id][index][k] & 0xFF);
            else
                for (uint32_t k = 0; k < count; ++k)
                    dst[k] = (uint8_t)(_this->ara.iss.tile.mregs[tile_id][k][index] & 0xFF);
            break;
        case 2:
            if (pattern == 0)
                for (uint32_t k = 0; k < count; ++k)
                    *(uint16_t *)(dst + k * 2) = (uint16_t)(_this->ara.iss.tile.mregs[tile_id][index][k] & 0xFFFF);
            else
                for (uint32_t k = 0; k < count; ++k)
                    *(uint16_t *)(dst + k * 2) = (uint16_t)(_this->ara.iss.tile.mregs[tile_id][k][index] & 0xFFFF);
            break;
        case 4:
            // pattern == 1 (column): mregs[tile_id][k][index] is strided, must loop
            for (uint32_t k = 0; k < count; ++k)
                *(uint32_t *)(dst + k * 4) = (uint32_t)_this->ara.iss.tile.mregs[tile_id][k][index];
            break;
        // esize == 8: TEW=32 cannot store 64-bit (should not be reached)
        }
    }

    _this->pending_is_tile   = true;
    _this->pending_tile_id      = tile_id;
    _this->pending_tile_pattern = pattern;
    _this->pending_tile_index   = index;
    _this->pending_tile_count   = count;
    _this->pending_is_write  = true;
    _this->inst_elem_size    = (int)esize;
    _this->elem_size         = (int)esize;
    _this->pending_addr      = base;
    _this->pending_size      = count * esize;
    _this->strided           = false;
    _this->reg_indexed       = -1;
    _this->pending_elem      = 0;
    _this->pending_vreg      = 0;
    _this->pending_velem     = _this->tile_staging;
}

// ---------------------------------------------------------------------------
// tile_load_commit — called by FSM when a tile load finishes (pending_size==0)
//
// The staging buffer holds count * inst_elem_size raw memory bytes.
// Zero-extend each element and scatter into tile.mregs.
// ---------------------------------------------------------------------------
void AraVlsu::tile_load_commit()
{
    const uint32_t tile_id  = this->pending_tile_id;
    const uint32_t pattern  = this->pending_tile_pattern;
    const uint32_t index    = this->pending_tile_index;
    const uint32_t count    = this->pending_tile_count;
    const uint32_t esize    = (uint32_t)this->inst_elem_size;
    const uint8_t *src      = this->tile_staging;

    // esize/pattern switches are hoisted outside the loop to avoid per-element branching.
    // Fast path for esize==4 row: destination is contiguous int32_t[] → single memcpy.
    if (esize == 4 && pattern == 0)
    {
        memcpy(&this->ara.iss.tile.mregs[tile_id][index][0], src, count * 4);
    }
    else
    {
        switch (esize)
        {
        case 1:
            if (pattern == 0)
                for (uint32_t k = 0; k < count; ++k)
                    this->ara.iss.tile.mregs[tile_id][index][k] = (int32_t)(uint32_t)src[k];
            else
                for (uint32_t k = 0; k < count; ++k)
                    this->ara.iss.tile.mregs[tile_id][k][index] = (int32_t)(uint32_t)src[k];
            break;
        case 2:
            if (pattern == 0)
                for (uint32_t k = 0; k < count; ++k)
                    this->ara.iss.tile.mregs[tile_id][index][k] = (int32_t)(uint32_t)*(const uint16_t *)(src + k * 2);
            else
                for (uint32_t k = 0; k < count; ++k)
                    this->ara.iss.tile.mregs[tile_id][k][index] = (int32_t)(uint32_t)*(const uint16_t *)(src + k * 2);
            break;
        case 4:
            // pattern == 1 (column): mregs[tile_id][k][index] is strided, must loop
            for (uint32_t k = 0; k < count; ++k)
                this->ara.iss.tile.mregs[tile_id][k][index] = (int32_t)*(const uint32_t *)(src + k * 4);
            break;
        // esize == 8: not supported (TEW=32)
        }
    }
}

void AraVlsu::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    AraVlsu *_this = (AraVlsu *)__this;

    // In case nothing is on-going, disable the FSM
    if (_this->nb_pending_insn.get() == 0)
    {
        uint8_t zero = 0;
        _this->event_queue.event_highz();
        _this->event_pc.event_highz();
        _this->event_active.event(&zero);
        for (int i=0; i<_this->nb_ports; i++)
        {
            _this->event_addr[i].event_highz();
            _this->event_size[i].event_highz();
            _this->event_is_write[i].event(&zero);
        }
        _this->event_label.event_string((char *)1, false);

        _this->fsm_event.disable();
    }

    // Check if the first waiting instruction can be started
    if (_this->nb_waiting_insn > 0)
    {
        AraVlsuPendingInsn &slot = _this->insns[_this->insn_first_waiting];
        PendingInsn *pending_insn = slot.insn;

        if (pending_insn->timestamp <=_this->ara.iss.top.clock.get_cycles() &&
            _this->pending_size == 0)
        {
            _this->event_label.event_string(pending_insn->insn->desc->label, false);
            _this->event_pc.event((uint8_t *)&pending_insn->pc);

            iss_insn_t *insn = pending_insn->insn;
            ((void (*)(AraVlsu *, iss_insn_t *))insn->decoder_item->u.insn.block_handler)(_this, insn);
        }
    }

    if (_this->pending_size)
    {
        // If a pending request is ready, try to send requests to available ports
        for (int i=0; i<_this->ports.size(); i++)
        {
            if (_this->pending_size == 0) break;

            // Only use this port if it has requests available otherwise it means too many
            // are pending
            if (!_this->req_queues[i]->empty())
            {
                uint64_t size;

                if (_this->strided || _this->reg_indexed != -1)
                {
                    size = _this->elem_size;
                }
                else if (_this->pending_is_tile)
                {
                    size = std::min((iss_addr_t)_this->tile_width, _this->pending_size);
                }
                else
                {
                    size = std::min((iss_addr_t)_this->width, _this->pending_size);
                }

                _this->trace.msg(vp::Trace::LEVEL_TRACE,
                    "Sending request (port: %d, addr: 0x%lx, size: 0x%lx, pending_size: 0x%lx, is_write: %d)\n",
                    i, _this->pending_addr, size, _this->pending_size, _this->pending_is_write);

                _this->event_addr[i].event((uint8_t *)&_this->pending_addr);
                _this->event_size[i].event((uint8_t *)&size);
                _this->event_is_write[i].event((uint8_t *)&_this->pending_is_write);

                /// Pop a request from this port queue to limit number of outstanding requests
                vp::IoReq *req = (vp::IoReq *)_this->req_queues[i]->pop();

                req->prepare();

                iss_reg_t addr = _this->pending_addr;

                if (_this->reg_indexed != -1)
                {
                    uint64_t offset = velem_get_value(&_this->ara.iss, _this->reg_indexed, _this->pending_elem,
                        _this->inst_elem_size, _this->ara.iss.vector.lmul);
                    addr += offset;
                    _this->pending_elem++;
                }

                req->set_addr(addr);
                req->set_is_write(_this->pending_is_write);
                req->set_size(size);

                req->set_data(_this->pending_velem);

                vp::IoReqStatus err = _this->ports[i].req(req);

                // For now only synchronous requests are supported, so for snitch cluster
                // Asynchronous will be needed when having more complex interconnects behind.
                // Then the requests should be handled properly to model the number of on-going
                // transactions.
                if (err != vp::IO_REQ_OK)
                {
                    _this->trace.fatal("Unimplemented async response");
                }

                // Put it back now until asynchronous responses are supported
                _this->req_queues[i]->push_back(req);

                // Prepare the next burst
                if (_this->reg_indexed == -1)
                {
                    _this->pending_addr += _this->strided ? _this->stride : size;
                }
                _this->pending_size -= size;
                _this->pending_velem += size;
                bool was_tile = _this->pending_is_tile;
                // Switch to next instruction once all burst have been sent
                if (_this->pending_size == 0)
                {
                    // Tile load post-processing: extend raw bytes from staging into tile.mregs
                    if (_this->pending_is_tile && !_this->pending_is_write)
                        _this->tile_load_commit();
                    _this->pending_is_tile = false;

                    _this->nb_waiting_insn--;
                    _this->insn_first_waiting = (_this->insn_first_waiting + 1) % AraVlsu::queue_size;
                }

                // In case chaining is enabled, notify that some elements has been handled as it
                // might start an instruction. Tile ops have no vreg to chain on.
                if (!was_tile)
                    _this->ara.insn_commit(_this->pending_vreg, req->get_size());
            }
        }

        // Check if the first enqueued instruction must be removed
        if (_this->nb_pending_insn.get() > 0)
        {
            AraVlsuPendingInsn &slot = _this->insns[_this->insn_first];
            PendingInsn *pending_insn = slot.insn;
            if (_this->pending_size == 0 && slot.nb_pending_bursts == 0 &&
                pending_insn->timestamp <= _this->ara.iss.top.clock.get_cycles())
            {
                _this->insn_first = (_this->insn_first + 1) % AraVlsu::queue_size;
                _this->nb_pending_insn.dec(1);
                _this->ara.insn_end(pending_insn);
            }
        }
    }
}
