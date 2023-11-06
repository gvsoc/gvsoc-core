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


#ifndef _LINEAR_INTERPOLATOR_H_
#define _LINEAR_INTERPOLATOR_H_


typedef struct linear_interpolator_context
    {
    int8_t      interpolation_ratio;           /* integer interpolation ratio for 0 insertion                           */
    int8_t      ratio_shift;                   /* number of biquad stages in cascade                                    */
    int32_t     previous_sample;
    } linear_interpolator_context;


extern int32_t  linear_interpolator(int32_t input, int32_t *output, linear_interpolator_context *my_context );
#endif
