/*
 * Copyright (C) 2026 ETH Zurich and University of Bologna
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

#pragma once

#include <cpu/iss_v2/include/types.hpp>

// Base of the core classes (CONFIG_GVSOC_ISS_ARCH). It gives the hooks the ISS
// calls on the core class, empty by default. A core class implements a hook by
// defining a method with the same name; the ISS uses the concrete class, so the
// call is resolved statically and an empty hook costs nothing.
class IssArch
{
public:
    // Called by the decoder on every decoded instruction, after its stub
    // handlers are installed
    inline void decode_hook(iss_insn_t *insn) {}
};
