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
#include "iir_interpolator.h"

#define DO_NOT_SATURATE 0
#define BIQUAD_ORDER 2       // biquad order per definition is order 2
#define BIQUAD_STATE_DEPTH 4 // biquad state per definition is order 4 in direct form I

// Status bit-field mask
#define MAX_PRECISION 64

#define SIGNED_SHIFT_LEFT(X, Y) (((Y) < 0) ? (X >> (-Y)) : (X << Y))


int32_t biquad_directI_fix(int32_t input, int8_t input_shift, int32_t *output, int8_t output_shift, int32_t *MA_coefficient, int8_t MA_shift, int32_t *AR_coefficient, int8_t AR_shift, int32_t *delay_line, int8_t precision_nb_bit, bool allow_saturation, bool *saturation_status);


int32_t IIR_interpolator(int32_t input, int32_t *output, iir_interpolator_context *my_context)
{
    int32_t iir_out = input;
    int upsampling_index = 0, cell_index = 0, index_ma = 0, index_ar = 0, index_state = 0;
    bool last_stage = 0;

    // forced-mode filtering
    for (cell_index = 0, index_ma = 0, index_ar = 0, index_state = 0; cell_index < my_context->number_biquad; cell_index++, index_ma += BIQUAD_NB_COEF_MA, index_ar += BIQUAD_NB_COEF_AR, index_state += BIQUAD_STATE_DEPTH)
    {
        last_stage = ((cell_index + 1) == my_context->number_biquad);
        biquad_directI_fix(iir_out, my_context->scaling_shifts_in[cell_index], &iir_out, my_context->scaling_shifts_out[cell_index], &my_context->coefficients_MA[index_ma], my_context->scaling_shifts_MA[cell_index], &my_context->coefficients_AR[index_ar], my_context->scaling_shifts_AR[cell_index], &my_context->filter_state[index_state], my_context->precision_nb_bits, (my_context->allow_saturation & last_stage), my_context->status);
    }
    output[upsampling_index] = SIGNED_SHIFT_LEFT(iir_out, my_context->scaling_shifts_end[0]);
    // free-mode filtering

    for (upsampling_index = 1; upsampling_index < my_context->interpolation_ratio; upsampling_index++)
    {

        iir_out = 0;
        for (cell_index = 0, index_ma = 0, index_ar = 0, index_state = 0; cell_index < my_context->number_biquad; cell_index++, index_ma += BIQUAD_NB_COEF_MA, index_ar += BIQUAD_NB_COEF_AR, index_state += BIQUAD_STATE_DEPTH)
        {
            last_stage = ((cell_index + 1) == my_context->number_biquad);
            biquad_directI_fix(iir_out, my_context->scaling_shifts_in[cell_index], &iir_out, my_context->scaling_shifts_out[cell_index], &my_context->coefficients_MA[index_ma], my_context->scaling_shifts_MA[cell_index], &my_context->coefficients_AR[index_ar], my_context->scaling_shifts_AR[cell_index], &my_context->filter_state[index_state], my_context->precision_nb_bits, (my_context->allow_saturation & last_stage), my_context->status);
        }
        output[upsampling_index] = SIGNED_SHIFT_LEFT(iir_out, my_context->scaling_shifts_end[0]);
    }

    return (my_context->interpolation_ratio);
}

/****************************************************************************
 * Implementation of IIR direct form I fixed point.
 *
 * - MA or B stage (input):
 *   bint = b0*x[n] + b1*db[n-1] + ... + bN*db[n-N]
 * - AR or A stage (output):
 *   y[n] = bint - a1*da[n-1] - ... - aN*da[n-N]
 *
 * db[n] = db[n-1]
 * - delay line A: split delay_line in  2 parts: 2 first indexes are MA last AR state
 *   - da[1] = B - a1*da[1] - a2*da[2] - .... - aN*da[N]
 *   - da[n] = da[n-1]
 * - delay line B: db, even indexes of delay_line[]
 *   - db[1] = IN
 *   - db[n] = db[n-1]
 *
 * a: denominator coefficients (AR)
 * b: numerator coefficients (MA)
 *
 * pi: input precision
 * po: output precision
 * pa: a coeff precision
 * pb: b coeff precision
 *
 * shift[0]: input scaling shift
 * shift[1]: MA convolution scaling shift
 * shift[2]: AR convolution scaling shift
 * shift[3]: output scaling shift for delay-line representation
 *
 *
 * TODO: Support custom precision config.
 *       Currently supporting only pi = pb = po = pa = (precision_nb_bit-1)
 ****************************************************************************/

int32_t biquad_directI_fix(int32_t input, int8_t input_shift, int32_t *output, int8_t output_shift, int32_t *MA_coefficient, int8_t MA_shift, int32_t *AR_coefficient, int8_t AR_shift, int32_t *delay_line, int8_t precision_nb_bit, bool allow_saturation, bool *saturation_status)
{
    int64_t accumulator = 0, accumulator_MA = 0, accumulator_AR = 0;
    int32_t *delay_line_MA = delay_line;
    int32_t *delay_line_AR = &delay_line[BIQUAD_ORDER];
    int32_t shifted_input = SIGNED_SHIFT_LEFT(input, input_shift);
    uint8_t index;

    // MA stage
    accumulator_MA = fix_mac_sat(shifted_input, MA_coefficient[0], MAX_PRECISION, accumulator_MA, DO_NOT_SATURATE, saturation_status);
    for (index = 0; index < BIQUAD_ORDER; index++)
    {
        accumulator_MA = fix_mac_sat(delay_line_MA[index], MA_coefficient[index + 1], MAX_PRECISION, accumulator_MA, DO_NOT_SATURATE, saturation_status);
        accumulator_AR = fix_mac_sat(delay_line_AR[index], -AR_coefficient[index], MAX_PRECISION, accumulator_AR, DO_NOT_SATURATE, saturation_status);
    }

    accumulator_MA = SIGNED_SHIFT_LEFT(accumulator_MA, MA_shift); // Not in the sum  below for debug purpose.
    accumulator_AR = SIGNED_SHIFT_LEFT(accumulator_AR, AR_shift);

    //*output = (int32_t)(fix_add_sat(accumulator_AR, accumulator_MA, precision_nb_bit, allow_saturation, saturation_status));
    accumulator = fix_add_sat(accumulator_AR, accumulator_MA, precision_nb_bit, allow_saturation, saturation_status);

    // Roll the delay lines
    for (index = BIQUAD_ORDER - 1; index >= 1; index--)
    {
        delay_line_MA[index] = delay_line_MA[index - 1];
        delay_line_AR[index] = delay_line_AR[index - 1];
    }
    delay_line_MA[0] = shifted_input;
    delay_line_AR[0] = SIGNED_SHIFT_LEFT(accumulator, output_shift);

    output[0] = accumulator;

    return 0;
}

// TODO: Need to add method to reset the filter history
