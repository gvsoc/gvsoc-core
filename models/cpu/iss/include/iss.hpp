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

#ifndef __CPU_ISS_ISS_HPP
#define __CPU_ISS_ISS_HPP

#include "types.hpp"
#include ISS_CORE_INC(class.hpp)
#include "iss_core.hpp"

#include <lsu_implem.hpp>

#include "perf_event.hpp"
#include <csr.hpp>
#include <dbgunit.hpp>
#include <syscalls.hpp>
#include "timing.hpp"
#include "rv64i.hpp"
#include "rv32i.hpp"
// #include "rv32v.hpp"
#include "rv32c.hpp"
#include "rv32a.hpp"
#include "rv64c.hpp"
#include "rv32m.hpp"
#include "rv64m.hpp"
#include "rv64a.hpp"
#include "rvf.hpp"
#include "rvd.hpp"
#include "rvXf16.hpp"
#include "rvXf16alt.hpp"
#include "rvXf8.hpp"
#include "rv32Xfvec.hpp"
#include "rv32Xfaux.hpp"
#include "priv.hpp"
#include "pulp_v2.hpp"
#include "rvXgap9.hpp"
#include "rvXint64.hpp"
#include "rnnext.hpp"
#include "pulp_nn.hpp"
#include "corev.hpp"


#include <regfile_implem.hpp>
#include <timing_implem.hpp>
#ifdef CONFIG_GVSOC_ISS_RISCV_EXCEPTIONS
#include <irq/irq_riscv_implem.hpp>
#else
#include <irq/irq_external_implem.hpp>
#endif
#include <mmu_implem.hpp>
#include <exec/exec_inorder_implem.hpp>
#include <prefetch/prefetch_single_line_implem.hpp>


#endif
