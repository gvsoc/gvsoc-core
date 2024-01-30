#include "io_audio.h"

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


Rx_stream_libsnd_file::Rx_stream_libsnd_file(I2s_verif *i2s, pi_testbench_i2s_verif_start_config_rx_file_reader_type_e type, std::string filepath, int nb_channels, int width)
{
    this->i2s = i2s;

#ifdef USE_SNDFILE

    unsigned int pcm_width;
    
    this->width = width != 0 ? width : i2s->config.word_size;
    this->pending_channels = 0;

    int format = type == PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_AU ? SF_FORMAT_AU : SF_FORMAT_WAV;

    this->sfinfo.format = 0;
    this->sfinfo.channels = nb_channels;
    this->sndfile = sf_open(filepath.c_str(), SFM_READ, &this->sfinfo);
    if (this->sndfile == NULL)
    {
        throw std::invalid_argument(("Failed to open file " + filepath + ": " + strerror(errno)).c_str());
    }
    this->period = 1000000000000UL / this->sfinfo.samplerate;

    this->last_data_time = -1;
    this->next_data_time = -1;

    this->items = new int32_t[nb_channels];
#else

    this->i2s->top->trace.fatal("Unable to open file (%s), libsndfile support is not active\n", filepath.c_str());
    return;

#endif
}

uint32_t Rx_stream_libsnd_file::get_sample(int channel)
{
#ifdef USE_SNDFILE

    if (((this->pending_channels >> channel) & 1) == 0)
    {
        sf_readf_int(this->sndfile, this->items, 1);
        this->pending_channels = (1 << this->sfinfo.channels) - 1;
    }

    this->pending_channels &= ~(1 << channel);
    int32_t result = this->items[channel] >> (32 - this->width);

    return result;
#else
    return 0;
#endif
}*/
