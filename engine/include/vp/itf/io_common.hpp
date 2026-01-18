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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#pragma once

#include "vp/queue.hpp"
#include "vp/vp.hpp"

namespace vp {

typedef enum {
    READ = 0,
    WRITE = 1,
    LR = 2,
    SC = 3,
    SWAP = 4,
    ADD = 5,
    XOR = 6,
    AND = 7,
    OR = 8,
    MIN = 9,
    MAX = 10,
    MINU = 11,
    MAXU = 12,
} IoReqOpcode;

};
