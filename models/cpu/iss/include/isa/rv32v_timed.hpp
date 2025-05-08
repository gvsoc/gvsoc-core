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

#pragma once

#define RVV_FREG_GET(reg) (iss->ara.current_insn_reg_get())
#define RVV_REG_GET(reg) (iss->ara.current_insn_reg_get())

static inline uint64_t convert_scalar_to_vector(uint64_t val, unsigned int sew)
{
    if (ISS_REG_WIDTH < sew)
    {
        return ((int64_t)val) << (sew - ISS_REG_WIDTH) >> (sew - ISS_REG_WIDTH);
    }

    return val;
}

static inline bool velem_is_active(Iss *iss, unsigned int elem, unsigned int vm)
{
    unsigned int reg = elem / 8;
    unsigned int pos = elem % 8;
    return vm | ((iss->vector.vregs[0][reg] >> pos) & 1);
}

static inline uint8_t *velem_get(Iss *iss, unsigned int reg, unsigned int elem, unsigned int sewb,
    unsigned int lmul)
{
    return &iss->vector.vregs[reg][elem*sewb];
}

static inline uint64_t velem_get_value(Iss *iss, unsigned int reg, unsigned int elem, unsigned int sewb,
    unsigned int lmul)
{
    uint8_t *ptr = &iss->vector.vregs[reg][elem*sewb];
    switch (sewb)
    {
        case 1:
            return *ptr;
        case 2:
            return *(uint16_t *)ptr;
        case 4:
            return *(uint32_t *)ptr;
        default:
            return *(uint64_t *)ptr;
    }
}

static inline void velem_set_value(Iss *iss, unsigned int reg, unsigned int elem, unsigned int sewb,
    uint64_t value)
{
    uint8_t *ptr = &iss->vector.vregs[reg][elem*sewb];
    switch (sewb)
    {
        case 1:
            *ptr = value;
            break;
        case 2:
            *(uint16_t *)ptr = value;
            break;
        case 4:
            *(uint32_t *)ptr = value;
            break;
        default:
            *(uint64_t *)ptr = value;
            break;
    }
}

static inline iss_reg_t vadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vadd_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vadd_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsub_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vrsub_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vrsub_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vand_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vand_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vand_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vor_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vor_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vor_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vxor_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vxor_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vxor_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmin_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmin_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vminu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vminu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmax_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmax_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmaxu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmaxu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmul_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmulh_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmulh_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmulhu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmulhu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmulhsu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}
static inline iss_reg_t vmulhsu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmerge_vvm_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        int reg = velem_is_active(iss, i, 0) ? REG_IN(0) : REG_IN(1);
        uint64_t in = velem_get_value(iss, reg, i, sewb, lmul);
        velem_set_value(iss, REG_OUT(0), i, sewb, in);
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_v_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        uint64_t in = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
        velem_set_value(iss, REG_OUT(0), i, sewb, in);
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_v_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    uint64_t in = convert_scalar_to_vector(REG_GET(0), sewb * 8);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            velem_set_value(iss, REG_OUT(0), i, sewb, in);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_v_i_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    uint64_t in = convert_scalar_to_vector(SIM_GET(0), sewb * 8);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            velem_set_value(iss, REG_OUT(0), i, sewb, in);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_s_x_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    uint64_t in = convert_scalar_to_vector(REG_GET(0), sewb * 8);
    velem_set_value(iss, REG_OUT(0), 0, sewb, in);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmv_x_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmul_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulsu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmulsu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmacc_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vmadd_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsac_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vnmsub_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmacc_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccus_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccsu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vwmaccsu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredsum_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredand_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredor_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredxor_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredmin_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredminu_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredmax_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vredmaxu_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}


