#ifndef IO_AUDIO_H
#define IO_AUDIO_H

#include <stdio.h>
#include <string>
#include <stdint.h>
#include <cmath>
#ifdef USE_SNDFILE
#include <sndfile.hh>
#endif
#ifdef USE_SAMPLERATE
#include <samplerate.h>
#endif

class Rx_stream
{
public:
    void build_rx_stream(int nb_channels, int32_t desired_sample_rate, int32_t sample_rate);
    Rx_stream() : interpolate(0){};
    ~Rx_stream();
    virtual uint32_t get_sample(int channel_id) = 0;
    uint32_t get_sample_interpol(int channel_id);
    uint32_t get_sample()
    {
        if (interpolate)
        {
            return get_sample_interpol(0);
        }
        return get_sample(0);
    }
    int use_count = 0;
    int nb_channels = 1;

protected:
    // struct interpolation_context
    // {
    //     int64_t base_period;   // period between 2 samples in input signal
    //     int64_t period;        // period between two samples in output signal
    //     int64_t t1;            // timestamp of v1
    //     int64_t t2;            // timestamp of v2
    //     int64_t t;             // current time
    //     int32_t v1;
    //     int32_t v2;
    //     double coef;           // slope coefficient
    // };
    // interpolation_context* context = nullptr;
    // int64_t desired_sample_rate;
    // int64_t sample_rate;

#ifdef USE_SAMPLERATE
    SRC_DATA srcData;
    SRC_STATE *srcState;
#endif
    float input_buffer; // one sqmple buffer as input
    float *output_buffer;

private:
    int interpolate = 0;
    // void init_interpolation_context();
    uint32_t cnt_in_resampler = 0;
};

class Tx_stream
{
public:
    virtual void push_sample(uint32_t sample, int channel_id) = 0;
    Tx_stream() : interpolate(0){};
    void build_tx_stream(int nb_channels, int32_t desired_sample_rate, int32_t sample_rate);
    ~Tx_stream();
    void push_sample_interpol(uint32_t sample, int channel_id);
    void push_sample(uint32_t sample)
    {
        if (interpolate)
        {
            push_sample_interpol(sample, 0);
        }
        else
        {
            push_sample(sample, 0);
        }
    }

protected:
#ifdef USE_SAMPLERATE
    SRC_DATA srcData;
    SRC_STATE *srcState;
#endif
    float input_buffer; // one sample buffer as input
    float *output_buffer;
    uint32_t cnt_in_resampler = 0;

    int interpolate = 0;
    int use_count = 0;
};

class Signal_generator : public Rx_stream
{
public:
    Signal_generator(double signal_frequency, double amplitude, double sample_frequency)
        : signal_frequency(signal_frequency), sample_frequency(sample_frequency), amplitude(amplitude),
          current_time(0.0) {}

    uint32_t get_sample(int channel_id)
    {
        double sample = sin(2.0 * M_PI * signal_frequency * current_time) * amplitude;
        current_time += 1.0 / sample_frequency;

        // Conversion de la valeur du signal à une amplitude de 24 bits
        uint32_t gen_value = static_cast<int32_t>(sample * ((1 << 23) - 1));

        return gen_value;
    }

private:
    double signal_frequency;
    double sample_frequency;
    double current_time;
    double amplitude;
};

class Rx_stream_wav : public Rx_stream
{
public:
    Rx_stream_wav(std::string filepath, uint32_t *samplerate);
    // this constructor will enable the interpolator to match de desired samplerate
    Rx_stream_wav(std::string filepath, uint32_t *samplerate, uint32_t des_samplerate);
    uint32_t get_sample(int channel_id);

private:
    void initialize(std::string filepath, uint32_t *samplerate);

#ifdef USE_SNDFILE
    SNDFILE *sndfile;
    SF_INFO sfinfo;
#endif
    int width;
    uint32_t pending_channels;
    int32_t *items;
};

class Tx_stream_wav : public Tx_stream
{
public:
    Tx_stream_wav(std::string filepath, uint32_t width, uint32_t wav_desired_sampling_freq, uint32_t provided_sampling_freq);
    ~Tx_stream_wav();
    void push_sample(uint32_t sample, int channel_id);

private:
#ifdef USE_SNDFILE
    SNDFILE *sndfile;
    SF_INFO sfinfo;
#endif
    int width;
    uint32_t pending_channels;
    int32_t *items;
};

/*
class Rx_stream_raw_file : public Rx_stream
{
public:
    Rx_stream_raw_file(string filepath, int width);
    uint32_t get_sample(int channel_id);

private:
    FILE *infile;
    int width;
    bool is_bin;
};


class Tx_stream_raw_file : public Tx_stream
{
public:
    Tx_stream_raw_file(string filepath, int width);
    void push_sample(uint32_t sample, int channel_id);

private:
    FILE *outfile;
    int width;
    bool is_bin;
};

*/

#endif
