#ifndef __PCM_PDM_CONVERSION_HPP__
#define __PCM_PDM_CONVERSION_HPP__

#include <string>
#include <stdint.h>
#include <unistd.h>

#include "linear_interpolator.h"
#include "iir_interpolator.h"
#include "sigma_delta.h"

// #define DEBUG_CONVERSION

#define NO_ERROR 0
#define INTERNAL_ERROR -1

#define DEFAULT_RATIO_SHIFT 3 // Interpolation ratio of 8 by default

#define NB_STAGES 4
#define NB_COEF_MA 3
#define NB_COEF_AR 2
#define SAMPLE_PRECISION 24
#define COEF_FRACT_PRECISION 31
#define UPSAMPLING_RATIO 8


typedef enum interpolator_type
{
    LINEAR = 0,
    IIR
} interpolator_type;



typedef struct
{
    uint8_t interpolation_ratio_shift;
    uint8_t interpolation_type;
} pcm2pdm_config;





// Class for the PCM to PDM conversion
class PcmToPdm
{
private:
    int32_t nb_generated_samples;
    int32_t nb_samples_in;
    int32_t local_ratio_shift;
    int32_t output_index = 0;
    interpolator_type interpolation_type;
    iir_interpolator_context iir_context;
    linear_interpolator_context linear_context;

    int32_t *interpolated_samples = NULL;

    bool saturation_status = false;
    int64_t my_delay_line[2 * (CRFB_ORDER + 1)] = {0};

#ifdef DEBUG_CONVERSION
    FILE *file_debug_pcm_got, *file_debug_pdm_produced;  // files for debugging
#endif

public:
    int32_t output_size = 0;
    int32_t *output = NULL;
    int8_t *pdm_output = NULL;
#ifdef DEBUG_CONVERSION
    void debug_pcm_got(int32_t pcm);  // store pcm data that must be converted into a file int32_t
    void debug_pdm_produced(int32_t pdm);  // store pdm data that has been converted into a file (in int8_t)
#endif

    PcmToPdm(uint8_t interpolation_ratio_shift, uint8_t interpolation_type);
    void convert(int32_t pcm_data);
    ~PcmToPdm()
    {
        delete[] iir_context.filter_state;
        delete[] interpolated_samples;
        delete[] output;
        delete[] pdm_output;
#ifdef DEBUG_CONVERSION
        fclose(file_debug_pcm_got);
        fclose(file_debug_pdm_produced);
#endif
    }
};




class PdmToPcm
{

private:
    int64_t my_delay_line[(MAX_LATTICE_LADDER_FILTER_NB_CELL) + ((MAX_CIC_DEPTH_M + 1) * (MAX_CIC_ORDER_N + 1)) + ((MAX_CIC_ORDER_N) + 1)] = {0};
    int32_t cic_ladder_v[10] = {164, 621, 16364, 47766, 409190, 863368, 3611474, 4401319, 7616938, 3533785};
    int32_t cic_parcor_k[9] = {-8304265, 8375337, -8351207, 8357304, -8354257, 8350102, -8337066, 8248286, -6391596};
    int32_t cic_decimation = 4;
    int32_t cic_order = 8;
    int32_t cic_depth = 2;
    int32_t cic_output_shift = 1;
    int32_t filter_coef;
    bool cic_filter_enable = false;
    int32_t cic_parkor_shift = 0;
    int32_t cic_ladder_shift = 14;
    int32_t cic_lattice_ladder_nb_stages = 9;

#ifdef DEBUG_CONVERSION
    FILE *file_debug_pdm_got, *file_debug_pcm_produced;
#endif

public:
    int32_t pcm_output;
    PdmToPcm(int32_t cic_r, int32_t cic_n, int32_t cic_m, int32_t cic_shift);
    int convert(int32_t input_bit);
    void debug_pdm_got(int32_t pdm);
    void debug_pcm_produced(int32_t pcm);
    ~PdmToPcm()
    {
#ifdef DEBUG_CONVERSION
        fclose(file_debug_pdm_got);
        fclose(file_debug_pcm_produced);
#endif
    }
};

#endif