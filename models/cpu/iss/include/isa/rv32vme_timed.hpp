/*
 * Copyright (C) 2024 ETH Zurich and University of Bologna
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
#include <stdint.h>

#define CSR_VTYPE_VALUE(iss) ((uint32_t)((iss)->csr.vtype.value))
#define CSR_MTYPE_VALUE(iss) ((uint32_t)((iss)->csr.mtype.value))
#define ALTFMT               ((bool)((((iss)->csr.vtype.value) >> 8) & 0x1))
#define VTMM_ILLEGAL(iss, insn, pc) do { return (pc); } while(0)
#define TEW 32
#define TEWB (TEW/8)

// =============================================================
// mtype CSR field accessors
// bits 23:10 = tm[13:0]
// bits  7:5  = tk[2:0]
// bits  1:0  = mtwiden[1:0]
// =============================================================

static inline uint32_t mtype_tm(uint32_t mtype)      { return (mtype >> 10) & 0x3FFF; }
static inline uint32_t mtype_tk(uint32_t mtype)      { return (mtype >>  5) & 0x7;    }
static inline uint32_t mtype_mtwiden(uint32_t mtype) { return (mtype >>  0) & 0x3;    }

// =============================================================
// Tile accumulator access macros
// =============================================================

// # TODO: #Tiles should adapt to different TEW
#define TILE_ACC_GET_I32(iss, tile_id, r, c) \
    ((int32_t)((iss)->tile.mregs[(tile_id)][(r)][(c)]))

#define TILE_ACC_SET_I32(iss, tile_id, r, c, v) \
    do { (iss)->tile.mregs[(tile_id)][(r)][(c)] = (int32_t)(v); } while(0)
    
#define TILE_ACC_GET_F(iss, tile_id, r, c) \
    ((uint64_t)(uint32_t)((iss)->tile.mregs[(tile_id)][(r)][(c)]))

#define TILE_ACC_SET_F(iss, tile_id, r, c, v) \
    do { (iss)->tile.mregs[(tile_id)][(r)][(c)] = (int32_t)(uint32_t)(v); } while(0)

static inline uint32_t get_ete(void) {
    return (TEW < 64) ? CONFIG_ISS_TE : (CONFIG_ISS_TE / 2);
}
static inline uint32_t get_tn(Iss *iss) {
    const uint32_t ETE = get_ete();
    return (iss->csr.vl.value < ETE) ? (uint32_t)iss->csr.vl.value : ETE;
}

// =============================================================
// Helper: decode TSS into tile_id, pattern, index
// =============================================================
static inline void tss_decode(uint32_t  tss, uint32_t *tile_id,
                                   uint32_t *pattern, uint32_t *index) {
    *tile_id = (tss >> 27) & 0xF;
    *pattern = (tss >> 24) & 0x7;
    *index   = (tss >>  0) & 0xFFFFFF;
}

// =============================================================
// Supported combinations from the spec:
//
// TEW=8:   SEW=8,  TWIDEN=1 -> KMAX=4
// TEW=16:  SEW=8,  TWIDEN=2 -> KMAX=4
//          SEW=16, TWIDEN=1 -> KMAX=2
// TEW=32:  SEW=8,  TWIDEN=4 -> KMAX=4
//          SEW=16, TWIDEN=2 -> KMAX=2
//          SEW=32, TWIDEN=1 -> KMAX=1
// TEW=64:  SEW=16, TWIDEN=4 -> KMAX=2
//          SEW=32, TWIDEN=2 -> KMAX=1
//          SEW=64, TWIDEN=1 -> KMAX=1
//
// Successive rows are separated by (8 / KMAX) vector registers.
// =============================================================

static inline uint32_t valid_kmax(uint32_t sew_bytes, uint32_t twiden) {
    switch (sew_bytes) {
        case 1: return (twiden == 1 || twiden == 2 || twiden == 4) ? 4 : 0;
        case 2: return (twiden == 1 || twiden == 2 || twiden == 4) ? 2 : 0;
        case 4: return (twiden == 1 || twiden == 2) ? 1 : 0;
        case 8: return (twiden == 1) ? 1 : 0;
        default: return 0;
    }
}

static inline uint32_t valid_row_stride(uint32_t sew_bytes, uint32_t twiden) {
    const uint32_t kmax = valid_kmax(sew_bytes, twiden);
    return kmax ? (8u / kmax) : 0;
}

// =============================================================
// msetmtypei
// UIM(0) = mtype[4:0] from bits [19:15]
// UIM(1) = vsew[1:0]  from bits [24:23]
// =============================================================

static inline iss_reg_t msetmtypei_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    const uint32_t mtypei = (uint32_t)UIM_GET(0);
    const uint32_t vsew   = (uint32_t)UIM_GET(1);
    const uint32_t mtwiden = mtypei & 0x3;

    iss->csr.mtype.value = mtypei & 0x1F;

    uint32_t new_vtype = (vsew & 0x7) << 2;
    if (mtwiden != 0) {
        new_vtype |= (1 << 7);  // vma = 1
        new_vtype |= (1 << 6);  // vta = 1
    }

    iss->csr.vtype.value = new_vtype;
    iss->csr.vl.value    = 0;
    iss->vector.sewb     = 1 << ((new_vtype >> 2) & 0x7);
    iss->vector.lmul     = new_vtype & 0x7;

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// msetmtype
// REG_GET(0) = rs1 = new mtype value
// REG_GET(1) = rs2 = new vtype value
// =============================================================

static inline iss_reg_t msetmtype_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    const uint32_t new_mtype = (uint32_t)REG_GET(0);
    const uint32_t new_vtype = (uint32_t)REG_GET(1);

    iss->csr.mtype.value = new_mtype;
    iss->csr.vl.value    = 0;
    iss->csr.vtype.value = new_vtype;
    iss->vector.sewb     = 1 << ((new_vtype >> 2) & 0x7);
    iss->vector.lmul     = new_vtype & 0x7;

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// msettn
// REG_GET(0) = rs1 = requested tn
// REG_SET(0)       = rd  = new vl/tn
// =============================================================

static inline iss_reg_t msettn_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    const uint32_t mtwiden = mtype_mtwiden(mtype);
    const uint32_t rs1_val = (uint32_t)REG_GET(0);

    uint32_t new_vl = 0;

    if (mtwiden == 0) {
        const uint32_t sewb  = iss->vector.sewb;
        const uint32_t lmul  = iss->vector.lmul;
        uint32_t vlmax = (CONFIG_ISS_VLEN / 8 / sewb) * (1 << (lmul & 0x3));
        new_vl = (rs1_val < vlmax) ? rs1_val : vlmax;
    } else {
        const uint32_t ETE   = get_ete();
        const uint32_t sewb  = iss->vector.sewb;
        const uint32_t lmul  = iss->vector.lmul;
        uint32_t EVE   = CONFIG_ISS_VLEN / 8 / sewb;
        uint32_t vlmax = EVE * (1 << (lmul & 0x3));
        uint32_t limit = (vlmax < ETE) ? vlmax : ETE;
        new_vl = (rs1_val < limit) ? rs1_val : limit;
    }

    iss->csr.vl.value = new_vl;
    REG_SET(0, new_vl);

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// msettm
// REG_GET(0) = rs1 = requested tm
// REG_SET(0)       = rd  = new tm
// =============================================================

static inline iss_reg_t msettm_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    const uint32_t mtwiden = mtype_mtwiden(mtype);
    const uint32_t rs1_val = (uint32_t)REG_GET(0);

    uint32_t new_tm = 0;

    if (mtwiden != 0) {
        const uint32_t ETE   = get_ete();
        const uint32_t sewb  = iss->vector.sewb;
        const uint32_t lmul  = iss->vector.lmul;
        uint32_t EVE   = CONFIG_ISS_VLEN / 8 / sewb;
        uint32_t vlmax = EVE * (1 << (lmul & 0x3));
        uint32_t limit = (vlmax < ETE) ? vlmax : ETE;
        new_tm = (rs1_val < limit) ? rs1_val : limit;
    }

    uint32_t cur = iss->csr.mtype.value;
    cur = (cur & ~(0x3FFF << 10)) | ((new_tm & 0x3FFF) << 10);
    iss->csr.mtype.value = cur;
    REG_SET(0, new_tm);

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// msettk
// REG_GET(0) = rs1 = requested tk
// REG_SET(0)       = rd  = new tk
// =============================================================

static inline iss_reg_t msettk_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    const uint32_t mtwiden = mtype_mtwiden(mtype);
    const uint32_t rs1_val = (uint32_t)REG_GET(0);

    uint32_t new_tk = 0;

    if (mtwiden != 0) {
        const uint32_t KMAX = 4;
        new_tk = (rs1_val < KMAX) ? rs1_val : KMAX;
    }

    uint32_t cur = iss->csr.mtype.value;
    cur = (cur & ~(0x7 << 5)) | ((new_tk & 0x7) << 5);
    iss->csr.mtype.value = cur;
    REG_SET(0, new_tk);

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// vtmmu.tvv — unsigned integer matmul
//   A: uint8 (vs2), always unsigned
//   B: uint8 (altfmt=0) or int8 (altfmt=1) (vs1)
//   C: int32 accumulator
// =============================================================
static inline iss_reg_t vtmmu_tvv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (VSTART != 0)
        VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    uint32_t       tm      = mtype_tm(mtype);
    uint32_t       tk      = mtype_tk(mtype);
    const uint32_t mtwiden = mtype_mtwiden(mtype);

    if (mtwiden == 0) return iss_insn_next(iss, insn, pc);
    if (tk > 4) tk = 4;

    const uint32_t tn  = get_tn(iss);
    const uint32_t twiden = 1u << (mtwiden - 1);

    if (tm == 0 || tn == 0 || tk == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t sewb = iss->vector.sewb;
    if (sewb != 1)
        VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t lmul       = iss->vector.lmul;
    const uint32_t base_vs1   = (uint32_t)REG_IN(0);   // B
    const uint32_t base_vs2   = (uint32_t)REG_IN(1);   // A
    const uint32_t row_stride = valid_row_stride(sewb, twiden);  // 2
    const uint32_t tile_id    = (uint32_t)REG_OUT(0) & 0xF;

    const bool b_signed = ALTFMT;

    if ((tile_id & 0x3) != 0)
        VTMM_ILLEGAL(iss, insn, pc);

    for (uint32_t i = 0; i < tm; ++i)
    {
        for (uint32_t j = 0; j < tn; ++j)
        {
            int32_t acc = TILE_ACC_GET_I32(iss, tile_id, i, j);

            for (uint32_t k = 0; k < tk; ++k)
            {
                const uint32_t regA = base_vs2 + k * row_stride;
                const uint32_t regB = base_vs1 + k * row_stride;

                // A: always uint8
                const uint32_t a = (uint32_t)(uint8_t)
                    velem_get_value(iss, regA, i, 1, lmul);

                // B: uint8 (altfmt=0) or int8 (altfmt=1)
                const int32_t b = b_signed
                    ? (int32_t)(int8_t)(uint8_t)
                        velem_get_value(iss, regB, j, 1, lmul)
                    : (int32_t)(uint8_t)
                        velem_get_value(iss, regB, j, 1, lmul);

                acc += a * b;
            }

            TILE_ACC_SET_I32(iss, tile_id, i, j, acc);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// vtmms.tvv — signed/mixed integer matmul
//   A: int8  (vs2, always signed)
//   B: uint8 (vs1, if altfmt=0) or int8 (vs1, if altfmt=1)
//   C: int32 accumulator
//   C[i][j] += sum_k( int8(A[i][k]) * B[j][k] )
// =============================================================
static inline iss_reg_t vtmms_tvv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (VSTART != 0)
        VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    uint32_t       tm      = mtype_tm(mtype);
    uint32_t       tk      = mtype_tk(mtype);
    const uint32_t mtwiden = mtype_mtwiden(mtype);

    if (mtwiden == 0) return iss_insn_next(iss, insn, pc);

    if (tk > 4) tk = 4;

    const uint32_t tn  = get_tn(iss);
    const uint32_t twiden = 1u << (mtwiden - 1);

    if (tm == 0 || tn == 0 || tk == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t sewb = iss->vector.sewb;
    if (sewb != 1)
        VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t lmul       = iss->vector.lmul;
    const uint32_t base_vs1   = (uint32_t)REG_IN(0);   // B
    const uint32_t base_vs2   = (uint32_t)REG_IN(1);   // A
    const uint32_t row_stride = valid_row_stride(sewb, twiden);  // 2
    const uint32_t tile_id    = (uint32_t)REG_OUT(0) & 0xF;

    // altfmt=1: B is also int8; altfmt=0: B is uint8
    const bool b_signed = ALTFMT;

    if ((tile_id & 0x3) != 0)
        VTMM_ILLEGAL(iss, insn, pc);

    for (uint32_t i = 0; i < tm; ++i)
    {
        for (uint32_t j = 0; j < tn; ++j)
        {
            int32_t acc = TILE_ACC_GET_I32(iss, tile_id, i, j);

            for (uint32_t k = 0; k < tk; ++k)
            {
                const uint32_t regA = base_vs2 + k * row_stride;
                const uint32_t regB = base_vs1 + k * row_stride;

                // A is always int8 (signed)
                const int32_t a = (int32_t)(int8_t)(uint8_t)
                    velem_get_value(iss, regA, i, 1, lmul);

                // B is uint8 (altfmt=0) or int8 (altfmt=1)
                const int32_t b = b_signed
                    ? (int32_t)(int8_t)(uint8_t)
                        velem_get_value(iss, regB, j, 1, lmul)
                    : (int32_t)(uint8_t)
                        velem_get_value(iss, regB, j, 1, lmul);

                acc += a * b;
            }

            TILE_ACC_SET_I32(iss, tile_id, i, j, acc);
        }
    }

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// FP Matmuls (OCP, BF16, IEEE)
// =============================================================
// Instruction       SEW  ALTFMT   A format        B format        Accumulator
// vtfmm.tvv          8     0     E4M3 (4,3)      E4M3 (4,3)      FP32 (8,23)
// vtfmm.tvv          8     1     E4M3 (4,3)      E5M2 (5,2)      FP32 (8,23)
// vtfmm.tvv         16     0     FP16 (5,10)     FP16 (5,10)     FP32 (8,23)
// vtfmm.tvv         32     0     FP32 (8,23)     FP32 (8,23)     FP32 (8,23)
// vtfmm.alt.tvv      8     0     E5M2 (5,2)      E4M3 (4,3)      FP32 (8,23)
// vtfmm.alt.tvv      8     1     E5M2 (5,2)      E5M2 (5,2)      FP32 (8,23)
// vtfmm.alt.tvv     16     1     BF16 (8,7)      BF16 (8,7)      FP32 (8,23)

// single-width floating-point matmul
static inline iss_reg_t vtfmm_tvv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (VSTART != 0) VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    uint32_t       tm      = mtype_tm(mtype);
    uint32_t       tk      = mtype_tk(mtype);
    const uint32_t mtwiden = mtype_mtwiden(mtype);

    if (mtwiden == 0) return iss_insn_next(iss, insn, pc);
    if (tk > 4) tk = 4;

    const uint32_t tn     = get_tn(iss);
    const uint32_t twiden = 1u << (mtwiden - 1);

    if (tm == 0 || tn == 0 || tk == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t sewb = iss->vector.sewb;

    // Accumulator is always FP32 (TEW=32)
    const uint8_t acc_exp = 8, acc_mant = 23;

    // A format: determined by instruction (vtfmm.tvv)
    // B format: ALTFMT selects alternate B type for SEW=8
    uint8_t A_exp, A_mant, B_exp, B_mant;

    switch (sewb) {
    case 1:  // SEW=8 → FP8 inputs, TWIDEN must be 4 (TEW=32)
        if (twiden != 4) VTMM_ILLEGAL(iss, insn, pc);
        A_exp = 4; A_mant = 3;  // A: always E4M3 for vtfmm.tvv
        if (ALTFMT) { B_exp = 5; B_mant = 2; }  // B: E5M2
        else        { B_exp = 4; B_mant = 3; }  // B: E4M3
        break;

    case 2:  // SEW=16 → FP16 inputs widened to FP32, TWIDEN must be 2
        if (twiden != 2) VTMM_ILLEGAL(iss, insn, pc);
        A_exp = 5; A_mant = 10;  // FP16
        B_exp = 5; B_mant = 10;  // FP16
        break;

    case 4:  // SEW=32 → FP32 direct, TWIDEN must be 1
        if (twiden != 1) VTMM_ILLEGAL(iss, insn, pc);
        A_exp = 8; A_mant = 23;  // FP32
        B_exp = 8; B_mant = 23;  // FP32
        break;
#ifdef TEW64
    case 5:  // SEW=64 → FP64 direct, TWIDEN must be 1
        if (twiden != 1) VTMM_ILLEGAL(iss, insn, pc);
        A_exp = 11; A_mant = 52;  // FP64
        B_exp = 11; B_mant = 52;  // FP64
        break;
#endif

    default:
        VTMM_ILLEGAL(iss, insn, pc);
    }

    const uint32_t lmul       = iss->vector.lmul;
    const uint32_t base_vs1   = (uint32_t)REG_IN(0);
    const uint32_t base_vs2   = (uint32_t)REG_IN(1);
    const uint32_t tile_id    = (uint32_t)REG_OUT(0) & 0xF;
    const uint32_t row_stride = valid_row_stride(sewb, twiden);

    if ((tile_id & 0x3) != 0) VTMM_ILLEGAL(iss, insn, pc);

    // SEW=32 or SEW=64: no input widening needed
    const bool fp_conversion = !((sewb == 4) || (sewb == 8));

    for (uint32_t i = 0; i < tm; ++i)
    {
        for (uint32_t j = 0; j < tn; ++j)
        {
            uint64_t acc = TILE_ACC_GET_F(iss, tile_id, i, j);

            for (uint32_t k = 0; k < tk; ++k)
            {
                const uint32_t regA = base_vs2 + k * row_stride;
                const uint32_t regB = base_vs1 + k * row_stride;

                uint64_t a = velem_get_value(iss, regA, i, sewb, lmul);
                uint64_t b = velem_get_value(iss, regB, j, sewb, lmul);

                if (fp_conversion) {
                    // Widen inputs to FP32 before MAC
                    a = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round,
                                     a, A_exp, A_mant, acc_exp, acc_mant, 7);
                    b = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round,
                                     b, B_exp, B_mant, acc_exp, acc_mant, 7);
                }

                // FP32 fused multiply-add
                acc = LIB_FF_CALL4(lib_flexfloat_madd_round,
                                   a, b, acc, acc_exp, acc_mant, 7);
            }

            TILE_ACC_SET_F(iss, tile_id, i, j, acc);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

// widening floating-point matmul
static inline iss_reg_t vtfmm_alt_tvv_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (VSTART != 0) VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    uint32_t       tm      = mtype_tm(mtype);
    uint32_t       tk      = mtype_tk(mtype);
    const uint32_t mtwiden = mtype_mtwiden(mtype);

    if (mtwiden == 0) return iss_insn_next(iss, insn, pc);
    if (tk > 4) tk = 4;

    const uint32_t tn     = get_tn(iss);
    const uint32_t twiden = 1u << (mtwiden - 1);

    if (tm == 0 || tn == 0 || tk == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t sewb = iss->vector.sewb;

    // Accumulator always FP32
    const uint8_t acc_exp = 8, acc_mant = 23;

    // A format: E5M2 for vtfmm.alt.tvv (SEW=8) or BF16 (SEW=16)
    // B format: ALTFMT selects for SEW=8
    uint8_t A_exp, A_mant, B_exp, B_mant;

    switch (sewb) {
        case 1:  // SEW=8 → FP8 inputs, TWIDEN=4
            if (twiden != 4) VTMM_ILLEGAL(iss, insn, pc);
            A_exp = 5; A_mant = 2;  // A: always E5M2 for vtfmm.alt.tvv
            if (ALTFMT) { B_exp = 5; B_mant = 2; }  // B: E5M2
            else        { B_exp = 4; B_mant = 3; }  // B: E4M3
            break;

        case 2:  // SEW=16 → BF16 inputs (Zvtbf16fmm), TWIDEN=2
            if (twiden != 2 or ALTFMT != 1) VTMM_ILLEGAL(iss, insn, pc);
            A_exp = 8; A_mant = 7;  // BF16 (same exp as fp32, truncated mant)
            B_exp = 8; B_mant = 7;  // BF16
            break;

        default:  // SEW=32/64 not defined for vtfmm.alt.tvv
            VTMM_ILLEGAL(iss, insn, pc);
    }

    const uint32_t lmul       = iss->vector.lmul;
    const uint32_t base_vs1   = (uint32_t)REG_IN(0);
    const uint32_t base_vs2   = (uint32_t)REG_IN(1);
    const uint32_t tile_id    = (uint32_t)REG_OUT(0) & 0xF;
    const uint32_t row_stride = valid_row_stride(sewb, twiden);

    if ((tile_id & 0x3) != 0) VTMM_ILLEGAL(iss, insn, pc);

    for (uint32_t i = 0; i < tm; ++i)
    {
        for (uint32_t j = 0; j < tn; ++j)
        {
            uint64_t acc = TILE_ACC_GET_F(iss, tile_id, i, j);

            for (uint32_t k = 0; k < tk; ++k)
            {
                const uint32_t regA = base_vs2 + k * row_stride;
                const uint32_t regB = base_vs1 + k * row_stride;

                uint64_t a = velem_get_value(iss, regA, i, sewb, lmul);
                uint64_t b = velem_get_value(iss, regB, j, sewb, lmul);

                // Always widen to FP32 (vtfmm.alt.tvv never has FP32 inputs)
                a = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round,
                                 a, A_exp, A_mant, acc_exp, acc_mant, 7);
                b = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round,
                                 b, B_exp, B_mant, acc_exp, acc_mant, 7);

                acc = LIB_FF_CALL4(lib_flexfloat_madd_round,
                                   a, b, acc, acc_exp, acc_mant, 7);
            }

            TILE_ACC_SET_F(iss, tile_id, i, j, acc);
        }
    }
    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// vtzero — zero the tm x tn submatrix of the destination tile
// rd[4:1] = tile specifier (input operand)
// =============================================================

static inline iss_reg_t vtzero_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (VSTART != 0)
        VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    const uint32_t tm      = mtype_tm(mtype);
    const uint32_t mtwiden = mtype_mtwiden(mtype);

    if (mtwiden == 0) return iss_insn_next(iss, insn, pc);

    const uint32_t tn  = get_tn(iss);

    if (tm == 0 || tn == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t tile_id = (uint32_t)REG_OUT(0) & 0xF;

    if ((tile_id & 0x3) != 0) VTMM_ILLEGAL(iss, insn, pc);

    for (uint32_t i = 0; i < tm; ++i)
        for (uint32_t j = 0; j < tn; ++j)
            TILE_ACC_SET_I32(iss, tile_id, i, j, 0);

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// vtmv.v.t — move tile row/col → vector register
//
//   vd  = REG_OUT(0)          destination vector register
//   rs1 = REG_GET(0)          TSS (scalar GPR, NOT a vreg value)
// =============================================================
static inline iss_reg_t vtmv_v_t_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (VSTART != 0)
        VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    const uint32_t mtwiden = mtype_mtwiden(mtype);

    if (mtwiden == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t tn  = get_tn(iss);

    if (tn == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t tss  = (uint32_t)REG_GET(0);
    uint32_t tile_id, pattern, index;
    tss_decode(tss, &tile_id, &pattern, &index);

    const uint32_t vd   = (uint32_t)REG_OUT(0);
    const uint32_t sewb = iss->vector.sewb;

    if (sewb != TEWB) VTMM_ILLEGAL(iss, insn, pc);

    if (pattern == 0)
    {
        // Row pattern: tile[tile_id][index][j] → vd[j]  for j in [0, tn)
        for (uint32_t j = 0; j < tn; ++j)
        {
            const int32_t val = TILE_ACC_GET_I32(iss, tile_id, index, j);
            velem_set_value(iss, vd, j, sewb, (uint64_t)(uint32_t)val);
        }
    }
    else if (pattern == 1)
    {
        // Column pattern: tile[tile_id][i][index] → vd[i]  for i in [0, tm)
        const uint32_t tm = mtype_tm(mtype);
        for (uint32_t i = 0; i < tm; ++i)
        {
            const int32_t val = TILE_ACC_GET_I32(iss, tile_id, i, index);
            velem_set_value(iss, vd, i, sewb, (uint64_t)(uint32_t)val);
        }
    }
    else
    {
        // pattern > 1: reserved by spec
        VTMM_ILLEGAL(iss, insn, pc);
    }

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// vtmv.t.v — move vector register → tile row/col
//   rs1 = REG_GET(0)          TSS (scalar GPR, destination tile subset)
//   vs2 = REG_IN(1)           source vector register group
// =============================================================
static inline iss_reg_t vtmv_t_v_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    if (VSTART != 0)
        VTMM_ILLEGAL(iss, insn, pc);

    const uint32_t mtype   = CSR_MTYPE_VALUE(iss);
    const uint32_t mtwiden = mtype_mtwiden(mtype);

    if (mtwiden == 0)
        return iss_insn_next(iss, insn, pc);

    const uint32_t tn  = get_tn(iss);

    if (tn == 0)
        return iss_insn_next(iss, insn, pc);

    // TSS comes from scalar GPR rs1 — use REG_GET(0)
    const uint32_t tss  = (uint32_t)REG_GET(0);
    uint32_t tile_id, pattern, index;
    tss_decode(tss, &tile_id, &pattern, &index);

    // vs2 is the source vector register group
    const uint32_t vs2  = (uint32_t)REG_IN(1);
    const uint32_t sewb = iss->vector.sewb;
    const uint32_t lmul = iss->vector.lmul;

    // TEW=32: SEW must match for lossless transfer
    if (sewb != TEWB)
        VTMM_ILLEGAL(iss, insn, pc);

    if (pattern == 0)
    {
        // Row pattern: vs2[j] → tile[tile_id][index][j]  for j in [0, tn)
        for (uint32_t j = 0; j < tn; ++j)
        {
            const uint32_t val =
                (uint32_t)velem_get_value(iss, vs2, j, sewb, lmul);
            TILE_ACC_SET_I32(iss, tile_id, index, j, (int32_t)val);
        }
    }
    else if (pattern == 1)
    {
        // Column pattern: vs2[i] → tile[tile_id][i][index]  for i in [0, tm)
        const uint32_t tm = mtype_tm(mtype);
        for (uint32_t i = 0; i < tm; ++i)
        {
            const uint32_t val =
                (uint32_t)velem_get_value(iss, vs2, i, sewb, lmul);
            TILE_ACC_SET_I32(iss, tile_id, i, index, (int32_t)val);
        }
    }
    else
    {
        // pattern > 1: reserved by spec
        VTMM_ILLEGAL(iss, insn, pc);
    }

    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// vtle*/vtse* exec stubs
