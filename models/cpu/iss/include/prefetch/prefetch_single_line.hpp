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
#include <types.hpp>

class Prefetcher
{
public:
    Prefetcher(Iss &iss);

    // Build the prefetcher, trace will be declared here
    int build(vp::component &top);

    // Reset the prefetcher, which will flush it to make it empty
    void reset(bool active);

    // Flush the prefetch buffer
    inline void flush();

    // Fetch the given instruction from prefetch buffer
    inline void fetch(iss_insn_t *insn);

    // Response callback for the refill
    static void fetch_response(void *_this, vp::io_req *req);

private:
    // Fake a fetch of the given instruction from prefetch buffer (for timing).
    inline void fetch_novalue(iss_insn_t *insn);

    // Fake a refill of the prefetch buffer
    void fetch_novalue_refill(iss_insn_t *insn, iss_addr_t addr, int index);

    // Resume a fake refill after the low part of the instruction has been received
    static void fetch_novalue_resume_after_low_refill(Prefetcher *_this);

    // Check if the current instruction fits entirely in the buffer and if not trigger another fetch
    void fetch_novalue_check_overflow(iss_insn_t *insn, int index);

    // Fetch the given instruction from prefetch buffer
    void fetch_value(iss_insn_t *insn);

    // Callback called when the fetch of the low part is received asynchronously.
    static void fetch_value_resume_after_low_refill(Prefetcher *_this);

    // Check if the current instruction fits entirely in the buffer and if not trigger another fetch
    void fetch_value_check_overflow(iss_insn_t *insn, int index);

    // Callback called when the high part of the instruction is received asynchronously
    static void fetch_value_resume_after_high_refill(Prefetcher *_this);

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
    vp::io_req fetch_req;

    // Callback called when a pending fetch response is received
    void (*fetch_stall_callback)(Prefetcher *_this);

    // Instruction being fetched, used by callbacks
    iss_insn_t *prefetch_insn;

    // Pending opcode, used by callback when the instruction is split on 2 lines
    iss_opcode_t fetch_stall_opcode;

    // Prefetcher trace
    vp::trace trace;
};
