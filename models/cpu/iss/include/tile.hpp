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
 * Authors: Pei-Yu Lin (peilin@ethz.ch)
 */

#pragma once
#include <cstdint>
#include <cstring>

// forward declare to avoid heavy include in header
class Iss;

#define MAX_TILES 16

class Tile
{
public:
    explicit Tile(Iss &iss);

    void build();
    void reset(bool active);

    // Call this whenever vector configuration (SEW/LMUL/...) changes
    void sync_with_vector();

    // tile config/state
    uint8_t tm = 0;
    uint8_t tn = 0;
    uint8_t tb = 0;
    uint8_t mtwiden = 0;
    uint8_t kmax = 4;

    // runtime-derived sizes
    uint32_t tew_bits = 0;     // = sew * kmax (and maybe widen)
    uint32_t ete_bits = 0;     // = (tew < 64) ? CONFIG_ISS_TE : (CONFIG_ISS_TE/2)
    uint32_t dim_bytes = 0;    // = ete_bits/8

    static constexpr uint32_t MAX_ETE_BITS  = CONFIG_ISS_TE;
    static constexpr uint32_t MAX_DIM_BYTES = MAX_ETE_BITS / 8;

    // raw byte tile storage (square)
    static constexpr uint32_t MAX_TILE_DIM = 16;  // max tm or tn elements
    int32_t mregs[MAX_TILES][MAX_TILE_DIM][MAX_TILE_DIM];

private:
    Iss &iss;
};