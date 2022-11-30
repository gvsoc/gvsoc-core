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


#include "iss_core.hpp"
#include "isa_lib/int.h"
#include "isa_lib/macros.h"



static inline iss_insn_t *lwu_exec_fast(iss_t *iss, iss_insn_t *insn)
{
  iss_lsu_load(iss, insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0));
  return insn->next;
}



static inline iss_insn_t *lwu_exec(iss_t *iss, iss_insn_t *insn)
{
  iss_lsu_check_stack_access(iss, REG_IN(0), REG_GET(0) + SIM_GET(0));
  iss_lsu_load_perf(iss, insn, REG_GET(0) + SIM_GET(0), 4, REG_OUT(0));
  return insn->next;
}



static inline iss_insn_t *ld_exec_fast(iss_t *iss, iss_insn_t *insn)
{
  iss_lsu_load_signed(iss, insn, REG_GET(0) + SIM_GET(0), 8, REG_OUT(0));
  return insn->next;
}



static inline iss_insn_t *ld_exec(iss_t *iss, iss_insn_t *insn)
{
  iss_lsu_check_stack_access(iss, REG_IN(0), REG_GET(0) + SIM_GET(0));
  iss_lsu_load_signed_perf(iss, insn, REG_GET(0) + SIM_GET(0), 8, REG_OUT(0));
  return insn->next;
}



static inline iss_insn_t *sd_exec_fast(iss_t *iss, iss_insn_t *insn)
{
  iss_lsu_store(iss, insn, REG_GET(0) + SIM_GET(0), 8, REG_IN(1));
  return insn->next;
}



static inline iss_insn_t *sd_exec(iss_t *iss, iss_insn_t *insn)
{
  iss_lsu_check_stack_access(iss, REG_OUT(0), REG_GET(0) + SIM_GET(0));
  iss_lsu_store_perf(iss, insn, REG_GET(0) + SIM_GET(0), 8, REG_IN(1));
  return insn->next;
}



static inline iss_insn_t *addiw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_ADDW, REG_GET(0), SIM_GET(0)));
  return insn->next;
}



static inline iss_insn_t *slliw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_SLLW, REG_GET(0), UIM_GET(0)));
  return insn->next;
}



static inline iss_insn_t *srliw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_SRLW, REG_GET(0), UIM_GET(0)));
  return insn->next;
}



static inline iss_insn_t *sraiw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_SRAW, REG_GET(0), UIM_GET(0)));
  return insn->next;
}



static inline iss_insn_t *addw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_ADDW, REG_GET(0), REG_GET(1)));
  return insn->next;
}



static inline iss_insn_t *subw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_SUBW, REG_GET(0), REG_GET(1)));
  return insn->next;
}



static inline iss_insn_t *sllw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_SLLW, REG_GET(0), REG_GET(1) & ((1<<5)-1)));
  return insn->next;
}



static inline iss_insn_t *srlw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_SRLW, REG_GET(0), REG_GET(1) & ((1<<5)-1)));
  return insn->next;
}



static inline iss_insn_t *sraw_exec(iss_t *iss, iss_insn_t *insn)
{
  REG_SET(0, LIB_CALL2(lib_SRAW, REG_GET(0), REG_GET(1) & ((1<<5)-1)));
  return insn->next;
}
