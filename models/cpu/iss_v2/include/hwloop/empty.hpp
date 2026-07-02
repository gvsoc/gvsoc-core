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
 * Default Hwloop module slot for cores that don't enable hardware loops.
 * The check() method returns the natural next PC unchanged; the compiler
 * inlines and eliminates it on the dispatch hot path so hwloop-less
 * cores pay zero overhead.
 */

#pragma once

#include <vp/vp.hpp>


class HwloopEmpty
{
public:
    HwloopEmpty(Iss &iss) {}

    void start() {}
    void stop() {}
    void reset(bool active) {}

    inline iss_reg_t check(iss_reg_t pc, iss_reg_t next_pc) { return next_pc; }

    // No-op setters so ISA subsets providing the lp.* instructions
    // (cpu/iss/include/isa/pulp_v2.hpp) compile on cores without the
    // hwloop module — the instructions then behave as nops.
    void set_start(int idx, iss_reg_t pc) {}
    void set_end(int idx, iss_reg_t pc) {}
    void set_count(int idx, iss_reg_t count) {}
};
