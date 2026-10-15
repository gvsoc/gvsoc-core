/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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

 #include "cpu/iss/include/iss.hpp"
#include "cpu/iss/include/tile.hpp"

Tile::Tile(Iss &iss) : iss(iss),
    mtwiden(0),
    kmax(CONFIG_ISS_KMAX)
{
}

void Tile::build()
{
    this->sync_with_vector();
}

void Tile::sync_with_vector()
{
    // TEW = SEW × TWIDEN  (kmax is independent of TEW)
    const uint32_t sew_bits  = iss.vector.sewb * 8u;
    const uint32_t twiden    = (this->mtwiden == 0) ? 1u
                               : (1u << (this->mtwiden - 1));
    this->tew_bits = sew_bits * twiden;
    this->ete_bits = (this->tew_bits < 64) ? CONFIG_ISS_TE
                                            : (CONFIG_ISS_TE / 2);
    this->dim_bytes = this->ete_bits / 8;
    if (this->dim_bytes > MAX_DIM_BYTES)
        this->dim_bytes = MAX_DIM_BYTES;
}

void Tile::reset(bool active)
{
    if (!active) return;
    for (int t = 0; t < MAX_TILES; t++)
        std::memset(this->mregs[t], 0,
                    MAX_TILE_DIM * MAX_TILE_DIM * sizeof(int32_t));
}