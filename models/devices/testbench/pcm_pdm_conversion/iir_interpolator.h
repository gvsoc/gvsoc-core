/*
 * Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
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
 * Authors: Laurent Saïd, GreenWaves Technologies (laurent.said@greenwaves-technologies.com)
 */

#ifndef _IIR_INTERPOLATOR_H_
#define _IIR_INTERPOLATOR_H_
#define BIQUAD_NB_COEF_MA	3
#define BIQUAD_NB_COEF_AR	2


typedef struct iir_interpolator_context
    {
    int8_t      interpolation_ratio;            /* integer interpolation ratio for 0 insertion                           */
    int8_t      number_biquad;                  /* number of biquad stages in cascade                                    */
    int32_t     *coefficients_MA;               /* table of MA (b0 b1 and b2) coefficients (FIR) for all bi-quad cell    */
    int32_t     *coefficients_AR;               /* table of AR (a0 a1 and a2) coefficients (IIR) for all bi-quad cell    */
    int8_t      *scaling_shifts_MA;                /* table of scaling factor, implement bu shifts                          */
    int8_t      *scaling_shifts_AR;                /* table of scaling factor, implement bu shifts                          */
    int8_t      *scaling_shifts_in;                /* table of scaling factor, implement bu shifts                          */
    int8_t      *scaling_shifts_out;                /* table of scaling factor, implement bu shifts                          */
    int8_t      *scaling_shifts_end;                /* table of scaling factor, implement bu shifts                          */
    int32_t     *filter_state;                  /* table of filter history (w0 w1 and w2) for per bi-quad cell           */
    int8_t      precision_nb_bits;              /* precision in bits for output saturation                               */
    bool      allow_saturation;               /* number of bi-quad in cascade                                          */
    bool     *status;
    } iir_interpolator_context;


extern int32_t  IIR_interpolator(int32_t input, int32_t *output, iir_interpolator_context *my_context );
#endif
