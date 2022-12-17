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

static void insn_block_init(iss_insn_block_t *b, iss_addr_t pc);
void insn_init(iss_insn_t *insn, iss_addr_t addr);

static void flush_cache(Iss *iss, iss_insn_cache_t *cache)
{
    iss->prefetcher.flush();

    for (int i = 0; i < ISS_INSN_NB_BLOCKS; i++)
    {
        // Each page already allocated should be kept since various code will not
        // fetch again the pointer after the flush.
        // Just make sure the instruction will be decoded again after the flush.
        iss_insn_block_t *b = cache->blocks[i];
        while (b)
        {
            iss_insn_block_t *next = b->next;
            insn_block_init(b, b->pc);
            b = next;
        }
    }
}

int insn_cache_init(Iss *iss)
{
    iss_insn_cache_t *cache = &iss->insn_cache;
    memset(cache->blocks, 0, sizeof(iss_insn_block_t *) * ISS_INSN_NB_BLOCKS);
    return 0;
}

void insn_init(iss_insn_t *insn, iss_addr_t addr)
{
    insn->handler = iss_decode_pc_handler;
    insn->fast_handler = iss_decode_pc_handler;
    insn->addr = addr;
    insn->next = NULL;
    insn->hwloop_handler = NULL;
    insn->fetched = false;
    insn->input_latency_reg = -1;
}

static void insn_block_init(iss_insn_block_t *b, iss_addr_t pc)
{
    b->is_init = true;
    for (int i = 0; i < ISS_INSN_BLOCK_SIZE; i++)
    {
        iss_insn_t *insn = &b->insns[i];
        insn_init(insn, pc + (i << ISS_INSN_PC_BITS));
    }
}

void iss_cache_flush(Iss *iss)
{
    iss_opcode_t opcode = 0;
    iss_addr_t current_addr = 0;
    iss_addr_t prev_addr = 0;
    iss_addr_t stall_addr = 0;
    iss_addr_t prefetch_addr = 0;
    iss_addr_t hwloop_end_addr[2] = {0};

    if (iss->current_insn)
    {
        opcode = iss->current_insn->opcode;
        current_addr = iss->current_insn->addr;
    }

    if (iss->prev_insn)
    {
        prev_addr = iss->prev_insn->addr;
    }

    if (iss->stall_insn)
    {
        stall_addr = iss->stall_insn->addr;
    }

    // if (iss->prefetch_insn)
    // {
    //   stall_addr = iss->prefetch_insn->addr;
    // }

    if (iss->state.hwloop_end_insn[0])
    {
        hwloop_end_addr[0] = iss->state.hwloop_end_insn[0]->addr;
    }

    if (iss->state.hwloop_end_insn[1])
    {
        hwloop_end_addr[1] = iss->state.hwloop_end_insn[1]->addr;
    }

    flush_cache(iss, &iss->insn_cache);

    if (iss->current_insn)
    {
        iss->current_insn = insn_cache_get(iss, current_addr);
        iss->current_insn->opcode = opcode;
        iss->current_insn->fetched = true;
        iss->decode.decode_pc(iss->current_insn);
    }

    if (iss->prev_insn)
    {
        iss->prev_insn = insn_cache_get(iss, prev_addr);
    }

    if (iss->stall_insn)
    {
        iss->stall_insn = insn_cache_get(iss, stall_addr);
    }

    // if (iss->prefetch_insn)
    // {
    //   iss->prefetch_insn = insn_cache_get(iss, stall_addr);
    // }

    if (iss->state.hwloop_end_insn[0])
    {
        iss->state.hwloop_end_insn[0] = insn_cache_get(iss, hwloop_end_addr[0]);
        hwloop_set_insn_end(iss, iss->state.hwloop_end_insn[0]);
    }

    if (iss->state.hwloop_end_insn[1])
    {
        iss->state.hwloop_end_insn[1] = insn_cache_get(iss, hwloop_end_addr[1]);
        hwloop_set_insn_end(iss, iss->state.hwloop_end_insn[1]);
    }

    iss->irq.cache_flush();
}

iss_insn_t *insn_cache_get(Iss *iss, iss_addr_t pc)
{
    iss_addr_t pc_base = pc & ~((1 << (ISS_INSN_BLOCK_SIZE_LOG2 + ISS_INSN_PC_BITS)) - 1);
    unsigned insn_id = (pc >> ISS_INSN_PC_BITS) & (ISS_INSN_BLOCK_SIZE - 1);
    unsigned int block_id = pc_base & (ISS_INSN_NB_BLOCKS - 1);
    iss_insn_cache_t *cache = &iss->insn_cache;
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

    insn_block_init(b, pc_base);

    return &b->insns[insn_id];
}

void Iss::flush_cache_sync(void *__this, bool active)
{
    Iss *_this = (Iss *)__this;
    if (_this->iss_opened)
    {
        iss_cache_flush(_this);
    }
}

void Iss::flush_cache_ack_sync(void *__this, bool active)
{
    Iss *_this = (Iss *)__this;
    if (_this->state.cache_sync)
    {
        _this->state.cache_sync = false;
        _this->exec.stalled_dec();
        _this->exec.insn_terminate();
    }
}
