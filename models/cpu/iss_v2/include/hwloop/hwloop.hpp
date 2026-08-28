/*
 * Copyright (C) 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

/*
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 *
 * Hardware-loop module for iss_v2. Self-contained per-core state: each
 * configured loop has a (start_pc, end_pc, count) triple. After every
 * successfully executed instruction the dispatch loop calls check();
 * if the just-executed PC matches a loop's end and the counter is
 * non-zero, the counter is decremented and the next PC is redirected
 * to the loop start.
 *
 * Single source of truth: the LPSTART/LPEND/LPCOUNT CSRs route their
 * reads/writes through this module instead of carrying a parallel
 * shadow inside csr.hpp.
 *
 * Cores without hwloop support use the HwloopEmpty variant
 * (include/hwloop/empty.hpp) — its check() inlines to a no-op.
 */

#pragma once

#include <vp/vp.hpp>


#ifndef CONFIG_GVSOC_ISS_NB_HWLOOP
#define CONFIG_GVSOC_ISS_NB_HWLOOP 2
#endif


class Hwloop
{
public:
    Hwloop(Iss &iss);

    void start() {}
    void stop() {}
    void reset(bool active);

    // Hot-path post-execute hook. Returns the redirected PC if the
    // just-executed instruction is a loop end with a non-zero counter,
    // otherwise returns next_pc unchanged.
    inline iss_reg_t check(iss_reg_t pc, iss_reg_t next_pc);

    // Setters (called from ISA decoders and CSR writes). Header-inline on
    // purpose: external controllers living in a separate shared object can
    // only call what inlines (the model library does not export symbols).
    void set_start(int idx, iss_reg_t pc)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Setting hwloop start (idx: %d, pc: 0x%lx)\n", idx, (unsigned long)pc);
        this->start_pc[idx] = pc;
    }

    void set_end(int idx, iss_reg_t pc)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Setting hwloop end (idx: %d, pc: 0x%lx)\n", idx, (unsigned long)pc);
        this->end_pc[idx] = pc;
    }

    void set_count(int idx, iss_reg_t count)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Setting hwloop count (idx: %d, count: %d)\n", idx, (int)count);
        this->count[idx] = count;
        if (count == 0)
        {
            this->active &= ~(1u << idx);
        }
        else
        {
            this->active |= (1u << idx);
        }
    }

    // Getters (called from CSR reads).
    iss_reg_t get_start(int idx) const { return this->start_pc[idx]; }
    iss_reg_t get_end(int idx) const   { return this->end_pc[idx]; }
    iss_reg_t get_count(int idx) const { return this->count[idx]; }

private:
    Iss &iss;
    vp::Trace trace;

    iss_reg_t start_pc[CONFIG_GVSOC_ISS_NB_HWLOOP];
    iss_reg_t end_pc[CONFIG_GVSOC_ISS_NB_HWLOOP];
    iss_reg_t count[CONFIG_GVSOC_ISS_NB_HWLOOP];
    // Bit i is set when count[i] > 0; the hot-path check fast-exits on
    // active == 0.
    uint32_t active;
};
