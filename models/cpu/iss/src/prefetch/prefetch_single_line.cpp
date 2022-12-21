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
#include "iss.hpp"

Prefetcher::Prefetcher(Iss &iss)
    : iss(iss)
{
    this->prefetch_insn = NULL;
}

void Prefetcher::build()
{
    this->iss.traces.new_trace("prefetcher", &this->trace, vp::DEBUG);
    this->fetch_itf.set_resp_meth(&Prefetcher::fetch_response);
    this->iss.new_master_port(this, "fetch", &fetch_itf);
}

void Prefetcher::reset(bool active)
{
    if (active)
    {
        this->flush();
    }
}

void Prefetcher::fetch_novalue(void *__this, iss_insn_t *insn)
{
    Prefetcher *_this = (Prefetcher *)__this;

    // Compute where the instructions address falls into the prefetch buffer
    iss_addr_t addr = insn->addr;
    int index = addr - _this->buffer_start_addr;

    // If it is entirely within the buffer, returns nothing to fake a hit.
    if (likely(index >= 0 && index <= ISS_PREFETCHER_SIZE - sizeof(iss_opcode_t)))
    {
        return;
    }

    // Otherwise, fake a refill
    _this->fetch_novalue_refill(insn, addr, index);
}

void Prefetcher::fetch_novalue_refill(iss_insn_t *insn, iss_addr_t addr, int index)
{
    // We get here either if the instruction is entirely outside the buffer or if it is only
    // partially within.

    // If the instruction is entirely outside the buffer, first fetch the lower part
    if (unlikely(index < 0 || index >= ISS_PREFETCHER_SIZE))
    {
        if (this->fill(addr))
        {
            // In case of a pending refill, we have to register a callback and leave,
            // we'll get notified when the response is received
            this->handle_stall(fetch_novalue_resume_after_low_refill, insn);
            return;
        }
        index = addr - this->buffer_start_addr;
    }

    // Now check if we can fully get the instruction from the buffer, or it is only partially
    // inside.
    this->fetch_novalue_check_overflow(insn, index);
}

void Prefetcher::fetch_novalue_resume_after_low_refill(Prefetcher *_this)
{
    // Now that we have received the low part of the instruction
    // check if it fits entirely in the buffer or only partially
    iss_addr_t addr = _this->prefetch_insn->addr;
    int index = addr - _this->buffer_start_addr;
    _this->fetch_novalue_check_overflow(_this->prefetch_insn, index);
}

void Prefetcher::fetch_novalue_check_overflow(iss_insn_t *insn, int index)
{
    // Check if we overflow the buffer. If not, the instruction fetch is over
    if (unlikely(index + ISS_OPCODE_MAX_SIZE > ISS_PREFETCHER_SIZE))
    {
        // If it overflows, trigger a second fetch. No need to remember the low part,
        // since we are faking a fetch
        if (this->fill(this->buffer_start_addr + ISS_PREFETCHER_SIZE))
        {
            // Stall the core if the fetch is pending
            this->handle_stall(NULL, insn);
        }
    }
}

void Prefetcher::fetch_value(void *__this, iss_insn_t *insn)
{
    Prefetcher *_this = (Prefetcher *)__this;

    // Since an instruction can be 2 or 4 bytes, we need to be careful that only part of it can
    // fit the buffer, so we have to check both the low part and the high part.

    // Compute where the instructions address falls into the prefetch buffer
    iss_addr_t addr = insn->addr;
    int index = addr - _this->buffer_start_addr;

    // If it is entirely within the buffer, get the opcode and decode it.
    if (likely(index >= 0 && index <= ISS_PREFETCHER_SIZE - sizeof(iss_opcode_t)))
    {
        insn->opcode = *(iss_opcode_t *)&_this->data[index];
        _this->iss.decode.decode_pc(insn);
        return;
    }

    // If the low part is not within the buffer, fetch it
    if (unlikely(index < 0 || index >= ISS_PREFETCHER_SIZE))
    {
        if (_this->fill(addr))
        {
            // If the response is pending, stall the code and return
            _this->handle_stall(fetch_value_resume_after_low_refill, insn);
            return;
        }

        // Otherwise continue with the higher part
        index = addr - _this->buffer_start_addr;
    }

    // If the low part fits or if we manage to get it synchronously, check if the instruction
    // overflows the buffer.
    _this->fetch_value_check_overflow(insn, index);
}