// The VLSU block_handler (handle_insn_tile_load / handle_insn_tile_store
// in spatz_vlsu.cpp) owns the actual work. The exec function only advances
// the PC; the VLSU FSM does the memory transfers and tile.mregs updates.
// =============================================================

static inline iss_reg_t vtle8_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // Handled by AraVlsu::handle_insn_tile_load (tag: vtload)
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vtle16_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // Handled by AraVlsu::handle_insn_tile_load (tag: vtload)
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vtle32_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // Handled by AraVlsu::handle_insn_tile_load (tag: vtload)
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vtle64_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // TEW=32 accumulator cannot hold 64-bit values — illegal operation.
    // The VLSU handler leaves tile_staging zeroed and commits nothing.
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vtse8_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // Handled by AraVlsu::handle_insn_tile_store (tag: vtstore)
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vtse16_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // Handled by AraVlsu::handle_insn_tile_store (tag: vtstore)
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vtse32_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // Handled by AraVlsu::handle_insn_tile_store (tag: vtstore)
    return iss_insn_next(iss, insn, pc);
}

static inline iss_reg_t vtse64_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    // TEW=32 cannot store 64-bit values — illegal operation.
    return iss_insn_next(iss, insn, pc);
}

// =============================================================
// vtdiscard — invalidate tile state (context discard)
// No tile state written; sets mstatus.MS to Initial
// =============================================================

static inline iss_reg_t vtdiscard_exec(Iss *iss, iss_insn_t *insn, iss_reg_t pc)
{
    return iss_insn_next(iss, insn, pc);
}