static inline iss_reg_t vslideup_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslideup_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslidedown_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslidedown_vi_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslide1up_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vslide1down_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vdiv_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vdiv_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vdivu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vdivu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vrem_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vrem_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vremu_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vremu_vx_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vle8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vle16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vle32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vle64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vse8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vse16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vse32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vse64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // The instruction handler is empty as the VLSU block is taking care of the memory access
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl1r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl1re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl1re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl1re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl1re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl2r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl2re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl2re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl2re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl2re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl4r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl4re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl4re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl4re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl4re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl8r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl8re8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl8re16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl8re32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vl8re64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vs1r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vs2r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vs4r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vs8r_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vluxei8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vluxei16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vluxei32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vluxei64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsuxei8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsuxei16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsuxei32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsuxei64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vlse8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vlse16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vlse32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vlse64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsse8_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsse16_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsse32_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsse64_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfslide1down_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int reg_in = REG_IN(1);
    unsigned int reg_out = REG_OUT(0);
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    iss_reg_t last_elem = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint8_t *in = i == iss->csr.vl.value - 1 ?
                (uint8_t *)&last_elem : velem_get(iss, reg_in, i + 1, sewb, lmul);

            uint8_t *out = velem_get(iss, reg_out, i, sewb, lmul);

            memcpy(out, in, sewb);
        }
    }

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL3(lib_flexfloat_add_round, in0, in1, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL3(lib_flexfloat_add_round, in0, in1, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL3(lib_flexfloat_sub_round, in1, in0, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL3(lib_flexfloat_sub_round, in1, in0, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfrsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL3(lib_flexfloat_sub_round, in0, in1, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL2(lib_flexfloat_min, in0, in1, iss->vector.exp, iss->vector.mant);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmin_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL2(lib_flexfloat_min, in0, in1, iss->vector.exp, iss->vector.mant);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL2(lib_flexfloat_max, in0, in1, iss->vector.exp, iss->vector.mant);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmax_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL2(lib_flexfloat_max, in0, in1, iss->vector.exp, iss->vector.mant);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL3(lib_flexfloat_mul_round, in0, in1, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL3(lib_flexfloat_mul_round, in0, in1, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmacc_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_madd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_madd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmacc_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmadd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmadd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_msub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_msub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmsub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmsub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_madd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_madd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmadd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmadd_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_msub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_msub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in0 = RVV_FREG_GET(0);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmsub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);

            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfnmsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in0 = velem_get_value(iss, REG_IN(0), i, sewb, lmul);
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            uint64_t in2 = velem_get_value(iss, REG_IN(2), i, sewb, lmul);
            uint64_t res = LIB_FF_CALL4(lib_flexfloat_nmsub_round, in0, in1, in2, iss->vector.exp, iss->vector.mant, 7);
            velem_set_value(iss, REG_OUT(0), i, sewb, res);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredmax_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredmin_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredsum_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t res = velem_get_value(iss, REG_IN(0), 0, sewb, lmul);
    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            uint64_t in1 = velem_get_value(iss, REG_IN(1), i, sewb, lmul);
            res = LIB_FF_CALL3(lib_flexfloat_add_round, res, in1, iss->vector.exp, iss->vector.mant, 7);
        }
    }
    velem_set_value(iss, REG_OUT(0), 0, sewb, res);
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfredosum_vs_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwadd_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwadd_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwadd_wv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwadd_wf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwsub_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwsub_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwsub_wv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwsub_wf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmul_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmul_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmacc_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmacc_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwnmsac_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfwnmsac_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnj_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjn_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_vv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfsgnjx_vf_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_xu_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_x_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_f_xu_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_f_x_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_rtz_xu_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfcvt_rtz_x_f_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_xu_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_x_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_f_xu_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_f_x_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_f_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_rod_f_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_rtz_xu_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfncvt_rtz_x_f_w_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    abort();
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_v_f_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;

    uint64_t value = FREG_GET(0);

    for (unsigned int i=iss->csr.vstart.value; i<iss->csr.vl.value; i++)
    {
        if (velem_is_active(iss, i, UIM_GET(0)))
        {
            velem_set_value(iss, REG_OUT(0), i, sewb, value);
        }
    }

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_s_f_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;

    uint64_t value = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, FREG_GET(0), CONFIG_GVSOC_ISS_FP_EXP, CONFIG_GVSOC_ISS_FP_MANT,
        iss->vector.exp, iss->vector.mant, 7);

    velem_set_value(iss, REG_IN(0), 0, sewb, value);

    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vfmv_f_s_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    unsigned int sewb = iss->vector.sewb;
    unsigned int lmul = iss->vector.LMUL_t;
    uint64_t in = velem_get_value(iss, REG_IN(0), 0, sewb, lmul);
    FREG_SET(0, LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, in, iss->vector.exp, iss->vector.mant,
        CONFIG_GVSOC_ISS_FP_EXP, CONFIG_GVSOC_ISS_FP_MANT, 7));
    return iss_insn_next(iss, insn, pc);
}

#if ISS_REG_WIDTH == 64
#define VTYPE_VALUE 0x8000000000000000
#else
#define VTYPE_VALUE 0x80000000
#endif

#define SEW iss->vector.SEW_t
#define LMUL iss->vector.LMUL_t
#define VL iss->csr.vl.value
#define VSTART iss->csr.vstart.value

static inline iss_reg_t vlmax_get(Iss *iss)
{
    return iss->vector.VLEN / 8 * iss->vector.LMUL_t / iss->vector.sewb;
}

static inline void EMCase(int sew, uint8_t *m, uint8_t *e){
    switch (sew){
    case 8:{
        printf("8 bit is not supported for FLOAT\n");
        return;
        break;
    }
    case 16:{
        *e = 5;
        *m = 10;
        break;
    }
    case 32:{
        *e = 8;
        *m = 23;
        break;
    }
    case 64:{
        *e = 11;
        *m = 52;
        break;
    }
    default:
        break;
    }
}

static inline iss_reg_t lib_VSETVLI(Iss *iss, int idxRs1, int idxRd, int rs1, iss_uim_t lmul, iss_uim_t sew, iss_uim_t vtype){
    uint32_t AVL;

    if((int)(vtype/pow(2,31))){
        iss->csr.vtype.value = VTYPE_VALUE;
        VL = 0;
        AVL = 0;
        return 0;
    }else if((lmul==5 && (sew == 1 || sew == 2 || sew ==3)) || (lmul==6 && (sew == 2 || sew ==3)) || (lmul==7 && sew==3)){
        iss->csr.vtype.value = VTYPE_VALUE;
        VL = 0;
        AVL = 0;
        return 0;
    }else{
        iss->csr.vtype.value = vtype;
    }


        VSTART = 0;
        SEW = iss->vector.SEW_VALUES[sew];
        LMUL = iss->vector.LMUL_VALUES[lmul];
        iss->vector.sewb = 1 << sew;
        EMCase(iss->vector.sewb * 8, &iss->vector.mant, &iss->vector.exp);

        if(idxRs1){
            AVL = rs1;
            VL = MIN(AVL,vlmax_get(iss));
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            VL  = vlmax_get(iss);
        }else{
            AVL = VL;
        }

    return VL;
}

static inline iss_reg_t lib_VSETVL(Iss *iss, int idxRs1, int idxRd, int rs1, int rs2){

    uint32_t AVL;


    iss->csr.vtype.value = rs2;
    int sew = (rs2/8)%8;
    int lmul = rs2%8;
    if((int)(rs2/pow(2,31))){
        iss->csr.vtype.value = VTYPE_VALUE;
        VL = 0;
        AVL = 0;
        return 0;
    }else if((lmul==5 && (sew == 1 || sew == 2 || sew ==3)) || (lmul==6 && (sew == 2 || sew ==3)) || (lmul==7 && sew==3)){
        iss->csr.vtype.value = VTYPE_VALUE;
        VL = 0;
        AVL = 0;
        return 0;
    }else{
        iss->csr.vtype.value = rs2;
        VSTART = 0;
        SEW = iss->vector.SEW_VALUES[sew];
        LMUL = iss->vector.LMUL_VALUES[lmul];

        if(idxRs1){
            AVL = rs1;
            VL = MIN(AVL,vlmax_get(iss));
        }else if(!idxRs1 && idxRd){
            AVL = UINT32_MAX;
            VL  = vlmax_get(iss);
        }else{
            AVL = VL;
        }

    return VL;
    }
}

static inline iss_reg_t vsetvli_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL6(lib_VSETVLI, REG_IN(0), REG_OUT(0), RVV_REG_GET(0), UIM_GET(0), UIM_GET(1), UIM_GET(2)));// VLMUL-VSEW
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vsetvl_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    REG_SET(0, LIB_CALL4(lib_VSETVL, REG_IN(0), REG_OUT(0), REG_GET(0), REG_GET(1)));
    return iss_insn_next(iss, insn, pc);
}
