
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <stdbool.h>
#include <ctype.h>
#include <sstream>
#include <vector>
#include <getopt.h>
#include <cstring>
#include <iostream>

#include "linear_interpolator.h"
#include "iir_interpolator.h"
#include "sigma_delta.h"
#include "pcm_pdm_conversion.hpp"


int PdmToPcm::convert(int32_t input_bit)
{
    return sigma_delta_demodulator(input_bit, &pcm_output, this->my_delay_line,
                                   this->cic_decimation, this->cic_order, this->cic_depth, this->cic_output_shift, this->cic_filter_enable, this->cic_parcor_k,
                                   this->cic_parkor_shift, this->cic_ladder_v, this->cic_ladder_shift, this->cic_lattice_ladder_nb_stages, &(this->subsampling_state));
}


void PcmToPdm::convert(int32_t pcm_data)
{
    output_index = 0;
    if (interpolation_type == IIR)
    {
        nb_generated_samples = IIR_interpolator(pcm_data, this->interpolated_samples, &this->iir_context);
    }
    else
    {
        nb_generated_samples = linear_interpolator(pcm_data, this->interpolated_samples, &this->linear_context);
    }
    for (int index_interpolated = 0; index_interpolated < nb_generated_samples; index_interpolated++) // interpolated sample loop
    {
        sigma_delta_modulator(interpolated_samples[index_interpolated], &output[output_index], my_delay_line);
        if (output[output_index] <= 0)
        {
            pdm_output[output_index] = 0;
        }
        else
        {
            pdm_output[output_index] = 1;
        }
        output_index++;
    } // interpolated sample loop
}




PdmToPcm::PdmToPcm(int32_t cic_r, int32_t cic_n, int32_t cic_m, int32_t cic_shift)
{
    this->cic_decimation = cic_r;
    this->cic_order = cic_n;
    this->cic_depth = cic_m;
    this->cic_output_shift = cic_shift;
    this->filter_coef = filter_coef;
    this->subsampling_state = 0;

#ifdef DEBUG_CONVERSION
    file_debug_pdm_got = fopen("file_debug_pdm_got", "wb");
    file_debug_pcm_produced = fopen("file_debug_pcm_produced", "wb");
#endif
}



PcmToPdm::PcmToPdm(uint8_t interpolation_ratio_shift, uint8_t interp_type)
{
    int8_t iir_interpolator_shift_in[NB_STAGES] = {0, 0, 0, 0};
    int8_t iir_interpolator_shift_out[NB_STAGES] = {0, 0, 0, 0};
    int8_t iir_interpolator_shift_end[1] = {3};
    int32_t iir_interpolator_MA[NB_STAGES][NB_COEF_MA] = {{709185228, -1408142929, 709185228},
                                                          {1076329801, -2147002896, 1076329801},
                                                          {668750479, -1265161595, 668750479},
                                                          {852379081, -1698836148, 852379081}};
    int8_t iir_interpolator_shift_left_MA[NB_STAGES] = {-3 - 31, -2 - 31, -5 - 31, -1 - 31};
    int32_t iir_interpolator_AR[NB_STAGES][NB_COEF_AR] = {{-2122974449, 1050082652},
                                                          {-2141102846, 1069256467},
                                                          {-2116554201, 1043129460},
                                                          {-2132062194, 1059818016}};
    int8_t iir_interpolator_shift_left_AR[NB_STAGES] = {1 - 31, 1 - 31, 1 - 31, 1 - 31};

    /* default: out belong to (-1,1) couple*/
    if (interp_type == LINEAR)
    {
        interpolation_type = LINEAR; /* interpolation type ie. linear or interpolation filter*/
    }
    else
    {
        interpolation_type = IIR;
    }
    linear_context.interpolation_ratio = (1 << interpolation_ratio_shift); /* integer interpolation ratio for loop size of linear interpolation     */
    linear_context.ratio_shift = interpolation_ratio_shift;                /* hift to avoid division for step computation                           */
    linear_context.previous_sample = 0;                                            /* table of MA (b0 b1 and b2) coefficients (FIR) for all bi-quad cell    */
    iir_context.interpolation_ratio = (1 << interpolation_ratio_shift);    /* integer interpolation ratio for 0 insertion                           */
    iir_context.number_biquad = NB_STAGES;                                         /* number of biquad stages in cascade                                    */
    iir_context.coefficients_MA = (int32_t *)iir_interpolator_MA;                  /* table of MA (b0 b1 and b2) coefficients (FIR) for all bi-quad cell    */
    iir_context.coefficients_AR = (int32_t *)iir_interpolator_AR;                  /* table of AR (a0 a1 and a2) coefficients (IIR) for all bi-quad cell    */
    iir_context.scaling_shifts_MA = iir_interpolator_shift_left_MA;                /* table of scaling factor, implemented by shifts                        */
    iir_context.scaling_shifts_AR = iir_interpolator_shift_left_AR;                /* table of scaling factor, implemented by shifts                        */
    iir_context.scaling_shifts_in = iir_interpolator_shift_in;                     /* table of scaling factor, implemented by shifts                        */
    iir_context.scaling_shifts_out = iir_interpolator_shift_out;                   /* table of scaling factor, implemented by shifts                        */
    iir_context.scaling_shifts_end = iir_interpolator_shift_end;                   /* table of scaling factor, implemented by shifts                        */
    iir_context.filter_state = new int32_t[2 * NB_COEF_AR * NB_STAGES]();          /* table of filter history (w0 w1 and w2) for per bi-quad cell           */
    iir_context.precision_nb_bits = SAMPLE_PRECISION;                              /* precision in bits for output saturation                               */
    iir_context.allow_saturation = false;                                          /* number of bi-quad in cascade                                          */
    iir_context.status = &saturation_status;

    linear_context.interpolation_ratio = (1 << interpolation_ratio_shift); /* integer interpolation ratio for 0 insertion                           */
    linear_context.ratio_shift = interpolation_ratio_shift;
    iir_context.interpolation_ratio = 1 << interpolation_ratio_shift;
    output_size = linear_context.interpolation_ratio;

    interpolated_samples = new int32_t[linear_context.interpolation_ratio]();
    output = new int32_t[output_size]();
    pdm_output = new int8_t[output_size]();

#ifdef DEBUG_CONVERSION
    file_debug_pcm_got = fopen("file_debug_pcm_got", "wb");
    file_debug_pdm_produced = fopen("file_debug_pdm_produced", "wb");
#endif
}

#ifdef DEBUG_CONVERSION

void PcmToPdm::debug_pcm_got(int32_t pcm_data)
{
    fwrite(&pcm_data, sizeof(int32_t), 1, file_debug_pcm_got);
}


void PcmToPdm::debug_pdm_produced(int32_t pdm_data)
{
    int8_t pdm = (int8_t)pdm_data;
    fwrite(&pdm, sizeof(int8_t), 1, file_debug_pdm_produced);
}


void PdmToPcm::debug_pdm_got(int32_t pdm_data)
{
    int8_t pdm = (int8_t)pdm_data;
    fwrite(&pdm, sizeof(int8_t), 1, file_debug_pdm_got);
}


void PdmToPcm::debug_pcm_produced(int32_t pcm_data)
{
    fwrite(&pcm_data, sizeof(int32_t), 1, file_debug_pcm_produced);
}

#endif