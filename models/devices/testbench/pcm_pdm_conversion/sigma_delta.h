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

#ifndef _sigma_delta_H_
#define _sigma_delta_H_

// Sigma delta parameters
#define MAX_CIC_ORDER_N                 	8
#define MAX_CIC_DECIMATION_R            	64
#define MAX_CIC_DEPTH_M                		2
#define MAX_LATTICE_LADDER_FILTER_NB_CELL   11
#define LATTICE_LADDER_FILTER_NB_CELL   	9

#define CIC_PRECISION_IN_BIT        		24
#define CRFB_ORDER							5

#define TRUE                        		1
#define FALSE                       		0

extern int cic_order_n[2];
extern int cic_decimation_r[3];
extern int cic_depth_m[2];

extern void sigma_delta_modulator(int32_t input, int32_t *output, int64_t *delay_line);
extern int sigma_delta_demodulator(int input_bit,int32_t *output, int64_t *delay_line, int decimation, int order, int depth, int cic_in_shift, bool filter_enable, int32_t *cic_lattice_ladder_parcor_k, int cic_parkor_shift, int32_t *cic_lattice_ladder_v, int cic_ladder_shift, int cic_lattice_ladder_nb_stages);

#endif