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

#pragma once

#include <vp/vp.hpp>
#include <cpu/iss/include/types.hpp>

class Prefetcher
{
public:
    Prefetcher(Iss &iss);

    // Build the prefetcher, trace will be declared here
    void build();

    // Reset the prefetcher, which will flush it to make it empty
    void reset(bool active);

    // Flush the prefetch buffer
    inline void flush();

    // Response callback for the refill
    static void fetch_response(vp::Block *__this, IO_REQ *req);
#if defined(CONFIG_GVSOC_ISS_LSU_ACC)
    static void fetch_retry(vp::Block *__this);
#endif

    // Refill interface
    IO_MASTER fetch_itf;

    // Fetch the given instruction from prefetch buffer
    inline bool fetch(iss_reg_t pc);

private:
    // Refill of the prefetch buffer
    bool fetch_refill(iss_insn_t *insn, iss_addr_t addr, int index);

    // Callback called when the fetch of the low part is received asynchronously.
    static void fetch_resume_after_low_refill(Prefetcher *_this);

    // Check if the current instruction fits entirely in the buffer and if not trigger another fetch
    bool fetch_check_overflow(iss_insn_t *insn, int index);

    // Callback called when the high part of the instruction is received asynchronously
    static void fetch_resume_after_high_refill(Prefetcher *_this);

    // Send a fetch request to refill the buffer
    int fill(iss_addr_t addr);

    // Send the fetch request
    int send_fetch_req(uint64_t addr, uint8_t *data, uint64_t size, bool is_write);

    // Can be called to stall the core after a pending refill.
    // The core will be unstalled automatically once the refill is done
    inline void handle_stall(void (*callback)(Prefetcher *), iss_insn_t *current_insn);

    // Top component used to access other blocks
    Iss &iss;

    // Prefetch buffer
    uint8_t data[ISS_PREFETCHER_SIZE];

    // Start address of the prefetch buffer. Can be -1 to indicate it is empty
    iss_addr_t buffer_start_addr;

    // Request used for sending fetch request to the fetch interface
    IO_REQ fetch_req;

    // Callback called when a pending fetch response is received
    void (*fetch_stall_callback)(Prefetcher *_this);

    // Instruction being fetched, used by callbacks
    iss_insn_t *prefetch_insn;

    // Pending opcode, used by callback when the instruction is split on 2 lines
    iss_opcode_t fetch_stall_opcode;

    // Prefetcher trace
    vp::Trace trace;

    iss_reg_t current_pc;

};
