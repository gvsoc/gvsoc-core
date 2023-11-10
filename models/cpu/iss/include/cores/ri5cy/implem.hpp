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


#include <vp/vp.hpp>
#include <cpu/iss/include/lsu_implem.hpp>
#include <cpu/iss/include/decode_implem.hpp>
#include <cpu/iss/include/trace_implem.hpp>
#include <cpu/iss/include/csr_implem.hpp>
#include <cpu/iss/include/dbgunit_implem.hpp>
#include <cpu/iss/include/exception_implem.hpp>
#include <cpu/iss/include/syscalls_implem.hpp>
#include <cpu/iss/include/timing_implem.hpp>
#include <cpu/iss/include/regfile_implem.hpp>
#include <cpu/iss/include/irq/irq_external_implem.hpp>
#include <cpu/iss/include/exec/exec_inorder_implem.hpp>
#include <cpu/iss/include/prefetch/prefetch_single_line_implem.hpp>
#include <cpu/iss/include/gdbserver_implem.hpp>