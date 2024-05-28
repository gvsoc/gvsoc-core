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
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
//include "anc_arithmetics.h"
#include "sigma_delta.h"
#include "sigma_delta_int.h"

void sigma_delta_first_order_modulator(int32_t input,int32_t *output, int32_t *delay_line);
void sigma_delta_interpolator(void);
void cascade_of_resonator_feedforward(int32_t pcm_input, int32_t *output, int32_t *forward_coefficient, int32_t *feedback_coefficient, int32_t *loopback_gain, int8_t loopback_gain_shift, int8_t pdm_scaling_left_shift ,int64_t *delay_line);
int64_t delayed_integrator(int64_t *input, int64_t *output);
int32_t integrator_32(int32_t *input, int32_t *output);
int64_t integrator_64(int64_t *input, int64_t *output);
int32_t one_bit_quantizer_32(int32_t input);
int64_t one_bit_quantizer_64(int64_t input);
int32_t randn(void);
int64_t comb_filter(int64_t *input, int64_t *output, int depth);
int32_t lattice_ladder_fix(int32_t input, int32_t *output, int32_t *parcor_k, int parcor_shift, int32_t *ladder_v, int ladder_shift, int64_t *feedback_delay_line_g, int precision_nb_bit, int stage_nb);
int get_sign_edge_32( int64_t value);

void sigma_delta_interpolator(void)
    {
        //TODO: code of upsampler here
    }

void sigma_delta_modulator(int32_t input,int32_t *output, int64_t *delay_line)
    {
    int32_t loopback_gain[2] = {4248414, 1500259};
    int8_t  loopback_gain_shift_right = 8;
    int32_t forward_coefficient[CRFB_ORDER+1] = {5540, 70669, 458739, 2098672, 4665325, 8388608};
    int32_t *feedback_coefficient = forward_coefficient;
    int8_t pdm_scaling_left_shift = PRECISION+SHIFT_12dB-1; // to allow full-scale - 12dB
    cascade_of_resonator_feedforward(input, output, forward_coefficient, feedback_coefficient, loopback_gain,loopback_gain_shift_right, pdm_scaling_left_shift , delay_line);

    }

void sigma_delta_first_order_modulator(int32_t input,int32_t *output, int32_t *delay_line)
    {
    int32_t residual=0;
    int32_t *integrator_output= &delay_line[0], *scaled_pdm_output = &delay_line[1];

    residual = input - scaled_pdm_output[0];
    integrator_32(&residual, &integrator_output[0]);
    scaled_pdm_output[0] = one_bit_quantizer_32(integrator_output[0]);
    output[0]= scaled_pdm_output[0] > 0 ? 1 : -1;
    }


