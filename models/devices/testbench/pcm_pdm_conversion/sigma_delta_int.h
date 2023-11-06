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

#ifndef _sigma_delta_INT_H_
#define _sigma_delta_INT_H_

// Define 1 and -1 in the precision format
#define ONE_24_BITS              8388608  // Q9.23
#define MINUS_ONE_24_BITS       -8388608  // Q9.23
#define PRECISION				 23       // 23 because of Q9.23
#define SHIFT_12dB				 2					
// state of the delta-sigma
int subsampling_state = 0;

#endif