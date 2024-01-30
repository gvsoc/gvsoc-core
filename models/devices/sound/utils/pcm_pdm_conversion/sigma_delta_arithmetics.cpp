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

#include <stdint.h>
#include <stdbool.h>
#include "sigma_delta_arithmetics.h"

int64_t fix_mult(int32_t operand_left, int32_t operand_right, int8_t precision_nb_bit)
    {
    int64_t result;
    int64_t max_product_value =  ((int64_t)1L<<((precision_nb_bit-1)<<1))-1;
    int64_t minus_one_X_minus_one =  (int64_t)1L<<((precision_nb_bit-1)<<1);

    result = (int64_t) operand_left * (int64_t) operand_right;
    if (result == (minus_one_X_minus_one))        /* case -1 * -1 : 2 x (precision_nb_bit-1) */
        {
        result = (int64_t)max_product_value;        /* -1 * -1 = 1 */
        }

    return (result);
    }

int64_t fix_add_sat(int64_t operand_left, int64_t operand_right, int8_t precision_nb_bit, bool with_saturation, bool *saturation_status)
    {
    int64_t accumulator,edge;
    int64_t max_sum_value =  (int64_t)(1L<<(precision_nb_bit-1))-1;
    int64_t min_sum_value =  (int64_t)(-(1L<<(precision_nb_bit-1)));

    if(__builtin_add_overflow(operand_left,operand_right, &accumulator))
        {
        if(with_saturation)
            {
            // MODLS: case saturation of the total available dynamic
            if (operand_left>0) // NOTE: As we overflowed, the 2 operand have the same sign !
                {
                accumulator = MAX_64BIT_VALUE;
                }
            else
                {
                accumulator = MIN_64BIT_VALUE;
                }
            *saturation_status = SATURATED;
            }
        }
    else if(with_saturation)
        {
        // MODLS: case for saturation @ sample dynamic-range
        edge = (accumulator & min_sum_value) >> (precision_nb_bit-1); // MODLS: case for transmission 
        if ((edge) && (edge != -1))
            {
            if (edge>0)
                {
                accumulator = max_sum_value;
                }
            else
                {
                accumulator = min_sum_value;
                }
            *saturation_status = SATURATED;
            }
        }
    return (accumulator);
    }

int64_t fix_mac_sat(int64_t operand_left, int64_t operand_right, int8_t precision_nb_bit, int64_t accumulator, bool with_saturation, bool *saturation_status)
    {
    int64_t product_result, accumulator_64;

    product_result = fix_mult(operand_left, operand_right, precision_nb_bit);
    accumulator_64 = fix_add_sat(accumulator, product_result, (precision_nb_bit<<1)-1,with_saturation, saturation_status); // precision_nb_bit x 2 -1<-as mult is not frsigma_deltational but integer  

    return (accumulator_64);
    }