int sigma_delta_demodulator(int input_bit,int32_t *output, int64_t *delay_line, int decimation, int order, int depth, int filter_in_shift, bool filter_enable, int32_t *cic_lattice_ladder_parcor_k, int cic_parkor_shift, int32_t *cic_lattice_ladder_v, int cic_ladder_shift, int cic_lattice_ladder_nb_stages, int *subsampling_state)
    {
    int64_t input = (int64_t)input_bit;
    int64_t next_stage_input = 0;                                   // Required for debug else output will stay in delay_line[order]
    int64_t *delay_line_interpolator = &delay_line[0];              // NOTE:form 0 to 2*ORDER
    int64_t *delay_line_comb_filter  = &delay_line[order+1];        // NOTE: from 2*ORDER+1 to (2*ORDER+1)+((depth+1)*ORDER)-1)
    int32_t filter_input=0;
    int     comb_filter_atomic_depth = depth+1;                 
    int64_t *lattice_ladder_delay_line = &delay_line_comb_filter[((order+1)*(comb_filter_atomic_depth))];
    int     output_data_available = FALSE;
    int     integrator_index = 0;


    delay_line_interpolator[0] = input;

    //SIGMA : Integrator loop 
    for(int i=0; i<order; i++)
        {
        next_stage_input = integrator_64(&delay_line_interpolator[i], &delay_line_interpolator[i+1]);
        }
    //decimated loop: comp filters
    if(!(*subsampling_state)--)
        {
        //roll low sampling frequency part of the delay lines
        for(int i=1;i<=(((order+1)*comb_filter_atomic_depth));i++) 
            {
            delay_line_comb_filter[i-1] = delay_line_comb_filter[i];
            }
        delay_line_comb_filter[depth]=next_stage_input;             // NOTE: = delay_line_interpolator[2*ORDER];
        // DELTA: Comb filters
        for(integrator_index=depth; integrator_index<(order * comb_filter_atomic_depth); integrator_index+=(comb_filter_atomic_depth))  // comb_filter_atomic_depth = delay-line size for each stage
            {
            comb_filter(&delay_line_comb_filter[integrator_index], &delay_line_comb_filter[integrator_index+comb_filter_atomic_depth], depth);
            }

        filter_input = (int32_t)(delay_line_comb_filter[((order+1)*(comb_filter_atomic_depth))-1] >> filter_in_shift);
        if(filter_enable)
            {
            lattice_ladder_fix(filter_input, output, cic_lattice_ladder_parcor_k, cic_parkor_shift, cic_lattice_ladder_v, cic_ladder_shift, lattice_ladder_delay_line, CIC_PRECISION_IN_BIT, cic_lattice_ladder_nb_stages);
            }
        else
            {
            output[0] = filter_input;
            }
        *subsampling_state = decimation-1; // NOTE: from M-1 to 0 = M cycles
        output_data_available = TRUE;
        }

    return(output_data_available);
    }

void cascade_of_resonator_feedforward(int32_t pcm_input, int32_t *output, int32_t *forward_coefficient, int32_t *feedback_coefficient, int32_t *loopback_gain, int8_t loopback_gain_shift, int8_t pdm_scaling_left_shift ,int64_t *delay_line)
    {
    int64_t *stage_output   = &delay_line[0];                     // redirection for readability
    int64_t *pdm_output     = &delay_line[CRFB_ORDER+1];

  // stage n computation (delayed integrators)
    stage_output[1] =   stage_output[1] + (pcm_input *  (int64_t)forward_coefficient[1]) - (*pdm_output * (int64_t)feedback_coefficient[1]) - ((loopback_gain[0] * (stage_output[2]>>PRECISION))>>loopback_gain_shift) + (stage_output[0]);
    stage_output[3] =   stage_output[3] + (pcm_input *  (int64_t)forward_coefficient[3]) - (*pdm_output * (int64_t)feedback_coefficient[3]) - ((loopback_gain[1] * (stage_output[4]>>PRECISION))>>loopback_gain_shift) + (stage_output[2]);

    // compute non-quantized output
    stage_output[5] =  (pcm_input *  (int64_t)forward_coefficient[5]) + stage_output[4];
    *pdm_output = (stage_output[5] >= 0) ? (((int64_t)1 ) << pdm_scaling_left_shift) : (((int64_t)-1) << pdm_scaling_left_shift);

    // stage n-1 computation [i.e. for next iteration] (integrators) uses the newly computed output
    stage_output[0] = stage_output[0] + (pcm_input *  (int64_t)forward_coefficient[0]) - (*pdm_output * (int64_t)feedback_coefficient[0]);
    stage_output[2] = stage_output[2] + (pcm_input *  (int64_t)forward_coefficient[2]) - (*pdm_output * (int64_t)feedback_coefficient[2]) + stage_output[1];
    stage_output[4] = stage_output[4] + (pcm_input *  (int64_t)forward_coefficient[4]) - (*pdm_output * (int64_t)feedback_coefficient[4]) + stage_output[3];

    output[0] = (*pdm_output >= 0) ? 1 : -1;
    }


