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

#ifdef CONFIG_GVSOC_ISS_V2
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss_v2/include/isa_lib/macros.h"
#else
#include "cpu/iss/include/iss_core.hpp"
#include "cpu/iss/include/isa_lib/int.h"
#include "cpu/iss/include/isa_lib/macros.h"
#endif

#if CONFIG_GVSOC_ISS_FLOAT_USE_SOFTFLOAT
#include "cpu/iss/include/isa_lib/float_softfloat.h"
#endif

#if CONFIG_GVSOC_ISS_FLOAT_USE_FLEXFLOAT
#include "cpu/iss/include/isa_lib/float_flexfloat.h"
#endif

#if CONFIG_GVSOC_ISS_FLOAT_USE_NATIVE
#include "cpu/iss/include/isa_lib/float_native.hpp"
#endif
