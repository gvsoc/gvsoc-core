#include "io_audio.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdexcept>

#include <vp/vp.hpp>
#include <string.h>
#ifdef USE_SNDFILE
#include <sndfile.hh>
#endif
#ifdef USE_SAMPLERATE
#include <samplerate.h>
#endif

/*
Tx_stream_raw_file::Tx_stream_raw_file(Slot *slot, std::string filepath, int width, bool is_bin, pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding)
{
    this->width = width;
    this->is_bin = is_bin;
    this->encoding = encoding;
    this->slot = slot;
    this->outfile = fopen(filepath.c_str(), "w");
    this->slot->trace.msg(vp::Trace::LEVEL_INFO, "Opening dumper (path: %s)\n", filepath.c_str());
    if (this->outfile == NULL)
    {
        this->slot->top->trace.fatal("Unable to open output file (file: %s, error: %s)\n", filepath.c_str(), strerror(errno));
    }
}


void Tx_stream_raw_file::push_sample(uint32_t sample, int channel_id)
{
    if (this->is_bin)
    {
        if (this->encoding == PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_PLUSMINUS)
        {
            // Convert encoding from 0/1 to -1/+1
            if (sample == 0)
                sample = (uint32_t)-1;
            else if (sample == 1)
                sample = 1;
            else
                sample = 0; // Error
        }

        int nb_bytes = (this->width + 7) / 8;
        if (fwrite((void *)&sample, nb_bytes, 1, this->outfile) != 1)
        {
            return;
        }
    }
    else
    {
        fprintf(this->outfile, "0x%x\n", sample);
    }
}


Tx_stream_libsnd_file::Tx_stream_libsnd_file(I2s_verif *i2s, pi_testbench_i2s_verif_start_config_tx_file_dumper_type_e type, std::string filepath, int channels, int width, int pdm2pcm_is_true, int cic_r, int wav_sampling_freq)
: i2s(i2s)
{
#ifdef USE_SNDFILE

    unsigned int pcm_width;

    this->width = width != 0 ? width : i2s->config.word_size;

    switch (this->width)
    {
        case 8: pcm_width = SF_FORMAT_PCM_S8; break;
        case 16: pcm_width = SF_FORMAT_PCM_16; break;
        case 24: pcm_width = SF_FORMAT_PCM_24; break;
        case 32:
        default:
            pcm_width = SF_FORMAT_PCM_32; break;
        }

        this->sfinfo.format = pcm_width | (type == PI_TESTBENCH_I2S_VERIF_TX_FILE_DUMPER_TYPE_AU ? SF_FORMAT_AU : SF_FORMAT_WAV);
        if (pdm2pcm_is_true)
        {
            // in conversion context : sample rate = pdm_freq / cic_r
            if (wav_sampling_freq > 0)
                this->sfinfo.samplerate = wav_sampling_freq;
            else
                this->sfinfo.samplerate = i2s->config.sampling_freq / cic_r;
        }
        else
        {
            this->sfinfo.samplerate = i2s->config.sampling_freq;
        }
        this->sfinfo.channels = channels;
        this->sndfile = sf_open(filepath.c_str(), SFM_WRITE, &this->sfinfo);
        if (this->sndfile == NULL)
        {
            throw std::invalid_argument(("Failed to open file " + filepath + ": " + strerror(errno)).c_str());
    }
    this->period = 1000000000000UL / this->sfinfo.samplerate;

    this->pending_channels = 0;
    this->items = new int32_t[channels];
    memset(this->items, 0, sizeof(uint32_t)*this->sfinfo.channels);
#else

    this->i2s->top->trace.fatal("Unable to open file (%s), libsndfile support is not active\n", filepath.c_str());
    return;

#endif
}


Tx_stream_libsnd_file::~Tx_stream_libsnd_file()
{
#ifdef USE_SNDFILE
    if (this->pending_channels)
    {
        sf_writef_int(this->sndfile, (const int *)this->items, 1);
    }
    sf_close(this->sndfile);
#endif
}


void Tx_stream_libsnd_file::push_sample(uint32_t data, int channel)
{
#ifdef USE_SNDFILE
    if (((this->pending_channels >> channel) & 1) == 1)
    {
        sf_writef_int(this->sndfile, (const int *)this->items, 1);
        this->pending_channels = 0;
        memset(this->items, 0, sizeof(uint32_t)*this->sfinfo.channels);
    }

    data <<= (32 - this->width);
    items[channel] = data;
    this->pending_channels |= 1 << channel;
#endif
}


Rx_stream_raw_file::Rx_stream_raw_file(Slot *slot, std::string filepath, int width, bool is_bin, pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding)
{
    this->width = width;
    this->is_bin = is_bin;
    this->encoding = encoding;
    this->slot = slot;
    this->infile = fopen(filepath.c_str(), "r");
    if (this->infile == NULL)
    {
         this->slot->top->trace.fatal("Unable to open input file (file: %s, error: %s)\n", filepath.c_str(), strerror(errno));
    }
}


uint32_t Rx_stream_raw_file::get_sample(int channel_id)
{
    if (this->is_bin)
    {
        int nb_bytes = (this->width + 7) / 8;
        uint32_t result = 0;
        int freadres = fread((void *)&result, nb_bytes, 1, this->infile);

        // this->slot->top->trace.msg(vp::Trace::LEVEL_TRACE, "channel_id=%d, nb_bytes=%d, freadres=%d, result=%d\n", channel_id, nb_bytes, freadres, result);

        if (freadres != 1)
        {
            return 0;
        }

        if (this->encoding == PI_TESTBENCH_I2S_VERIF_FILE_ENCODING_TYPE_PLUSMINUS)
        {
            // Convert encoding from -1/+1 to 0/1
            if ((int32_t)result == -1)
                result = 0;
            else if ((int32_t)result == 1)
                result = 1;
            else
                result = 0;
        }

        return result;
    }
    else
    {
        char line [64];

        if (fgets(line, 64, this->infile) == NULL)
        {
            fseek(this->infile, 0, SEEK_SET);
            if (fgets(line, 16, this->infile) == NULL)
            {
                this->slot->top->trace.fatal("Unable to get sample from file (error: %s)\n", strerror(errno));
                return 0;
            }
        }

        return strtol(line, NULL, 0);
    }
}
*/