void Prefetcher::fetch_value_resume_after_low_refill(Prefetcher *_this)
{
    iss_addr_t addr = _this->prefetch_insn->addr;
    int index = addr - _this->buffer_start_addr;
    _this->fetch_value_check_overflow(_this->prefetch_insn, index);
}

void Prefetcher::fetch_value_check_overflow(iss_insn_t *insn, int index)
{
    iss_addr_t addr = insn->addr;

    // Check if we overflow the buffer. If not, the instruction fetch is over
    if (likely(index + ISS_OPCODE_MAX_SIZE <= ISS_PREFETCHER_SIZE))
    {
        insn->opcode = *(iss_opcode_t *)&this->data[index];
        this->iss.decode.decode_pc(insn);
    }
    else
    {
        // Case where the opcode is between 2 lines. The prefetcher can only store one line so we have
        // to temporarly store the first part to return it with the next one coming from the final line.
        iss_opcode_t opcode = 0;

        // Compute address of next line
        uint32_t next_addr = (addr + ISS_PREFETCHER_SIZE - 1) & ~(ISS_PREFETCHER_SIZE - 1);
        // Number of bytes of the opcode which fits the first line
        int nb_bytes = next_addr - addr;
        // Copy first part from first line
        memcpy((void *)&opcode, (void *)&this->data[index], nb_bytes);
        // Fetch next line
        if (this->fill(next_addr))
        {
            // Stall the core if the fetch is pending
            // We need to remember the opcode since the buffer is fully replaced
            this->fetch_stall_opcode = opcode;
            this->handle_stall(fetch_value_resume_after_high_refill, insn);
            return;
        }
        // If the refill is received now, append the second part from second line to the previous opcode
        // and decode it
        opcode = opcode | ((*(iss_opcode_t *)&this->data[0]) << (nb_bytes * 8));

        insn->opcode = opcode;
        this->iss.decode.decode_pc(insn);
    }
}

void Prefetcher::fetch_value_resume_after_high_refill(Prefetcher *_this)
{
    iss_addr_t addr = _this->prefetch_insn->addr;
    uint32_t next_addr = (addr + ISS_PREFETCHER_SIZE - 1) & ~(ISS_PREFETCHER_SIZE - 1);
    // Number of bytes of the opcode which fits the first line
    int nb_bytes = next_addr - addr;

    // And append the second part from second line
    _this->prefetch_insn->opcode = _this->fetch_stall_opcode | ((*(iss_opcode_t *)&_this->data[0]) << (nb_bytes * 8));
    _this->iss.decode.decode_pc(_this->prefetch_insn);
}

int Prefetcher::send_fetch_req(uint64_t addr, uint8_t *data, uint64_t size, bool is_write)
{
    vp::io_req *req = &this->fetch_req;

    this->trace.msg(vp::trace::LEVEL_TRACE, "Fetch request (addr: 0x%x, size: 0x%x)\n", addr, size);

    req->init();
    req->set_addr(addr);
    req->set_size(size);
    req->set_is_write(is_write);
    req->set_data(data);
    vp::io_req_status_e err = this->fetch_itf.req(req);
    if (err != vp::IO_REQ_OK)
    {
        if (err == vp::IO_REQ_INVALID)
        {
            this->trace.force_warning("Invalid fetch request (addr: 0x%x, size: 0x%x)\n", addr, size);
            return 0;
        }
        else
        {
            this->trace.msg(vp::trace::LEVEL_TRACE, "Waiting for asynchronous response\n");
            return -1;
        }
    }

    this->iss.timing.stall_fetch_account(req->get_latency());

    return 0;
}

int Prefetcher::fill(iss_addr_t addr)
{
    uint32_t aligned_addr = addr & ~(ISS_PREFETCHER_SIZE - 1);
    this->buffer_start_addr = aligned_addr;
    return this->send_fetch_req(aligned_addr, this->data, ISS_PREFETCHER_SIZE, false);
}

void Prefetcher::fetch_response(void *__this, vp::io_req *req)
{
    Prefetcher *_this = (Prefetcher *)__this;

    _this->trace.msg(vp::trace::LEVEL_TRACE, "Received fetch response\n");

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
