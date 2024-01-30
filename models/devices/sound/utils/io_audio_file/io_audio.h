
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <cmath>

#ifdef USE_SNDFILE
#include <sndfile.hh>
#endif


class Rx_stream {
public:
    virtual ~Rx_stream() {}
    virtual uint32_t get_sample(int channel_id) = 0;
    uint32_t get_sample() { return get_sample(0); }
    int use_count = 0;
};


class Signal_generator : public Rx_stream {
public:
    Signal_generator(double signal_frequency, double sample_frequency)
        : signal_frequency(signal_frequency), sample_frequency(sample_frequency),
          current_time(0.0) {}

    uint32_t get_sample(int channel_id) {
        double sample = sin(2.0 * M_PI * signal_frequency * current_time);
        current_time += 1.0 / sample_frequency;

        // Conversion de la valeur du signal à une amplitude de 24 bits
        uint32_t amplitude = static_cast<int32_t>(sample * ((1 << 23) - 1));

        return amplitude;
    }

private:
    double signal_frequency;
    double sample_frequency;
    double current_time;
};




class Tx_stream
{
public:
    virtual ~Tx_stream() {};
    virtual void push_sample(uint32_t sample, int channel_id) = 0;
    void push_sample(uint32_t sample) { push_sample(sample, 0); }
    int use_count = 0;
};


/*
class Rx_stream_libsnd_file : public Rx_stream
{
public:
    Rx_stream_libsnd_file(int type, string filepath, int nb_channels, int width);
    uint32_t get_sample(int channel_id);

private:
#ifdef USE_SNDFILE
    SNDFILE *sndfile;
    SF_INFO sfinfo;
#endif
    int period;
    int width;
    uint32_t pending_channels;

    int64_t last_data_time;
    long long last_data;
    int64_t next_data_time;
    long long next_data;

    int32_t *items;
};


class Rx_stream_raw_file : public Rx_stream
{
public:
    Rx_stream_raw_file(string filepath, int width, bool is_bin, pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding);
    uint32_t get_sample(int channel_id);

private:
    FILE *infile;
    int width;
    bool is_bin;
    pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding;
};


class Tx_stream_raw_file : public Tx_stream
{
public:
    Tx_stream_raw_file(string filepath, int width, bool is_bin, pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding);
    void push_sample(uint32_t sample, int channel_id);

private:
    FILE *outfile;
    int width;
    bool is_bin;
    pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding;
};


class Tx_stream_libsnd_file : public Tx_stream
{
public:
    Tx_stream_libsnd_file(I2s_verif *i2s, pi_testbench_i2s_verif_start_config_tx_file_dumper_type_e type, string filepath, int channels, int width, int pdm2pcm_is_true, int cic_r, int wav_sampling_freq);
    ~Tx_stream_libsnd_file();
    void push_sample(uint32_t sample, int channel_id);

private:
    I2s_verif *i2s;
#ifdef USE_SNDFILE
    SNDFILE *sndfile;
    SF_INFO sfinfo;
#endif
    int period;
    int width;
    uint32_t pending_channels;
    int32_t *items;
};


*/

