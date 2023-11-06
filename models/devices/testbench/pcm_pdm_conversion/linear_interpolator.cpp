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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "sigma_delta_arithmetics.h"
#include "linear_interpolator.h"

#define SIGNED_SHIFT_LEFT(X,Y) (((Y)<0) ? (X>>(-Y)) : (X<<Y))


int32_t linear_interpolator(int32_t input, int32_t *output, linear_interpolator_context *my_context )
    {
    int32_t delta = SIGNED_SHIFT_LEFT((input - my_context->previous_sample) , - my_context->ratio_shift);

    for(int output_index = 0; output_index < my_context->interpolation_ratio; output_index++)
        {
        my_context->previous_sample += delta;
        output[output_index] = my_context->previous_sample;
        }
    output[(my_context->interpolation_ratio)-1] = input;
    my_context->previous_sample = input;

    return(my_context->interpolation_ratio);
    }

//TODO: Need to add method to reset the filter history
