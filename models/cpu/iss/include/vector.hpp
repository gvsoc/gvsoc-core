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
//#define NB_VEL VLEN/SEW
//#define NB_VEL 256/8
#define NB_VEL 2048/8//???????????????????????

//#define VLMAX NB_VEL*iss->spatz.LMUL

#define XLEN = ISS_REG_WIDTH
#define FLEN = ISS_REG_WIDTH
#define ELEN = MAX(XLEN,FLEN)

// const float LMUL_VALUES[] = {1.0f, 2.0f, 4.0f, 8.0f, 0, 0.125f, 0.25f, 0.5f};

// const int SEW_VALUES[] = {8,16,32,64,128,256,512,1024};



// int   VLEN = 256;
// int   SEW  = SEW_VALUES[2];
// float LMUL = LMUL_VALUES[0];
// bool  VMA  = 0;
// bool  VTA  = 0;
// int vstart = 0;
// iss_reg_t vl;

//iss_uim_t vl;// the vl in vector CSRs

// define a new class named SPATZ like ISS in class.hpp


class Vector
{
public:
    Vector(Iss &iss);
	
    void build();
    void reset(bool reset);
	
    //const float LMUL_VALUES[8] = {1.0f, 2.0f, 4.0f, 8.0f, 0, 0.125f, 0.25f, 0.5f};
	
    //                          V 1.0
    const float LMUL_VALUES[8] = {1.0f, 2.0f, 4.0f, 8.0f, 1.0f, 0.125f, 0.25f, 0.5f};

    //                          V 0.8
    // const float LMUL_VALUES[4] = {1.0f, 2.0f, 4.0f, 8.0f};
    const int SEW_VALUES[8] = {8,16,32,64,128,256,512,1024};

	static constexpr int N_BANK = 4;

	bool Bank_status [N_BANK];

    int   VLEN   = 256;
    int   SEW_t    = SEW_VALUES[2];
    float LMUL_t   = LMUL_VALUES[0];
    bool  VMA    = 0;
    bool  VTA    = 0;
    // int   vstart = 0;
    // uint8_t counter = 1;
    // iss_reg_t vtype;
    // iss_reg_t vl;
    unsigned int sewb = 8;
    uint8_t exp;
    uint8_t mant;

#ifdef CONFIG_GVSOC_ISS_CVA6
    uint8_t vregs[N_BANK][ISS_NB_VREGS][CONFIG_ISS_VLEN/8];
#else
    uint8_t vregs[N_BANK][ISS_NB_VREGS][(int)NB_VEL];
#endif
};
