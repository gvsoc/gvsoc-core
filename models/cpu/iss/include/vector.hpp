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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */


#pragma once

#define ISS_NB_VREGS 32

class Vector
{
public:
    Vector(Iss &iss);

    void build();
    void reset(bool reset);

    const float LMUL_VALUES[8] = {1.0f, 2.0f, 4.0f, 8.0f, 1.0f, 0.125f, 0.25f, 0.5f};
    const int SEW_VALUES[8] = {8,16,32,64,128,256,512,1024};

    int   sew    = SEW_VALUES[2];
    float lmul   = LMUL_VALUES[0];
    unsigned int sewb = 8;
    uint8_t exp;
    uint8_t mant;

    uint8_t vregs[ISS_NB_VREGS][CONFIG_ISS_VLEN/8];
};
