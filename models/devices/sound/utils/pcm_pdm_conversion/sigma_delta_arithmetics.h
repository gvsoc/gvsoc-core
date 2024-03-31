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


#ifndef SIGMA_DELTA_ARITHMETICS_H
#define SIGMA_DELTA_ARITHMETICS_H

#define NOT_SATURATED 		0
#define SATURATED 			1
#define SINGLE_PRECISION	32   					// Number of bit for single precision
#define MAX_64BIT_VALUE		0x7FFFFFFFFFFFFFFF		// Number of bit for double precision
#define MIN_64BIT_VALUE		0x8000000000000000	 // Number of bit for double precision

/* GLOBAL VARIABLES */
/********************/
extern bool staturation_status;


/* PROTOTYPES */
/**************/
extern int64_t fix_add_sat(int64_t operand_left, int64_t operand_right, int8_t precision_nb_bit, bool with_saturation, bool *saturation_status);
extern int64_t fix_mac_sat(int64_t operand_left, int64_t operand_right, int8_t precision_nb_bit, int64_t accumulator, bool with_saturation, bool *saturation_status);
extern void clear_saturation(void);
#endif /* End of File *//* SIGMA_DELTA_ARITHMETICS_H */
