/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
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
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#include <vp/vp.hpp>
#include <cpu/iss/include/iss.hpp>

Prefetcher::Prefetcher(Iss &iss)
    : iss(iss)
{
    this->prefetch_insn = NULL;
}

void Prefetcher::build()
{
    this->iss.top.traces.new_trace("prefetcher", &this->trace, vp::DEBUG);
    this->fetch_itf.set_resp_meth(&Prefetcher::fetch_response);
    this->iss.top.new_master_port("fetch", &fetch_itf, (vp::Block *)this);
}

void Prefetcher::reset(bool active)
{
    if (active)
    {
        this->flush();
    }
}

bool Prefetcher::fetch_refill(iss_insn_t *insn, iss_addr_t addr, int index)
{
    // We get here either if the instruction is entirely outside the buffer or if it is only
    // partially within.

    // If the instruction is entirely outside the buffer, first fetch the lower part
    if (unlikely(index < 0 || index >= ISS_PREFETCHER_SIZE))
    {
        if (int err = this->fill(addr))
        {
            if (err == -1)
            {
                // In case of a pending refill, we have to register a callback and leave,
                // we'll get notified when the response is received
                this->handle_stall(fetch_resume_after_low_refill, insn);
            }
            return false;
        }
        index = addr - this->buffer_start_addr;
    }

    // Now check if we can fully get the instruction from the buffer, or it is only partially
    // inside.
    return this->fetch_check_overflow(insn, index);
}

void Prefetcher::fetch_resume_after_low_refill(Prefetcher *_this)
{
    iss_addr_t addr = _this->prefetch_insn->addr;
    int index = addr - _this->buffer_start_addr;
    _this->fetch_check_overflow(_this->prefetch_insn, index);
}

bool Prefetcher::fetch_check_overflow(iss_insn_t *insn, int index)
{
    iss_addr_t addr = insn->addr;

    // Check if we overflow the buffer. If not, the instruction fetch is over
    if (likely(index + ISS_OPCODE_MAX_SIZE <= ISS_PREFETCHER_SIZE))
    {
        insn->opcode = *(iss_opcode_t *)&this->data[index];
    }
    else
    {
        // Case where the opcode is between 2 lines. The prefetcher can only store one line so we have
        // to temporarly store the first part to return it with the next one coming from the final line.
        iss_opcode_t opcode = 0;

        // Compute address of next line
        iss_addr_t next_addr = (this->current_pc + ISS_PREFETCHER_SIZE - 1) & ~(ISS_PREFETCHER_SIZE - 1);
        iss_reg_t next_phys_addr;
        // Number of bytes of the opcode which fits the first line
        int nb_bytes = next_addr - addr;
        int nb_bits = nb_bytes * 8;
        iss_addr_t mask = ((1ULL << (nb_bytes*8)) - 1);
        // Copy first part from first line
        opcode = *(iss_opcode_t *)&this->data[index] & mask;
#ifdef CONFIG_GVSOC_ISS_MMU
        if (this->iss.mmu.insn_virt_to_phys(next_addr, next_phys_addr))
        {
            return false;
        }
#else
        next_phys_addr = next_addr;
#endif

        // Fetch next line
        if (int err = this->fill(next_phys_addr))
        {
            // Stall the core if the fetch is pending
            // We need to remember the opcode since the buffer is fully replaced
            if (err == -1)
            {
                this->fetch_stall_opcode = opcode;
                this->handle_stall(fetch_resume_after_high_refill, insn);
            }
            return false;
        }
        // If the refill is received now, append the second part from second line to the previous opcode
        // and decode it
        opcode = (opcode  & mask) | ((*(iss_opcode_t *)&this->data[0]) << (nb_bytes*8));

        insn->opcode = opcode;
    }

    return true;
}

void Prefetcher::fetch_resume_after_high_refill(Prefetcher *_this)
{
    iss_addr_t addr = _this->prefetch_insn->addr;
    iss_addr_t next_addr = (addr + ISS_PREFETCHER_SIZE - 1) & ~(ISS_PREFETCHER_SIZE - 1);
    // Number of bytes of the opcode which fits the first line
    int nb_bytes = next_addr - addr;

    // And append the second part from second line
    _this->prefetch_insn->opcode = _this->fetch_stall_opcode | ((*(iss_opcode_t *)&_this->data[0]) << (nb_bytes * 8));
}

int Prefetcher::send_fetch_req(uint64_t addr, uint8_t *data, uint64_t size, bool is_write)
{
    vp::IoReq *req = &this->fetch_req;

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Fetch request (addr: 0x%lx, size: 0x%lx)\n", addr, size);

    req->init();
    req->set_addr(addr);
    req->set_size(size);
    req->set_is_write(is_write);
    req->set_data(data);
    vp::IoReqStatus err = this->fetch_itf.req(req);
    if (err != vp::IO_REQ_OK)
    {
        if (err == vp::IO_REQ_INVALID)
        {
#ifndef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
            this->trace.force_warning("Invalid fetch request (addr: 0x%x, size: 0x%x)\n", addr, size);
#endif
            this->iss.exception.raise(this->iss.exec.current_insn, ISS_EXCEPT_INSN_FAULT);
            return 1;
        }
        else
        {
            this->trace.msg(vp::Trace::LEVEL_TRACE, "Waiting for asynchronous response\n");
            return -1;
        }
    }

    this->iss.timing.stall_fetch_account(req->get_latency());

    return 0;
}

int Prefetcher::fill(iss_addr_t addr)
{
    iss_addr_t aligned_addr = addr & ~(ISS_PREFETCHER_SIZE - 1);
    this->buffer_start_addr = aligned_addr;
    return this->send_fetch_req(aligned_addr, this->data, ISS_PREFETCHER_SIZE, false);
}

void Prefetcher::fetch_response(vp::Block *__this, vp::IoReq *req)
{
    Prefetcher *_this = (Prefetcher *)__this;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received fetch response\n");

    // Since a pending response can also include a latency, we need to account it
    // as a stall
    _this->iss.timing.stall_fetch_account(req->get_latency());

    // Now unstall the core and call the fetch callback so that we can continue the refill
    // operation
    _this->iss.exec.stalled_dec();
    if (_this->fetch_stall_callback)
    {
        _this->fetch_stall_callback(_this);
    }
}