int64_t delayed_integrator(int64_t *input, int64_t *output)
    {
    output[0] = output[0]+input[-1];

    return(output[0]);
    }

int32_t integrator_32(int32_t *input, int32_t *output)
    {
    output[0] = output[0]+input[0];

    return(output[0]);
    }

int64_t integrator_64(int64_t *input, int64_t *output)
    {
    output[0] = output[0]+input[0];

    return(output[0]);
    }

int64_t comb_filter(int64_t *input, int64_t *output, int depth)
    {
    output[0] = input[0]-input[-depth];    // 1- Z^-M

    return(output[0]);
    }

int32_t one_bit_quantizer_32(int32_t input)
    {
    int32_t dither_noise, MSB;

    dither_noise= randn();
    MSB = ((input + dither_noise) < 0) ? (int32_t)(MINUS_ONE_24_BITS) : (int32_t)ONE_24_BITS;

    return(MSB);  
    }

int64_t one_bit_quantizer_64(int64_t input)
    {
    int64_t dither_noise, MSB;

    dither_noise= randn();
    MSB = ((input + dither_noise) < 0) ? (int64_t)(MINUS_ONE_24_BITS) : (int64_t)ONE_24_BITS;

    return(MSB);  
    }

int32_t randn(void)
    {
    return((int32_t)0); //MODLS: No dithering for the moment
    }

int get_sign_edge_32( int64_t value)
    {
    int shift = 31-(value>0)-__builtin_clrsb((int32_t)(value>>32));

    shift = shift < 0 ? 0 : shift;

    return (shift);
    }

int32_t lattice_ladder_fix(int32_t input, int32_t *output, int32_t *parcor_k, int parcor_shift, int32_t *ladder_v, int ladder_shift, int64_t *feedback_delay_line_g, int precision_nb_bit, int stage_nb)
    {
    int64_t forward_f=0,forward_f_prev=0, fdbck_g=0;
    int64_t accumulator_y=0;
    int n=stage_nb-1;
    int forward_shift=0;
    int feedback_shift=0;

    // Preamble
    // ********
    feedback_shift = get_sign_edge_32(feedback_delay_line_g[n]);
    forward_f = (int64_t)(input) - (((parcor_k[n]) * (feedback_delay_line_g[n]>>feedback_shift))>>(precision_nb_bit-feedback_shift-1+parcor_shift));
    forward_shift = get_sign_edge_32(forward_f);
    fdbck_g = (((forward_f >> forward_shift) * parcor_k[n])>>(precision_nb_bit-forward_shift-1+parcor_shift)) + feedback_delay_line_g[n];
    accumulator_y += (fdbck_g * (ladder_v[n+1]))>> (ladder_shift);
    forward_f_prev = forward_f;
    // Main loop
    // *********
    for(n=(stage_nb-2);n>=0;n--)
        {
         /* Process samples */
        feedback_shift = get_sign_edge_32(feedback_delay_line_g[n]);
        forward_f = forward_f_prev - (((parcor_k[n]) * (feedback_delay_line_g[n]>>feedback_shift))>>(precision_nb_bit-feedback_shift-1+parcor_shift));
        forward_shift = get_sign_edge_32(forward_f);
        fdbck_g = (((forward_f >> forward_shift) * parcor_k[n])>>(precision_nb_bit-forward_shift-1+parcor_shift)) + feedback_delay_line_g[n];
        /* Output samples */
        accumulator_y += (fdbck_g * (ladder_v[n+1]))>> (ladder_shift);
        forward_f_prev = forward_f;
        feedback_delay_line_g[n+1] = fdbck_g;
        }

    // Postamble
    // *********
    feedback_delay_line_g[0]= forward_f;
    accumulator_y += ((feedback_delay_line_g[0]) * (ladder_v[0])) >> (ladder_shift);

    *output = (int32_t)(accumulator_y>>(precision_nb_bit-1));
    return(*output);
    }