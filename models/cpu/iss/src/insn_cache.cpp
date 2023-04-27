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

#include "iss.hpp"
#include <string.h>

static void insn_block_init(Iss *iss, iss_insn_block_t *b, iss_addr_t pc);
void insn_init(iss_insn_t *insn, iss_addr_t addr);

static void flush_cache(Iss *iss, iss_insn_cache_t *cache)
{
    iss->prefetcher.flush();

    cache->insns.clear();

    for (int i = 0; i < ISS_INSN_NB_BLOCKS; i++)
    {
        // Each page already allocated should be kept since various code will not
        // fetch again the pointer after the flush.
        // Just make sure the instruction will be decoded again after the flush.
        iss_insn_block_t *b = cache->blocks[i];
        while (b)
        {
            iss_insn_block_t *next = b->next;
            insn_block_init(iss, b, b->pc);
            b = next;
        }
    }
}

int insn_cache_init(Iss *iss)
{
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    memset(cache->blocks, 0, sizeof(iss_insn_block_t *) * ISS_INSN_NB_BLOCKS);
    return 0;
}

bool insn_cache_is_decoded(Iss *iss, iss_insn_t *insn)
{
    return insn->handler != iss_decode_pc_handler;
}

void insn_init(iss_insn_t *insn, iss_addr_t addr)
{
    insn->handler = iss_decode_pc_handler;
    insn->fast_handler = iss_decode_pc_handler;
    insn->addr = addr;
    insn->hwloop_handler = NULL;
    insn->breakpoint_saved_handler = NULL;
    insn->fetched = false;
    insn->input_latency_reg = -1;
}

static void insn_block_init(Iss *iss, iss_insn_block_t *b, iss_addr_t pc)
{
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    b->is_init = true;
    for (int i = 0; i < ISS_INSN_BLOCK_SIZE; i++)
    {
        iss_insn_t *insn = &b->insns[i];
        insn_init(insn, pc + (i << ISS_INSN_PC_BITS));
        cache->insns[pc + (i << ISS_INSN_PC_BITS)] = insn;
    }
}

static void iss_cache_update(Iss *iss, bool flush)
{
    iss_opcode_t opcode = 0;
    iss_addr_t current_addr = 0;
    iss_addr_t prev_addr = 0;
    iss_addr_t stall_addr = 0;
    iss_addr_t prefetch_addr = 0;
    iss_addr_t hwloop_end_addr[2] = {0};
    bool fetched = false;

    // TODO INSN
    // if (iss->exec.current_insn)
    // {
    //     fetched = iss->exec.current_insn->fetched;
    //     opcode = iss->exec.current_insn->opcode;
    //     current_addr = iss->exec.current_insn->addr;
    // }

    if (flush)
    {
        flush_cache(iss, &iss->decode.insn_cache);
    }

    // if (iss->exec.current_insn)
    // {
    //     iss->exec.current_insn = insn_cache_get(iss, current_addr);
    //     if (fetched)
    //     {
    //         iss->exec.current_insn->opcode = opcode;
    //         iss->exec.current_insn->fetched = true;
    //         iss->decode.decode_pc(iss->exec.current_insn);
    //     }
    // }

    iss->gdbserver.enable_all_breakpoints();

    iss->irq.cache_flush();
}

void iss_cache_sync(Iss *iss)
{
    iss_cache_update(iss, false);
}

void iss_cache_flush(Iss *iss)
{
    iss_cache_update(iss, true);
}

iss_insn_t *insn_cache_get(Iss *iss, iss_addr_t pc)
{
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    iss_insn_t *result = cache->insns[pc];
    if (result != NULL)
    {
        return result;
    }

    iss_insn_t *insn = new iss_insn_t;
    insn_init(insn, pc);
    cache->insns[pc] = insn;

    return insn;

#if 0
    iss_addr_t pc_base = pc & ~((1 << (ISS_INSN_BLOCK_SIZE_LOG2 + ISS_INSN_PC_BITS)) - 1);
    unsigned insn_id = (pc >> ISS_INSN_PC_BITS) & (ISS_INSN_BLOCK_SIZE - 1);
    unsigned int block_id = pc_base & (ISS_INSN_NB_BLOCKS - 1);
    iss_insn_cache_t *cache = &iss->decode.insn_cache;
    iss_insn_block_t *block = cache->blocks[block_id];
    iss_insn_block_t *first_free = NULL;

    while (block)
    {
        if (block->is_init)
        {
            if (block->pc == pc_base)
            {
                return &block->insns[insn_id];
            }
        }
        else
        {
            first_free = block;
        }

        block = block->next;
    }

    iss_insn_block_t *b = first_free;
    if (b == NULL)
    {
        b = (iss_insn_block_t *)malloc(sizeof(iss_insn_block_t));
        b->next = cache->blocks[block_id];
        cache->blocks[block_id] = b;
    }

    b->pc = pc_base;

    insn_block_init(iss, b, pc_base);

    return &b->insns[insn_id];
#endif
}

void Decode::flush_cache_sync(void *__this, bool active)
{
    Decode *_this = (Decode *)__this;
    iss_cache_flush(&_this->iss);
}