void Rx_stream::build_rx_stream(int nb_channels, int32_t desired_sample_rate, int32_t sample_rate)
{
    if (desired_sample_rate != sample_rate)
    {
#ifdef USE_SAMPLERATE
        if (nb_channels < 2) // TODO : manage several channels in interpolator
        {
            double ratio = (double)desired_sample_rate / (double)sample_rate;
            srcState = src_new(SRC_SINC_BEST_QUALITY, 1, nullptr);
            if (srcState)
            {
                // printf("Interpolation ON ! \n");
                interpolate = 1;
                output_buffer = new float[int(ratio) + 1];
                srcData.src_ratio = ratio;
                srcData.data_in = &input_buffer;
                srcData.data_out = output_buffer;

                srcData.input_frames = 1; // samples are given one by one to the resampler
                srcData.output_frames = static_cast<long>(ratio) + 1;
                srcData.output_frames_gen = 0;
                srcData.end_of_input = 0;
            }
        }
#else
        throw std::invalid_argument("Libsamplerate not found, please install it\n");
#endif
    }
}

Rx_stream::~Rx_stream()
{
#ifdef USE_SAMPLERATE
    if(output_buffer)
    {
        delete[] output_buffer; // Deallocate memory allocated for interpolation_context array
    }
    if (srcState)
    {
        src_delete(srcState);
    }
#endif
}

Tx_stream::~Tx_stream()
{
#ifdef USE_SAMPLERATE
    delete[] output_buffer; // Deallocate memory allocated for interpolation_context array
    if (srcState)
    {
        src_delete(srcState);
    }
#endif
}

uint32_t Rx_stream::get_sample_interpol(int channel_id)
{
#ifdef USE_SAMPLERATE
    if(srcState)
    {
        if (this->cnt_in_resampler >= srcData.output_frames_gen)
        {
            this->cnt_in_resampler = 0;
            do
            {
                uint32_t sample = get_sample(channel_id);
                int32_t int_sample = (int32_t)sample;
                input_buffer = (float)int_sample;
                if (src_process(srcState, &srcData) != 0)
                {
                    std::cout << "Error in resampling process" << std::endl;
                }
            } while (srcData.output_frames_gen == 0);
        }
        int32_t return_val = (int32_t)output_buffer[this->cnt_in_resampler++];
        return (uint32_t)return_val;
    }
    else{
        return 0;
    }
#else
    return 0;
#endif
}

void Rx_stream_wav::initialize(std::string filepath, uint32_t *samplerate)
{
#ifdef USE_SNDFILE
    // this->sfinfo.format = 0;
    // this->sfinfo.channels = nb_channels;
    this->sndfile = sf_open(filepath.c_str(), SFM_READ, &this->sfinfo);
    if (this->sndfile == NULL)
    {
        throw std::invalid_argument(("Failed to open file " + filepath + ": " + strerror(errno)).c_str());
    }
    // this->period = 1000000000000UL / this->sfinfo.samplerate;

    switch (this->sfinfo.format & SF_FORMAT_SUBMASK)
    {
    case SF_FORMAT_PCM_16:
        this->width = 16;
        break;
    case SF_FORMAT_PCM_24:
        this->width = 24;
        break;
    case SF_FORMAT_PCM_32:
        this->width = 32;
        break;
    default:
        throw std::invalid_argument("File format is not supported, please use a PCM format 16, 24 or 32");
        break;
    };
    *samplerate = this->sfinfo.samplerate;

    this->items = new int32_t[1]; // only one channel supported
#else
    throw std::invalid_argument("Libsnd file not found, please install it\n");
#endif
}

Rx_stream_wav::Rx_stream_wav(std::string filepath, uint32_t *samplerate)
{
    initialize(filepath, samplerate);
}

Rx_stream_wav::Rx_stream_wav(std::string filepath, uint32_t *samplerate, uint32_t des_samplerate)
{
    initialize(filepath, samplerate);
    // printf("Sample rate %i, desired %i \n", (int32_t) *samplerate, (int32_t) des_samplerate);
    build_rx_stream(1, des_samplerate, *samplerate);
}

void Tx_stream::build_tx_stream(int nb_channels, int32_t desired_sample_rate, int32_t sample_rate)
{
    if (desired_sample_rate != sample_rate)
    {
#ifdef USE_SAMPLERATE
        if (nb_channels < 2) // TODO : manage several channels in interpolator
        {
            double ratio = (double)desired_sample_rate / (double)sample_rate;
            srcState = src_new(SRC_SINC_BEST_QUALITY, 1, nullptr);
            if (srcState)
            {
                interpolate = 1;
                output_buffer = new float[int(ratio) + 1];
                srcData.src_ratio = ratio;
                srcData.data_in = &input_buffer;
                srcData.data_out = output_buffer;

                srcData.input_frames = 1; // samples are given one by one to the resampler
                srcData.output_frames = static_cast<long>(ratio) + 1;
                srcData.output_frames_gen = 0;
                srcData.end_of_input = 0;
            }
        }
#else
        throw std::invalid_argument("Libsamplerate not found, please install it\n");
#endif
    }
}

void Tx_stream::push_sample_interpol(uint32_t sample, int channel_id)
{
#ifdef USE_SAMPLERATE
    int32_t int_sample = (int32_t)sample;
    input_buffer = (float)int_sample;
    if (src_process(srcState, &srcData) != 0)
    {
        std::cout << "Error in resampling process" << std::endl;
    }
    for (int i = 0; i < srcData.output_frames_gen; i++)
    {
        int32_t return_val = (int32_t)output_buffer[i];
        push_sample((uint32_t)return_val, channel_id);
    }
#endif
}

Tx_stream_wav::Tx_stream_wav(std::string filepath, uint32_t width, uint32_t wav_desired_sampling_freq, uint32_t provided_sampling_freq)
    : width(width)
{
#ifdef USE_SNDFILE
    build_tx_stream(1, wav_desired_sampling_freq, provided_sampling_freq);
    unsigned int pcm_width;

    switch (this->width)
    {
    case 8:
        pcm_width = SF_FORMAT_PCM_S8;
        break;
    case 16:
        pcm_width = SF_FORMAT_PCM_16;
        break;
    case 24:
        pcm_width = SF_FORMAT_PCM_24;
        break;
    case 32:
        pcm_width = SF_FORMAT_PCM_32;
        break;
    default:
        pcm_width = SF_FORMAT_PCM_32;
        break;
    }

    this->sfinfo.format = pcm_width | SF_FORMAT_WAV;
    this->sfinfo.samplerate = wav_desired_sampling_freq;
    this->sfinfo.channels = 1;
    this->sndfile = sf_open(filepath.c_str(), SFM_WRITE, &this->sfinfo);
    if (this->sndfile == NULL)
    {
        throw std::invalid_argument(("Failed to open file " + filepath + ": " + strerror(errno)).c_str());
    }
    this->pending_channels = 0;
    this->items = new int32_t;
    memset(this->items, 0, sizeof(uint32_t) * this->sfinfo.channels);
#else
    throw std::invalid_argument("Libsnd file not found, please install it\n");
#endif
}

Tx_stream_wav::~Tx_stream_wav()
{
#ifdef USE_SNDFILE
    if (this->pending_channels)
    {
        sf_writef_int(this->sndfile, (const int *)this->items, 1);
    }
    sf_close(this->sndfile);
#endif
}

void Tx_stream_wav::push_sample(uint32_t data, int channel_id)
{
#ifdef USE_SNDFILE
    if (this->pending_channels == 1)
    {
        sf_writef_int(this->sndfile, (const int *)this->items, 1);
        this->pending_channels = 0;
        memset(this->items, 0, sizeof(uint32_t) * this->sfinfo.channels);
    }

    data <<= (32 - this->width);
    items[0] = data;
    this->pending_channels = 1;
#endif
}

uint32_t Rx_stream_wav::get_sample(int channel)
{
#ifdef USE_SNDFILE
    if (((this->pending_channels >> channel) & 1) == 0)
    {
        sf_readf_int(this->sndfile, this->items, 1);
        this->pending_channels = (1 << this->sfinfo.channels) - 1;
    }

    this->pending_channels &= ~(1 << channel);
    int32_t result = this->items[channel] >> (32 - this->width);
    if(this->width < 24)
    {
        int shift = 24 - this->width;
        result = result << shift;
    }

    return result;
#else
    return 0;
#endif
}
