/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
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
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#if defined(USE_SNDFILE) && defined(__M32_MODE)
#undef USE_SNDFILE
#endif

#include <vp/vp.hpp>
#include "i2s_verif.hpp"
#ifdef USE_SNDFILE
#include <sndfile.hh>
#endif

#include "pcm_pdm_conversion.hpp"

class Slot;


class Rx_stream
{
public:
    virtual ~Rx_stream() {};
    virtual uint32_t get_sample(int channel_id) = 0;
    int use_count = 0;
};


class Tx_stream
{
public:
    virtual ~Tx_stream() {};
    virtual void push_sample(uint32_t sample, int channel_id) = 0;
    int use_count = 0;
};


class Rx_stream_libsnd_file : public Rx_stream
{
public:
    Rx_stream_libsnd_file(I2s_verif *i2s, pi_testbench_i2s_verif_start_config_rx_file_reader_type_e type, string filepath, int nb_channels, int width);
    uint32_t get_sample(int channel_id);
    I2s_verif *i2s;

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
    Rx_stream_raw_file(Slot *slot, string filepath, int width, bool is_bin, pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding);
    uint32_t get_sample(int channel_id);
    Slot *slot;

private:
    FILE *infile;
    int width;
    bool is_bin;
    pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding;
};


class Tx_stream_raw_file : public Tx_stream
{
public:
    Tx_stream_raw_file(Slot *slot, string filepath, int width, bool is_bin, pi_testbench_i2s_verif_start_config_file_encoding_type_e encoding);
    void push_sample(uint32_t sample, int channel_id);
    Slot *slot;

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


class Rx_stream_iter : public Rx_stream
{
public:
    Rx_stream_iter();
    uint32_t get_sample(int channel_id);

private:
};


class Slot : public vp::Block
{
    friend class Rx_stream_libsnd_file;
    friend class Rx_stream_raw_file;
    friend class Tx_stream_libsnd_file;
    friend class Tx_stream_raw_file;
public:
    Slot(Testbench *top, I2s_verif *i2s, int itf, int id);
    void setup(pi_testbench_i2s_verif_slot_config_t *config);
    void start(pi_testbench_i2s_verif_slot_start_config_t *config, Slot *reuse_slot = NULL, int nb_channels=1, int channel_id=0);
    void stop(pi_testbench_i2s_verif_slot_stop_config_t *config);
    void start_frame();
    int get_data();
    void send_data(int sdo);
    void pdm_sync(int sd);
    void pdm_get();
    int64_t exec();

protected:
    Testbench *top;
    pi_testbench_i2s_verif_slot_config_t config_rx;

private:
    I2s_verif *i2s;
    int id;
    vp::Trace trace;
    pi_testbench_i2s_verif_slot_config_t config_tx;
    pi_testbench_i2s_verif_slot_start_config_t start_config_rx;
    pi_testbench_i2s_verif_slot_start_config_t start_config_tx;
    bool rx_started;
    int rx_pending_bits;
    int rx_current_value;
    uint32_t rx_pending_value;

    bool tx_started;
    int tx_pending_bits;
    uint32_t tx_pending_value;
    FILE *outfile;
    FILE *infile;
    Tx_stream *outstream;
    Rx_stream *instream;
    int rx_channel_id;
    std::vector<int> tx_channel_id;
    bool close;
    
    uint32_t cmpt = 0; // count number of pdm bits to send
    PcmToPdm* pcm2pdm = nullptr;
    PdmToPcm* pdm2pcm = nullptr;
    bool got_pcm_data = false;

    int32_t pcm_data;

};


I2s_verif::~I2s_verif()
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Closing I2S verif\n");
    this->time.get_engine()->dequeue(this);
    this->itf->sync(2, 2, (2 << 2) | 2, this->is_full_duplex);
}


I2s_verif::I2s_verif(Testbench *top, vp::I2sMaster *itf, int itf_id, pi_testbench_i2s_verif_config_t *config)
  : vp::Block(top, "i2s_" + std::to_string(itf_id)), top(top)
{
    ::memcpy(&this->config, config, sizeof(pi_testbench_i2s_verif_config_t));

    this->itf = itf;
    this->prev_ws = 0;
    this->frame_active = false;
    this->ws_delay = config->ws_delay;
    this->zero_delay_start = this->ws_delay == 0;
    this->sync_ongoing = false;
    this->current_ws_delay = 0;
    this->is_pdm = config->is_pdm;
    this->is_full_duplex = config->is_full_duplex;
    this->prev_sck = 0;
    this->is_ext_clk = config->is_ext_clk;
    this->clk_active = false;

    memset(this->pdm_lanes_is_out, 0, sizeof(this->pdm_lanes_is_out));

    this->clk = 2;
    this->propagated_clk = 0;
    this->propagated_ws = 0;
    this->ws_count = 0;
    this->ws_value = 2;
    this->data = (2 << 2) | 2;

    if (this->is_pdm)
    {
        this->config.nb_slots = 6;

        if (this->is_ext_clk)
        {
            if (config->sampling_freq <= 0)
            {
                this->trace.fatal("Invalid sampling frequency with external clock (sampling_freq: %d)\n", config->sampling_freq);
                return;
            }

            this->clk_period = 1000000000000ULL / config->sampling_freq / 2;

            this->clk = 0;
        }
    }
    else
    {
        if (this->is_ext_clk)
        {
            if (config->sampling_freq <= 0)
            {
                this->trace.fatal("Invalid sampling frequency with external clock (sampling_freq: %d)\n", config->sampling_freq);
                return;
            }

            this->clk_period = 1000000000000ULL / config->sampling_freq / config->nb_slots / config->word_size / 2;

            this->clk = 0;
        }
    }

    if (config->sampling_freq > 0)
    {
        this->sampling_period = 1000000000000ULL / config->sampling_freq;
    }
    else
    {
        this->sampling_period = 0;
    }

    if (config->is_sai0_clk)
    {
        top->i2ss[0]->clk_propagate |= 1 << itf_id;
    }

    if (config->is_sai0_ws)
    {
        top->i2ss[0]->ws_propagate |= 1 << itf_id;
    }

    top->traces.new_trace("i2s_verif_itf" + std::to_string(itf_id), &trace, vp::DEBUG);

    for (int i=0; i<this->config.nb_slots; i++)
    {
        this->slots.push_back(new Slot(top, this, itf_id, i));
    }

    if (this->config.is_ext_ws)
    {
        this->ws_value = 0;
    }

    if (this->config.is_ext_clk)
    {
        this->clk = 0;
    }

    this->prev_frame_start_time = -1;

    this->itf->sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
}


void I2s_verif::slot_setup(pi_testbench_i2s_verif_slot_config_t *config)
{
    if (config->slot >= this->config.nb_slots)
    {
        this->trace.fatal("Trying to configure invalid slot (slot: %d, nb_slot: %d)", config->slot, this->config.nb_slots);
        return;
    }

    this->slots[config->slot]->setup(config);

}


void I2s_verif::slot_start(pi_testbench_i2s_verif_slot_start_config_t *config, std::vector<int> slots)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Starting (slot: %d, nb_samples: %d, incr_start: 0x%x, incr_end: 0x%x, incr_value: 0x%x)\n",
        config->slot, config->rx_iter.nb_samples, config->rx_iter.incr_start, config->rx_iter.incr_end, config->rx_iter.incr_value);

    if (slots.size() > 1)
    {
        Slot *reuse_slot = NULL;

        int channel_id = 0;
        for (auto slot_id: slots)
        {
            if (slot_id != -1)
            {
                Slot *slot = this->slots[slot_id];
                slot->start(config, reuse_slot, slots.size(), channel_id);
                reuse_slot = slot;
            }
            channel_id++;
        }
    }
    else
    {
        int slot = config->slot;

        if (slot >= this->config.nb_slots)
        {
            this->trace.fatal("Trying to configure invalid slot (slot: %d, nb_slot: %d)\n", slot, this->config.nb_slots);
            return;
        }

        this->slots[slot]->start(config);
    }
}



void I2s_verif::slot_stop(pi_testbench_i2s_verif_slot_stop_config_t *config)
{
    this->trace.msg(vp::Trace::LEVEL_INFO, "Stopping (slot: %d)\n", config->slot);

    int slot = config->slot;

    if (slot >= this->config.nb_slots)
    {
        this->trace.fatal("Trying to configure invalid slot (slot: %d, nb_slot: %d)", slot, this->config.nb_slots);
        return;
    }

    this->slots[slot]->stop(config);
}


void I2s_verif::sync_sck(int sck)
{
    this->propagated_clk = sck;
    this->sync(sck, this->ws, this->sdio, this->is_full_duplex);
}

void I2s_verif::sync_ws(int ws)
{
    this->propagated_ws = ws;
    this->sync(this->prev_sck, ws, this->sdio, this->is_full_duplex);
}


void I2s_verif::set_pdm_data(int slot, int data)
{
    this->data = (this->data & ~(0x3 << (slot * 2))) | (data << (slot * 2));
    if (slot == 2)
        this->ws_value = data;
    this->itf->sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
}


void I2s_verif::sync(int sck, int ws, int sdio, bool is_full_duplex)
{
    bool got_data = false;

    if (this->sync_ongoing)
        return;

    this->sync_ongoing = true;

    this->sdio = sdio;

    if (this->config.is_sai0_clk)
    {
        sck = this->propagated_clk;
    }

    if (this->config.is_sai0_ws)
    {
        ws = this->propagated_ws;
    }

    sck = sck ^ this->config.clk_polarity;
    ws = ws ^ this->config.ws_polarity;

    this->ws = ws;

    int sd = this->is_full_duplex ? sdio >> 2 : sdio & 0x3;

    if (!this->is_pdm && this->zero_delay_start && this->prev_ws != ws && ws == 1 && !this->config.is_ext_ws)
    {
        got_data = true;
        this->zero_delay_start = false;
        this->frame_active = true;
        this->active_slot = 0;
        this->pending_bits = this->config.word_size;
        this->slots[0]->start_frame();
        this->data = this->slots[0]->get_data() | (2 << 2);
        this->itf->sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
    }

    if (sck != this->prev_sck)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "I2S edge (sck: %d, ws: %d, sdo: %d)\n", sck, ws, sd);
        
        this->prev_sck = sck;

        if (this->is_pdm)
        {
            if (sck == 0)
            {
                this->slots[0]->pdm_get();
                this->slots[2]->pdm_get();
                this->slots[4]->pdm_get();
            }
            else if (sck == 1)
            {
                this->slots[0]->pdm_sync(sdio & 3);
                this->slots[2]->pdm_sync(sdio >> 2);
                this->slots[4]->pdm_sync(ws);

                this->slots[1]->pdm_get();
                this->slots[3]->pdm_get();
                this->slots[5]->pdm_get();
            }
        }
        else
        {
            if (sck == 1)
            {
                // The channel is the one of this microphone
                if (this->prev_ws != ws && ws == 1)
                {
                    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Detected frame start\n");

                    if (this->sampling_period != 0 && this->prev_frame_start_time != -1)
                    {
                        int64_t measured_period = this->time.get_time() - this->prev_frame_start_time;

                        float error = ((float)measured_period - this->sampling_period) / this->sampling_period * 100;
                        if (error < 0)
                            error = -error;

                        if (error >= 10)
                        {
                            //this->trace.fatal("Detected wrong period (expected: %ld ps, measured: %ld ps)\n", this->sampling_period, measured_period);
                            //goto end;
                        }
                    }

                    this->prev_frame_start_time = this->time.get_time();

                    // If the WS just changed, apply the delay before starting sending
                    this->current_ws_delay = this->ws_delay;
                }

                if (this->frame_active)
                {
                    this->slots[this->active_slot]->send_data(sd);

                    this->pending_bits--;

                    if (this->pending_bits == 0)
                    {
                        this->pending_bits = this->config.word_size;
                        this->active_slot++;
                        if (this->active_slot == this->config.nb_slots)
                        {
                            this->frame_active = false;
                            if (this->ws_delay == 0)
                            {
                                this->zero_delay_start = true;
                            }
                        }
                    }

                }

                // If there is a delay, decrease it
                if (this->current_ws_delay > 0)
                {
                    this->current_ws_delay--;
                    if (this->current_ws_delay == 0)
                    {
                        this->frame_active = true;
                        this->pending_bits = this->config.word_size;
                        this->active_slot = 0;
                    }
                }

                this->prev_ws = ws;
            }
            else if (!sck)
            {
                if (this->frame_active)
                {
                    if (!got_data)
                    {
                        if (this->pending_bits == this->config.word_size)
                        {
                            this->slots[this->active_slot]->start_frame();
                        }
                        this->data = this->slots[this->active_slot]->get_data() | (2 << 2);

                        this->trace.msg(vp::Trace::LEVEL_TRACE, "I2S output data (sdi: 0x%x)\n", this->data & 3);
    
                        this->itf->sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
                    }
                }
            }

            if (sck == 1)
            {
                if (this->config.is_ext_ws)
                {
                    if (this->ws_count == 0 || this->ws_value == 1)
                    {
                        if (this->ws_count == 0)
                        {
                            this->ws_count = this->config.word_size * this->config.nb_slots;
                        }

                        // Delay a bit the WS so that it does not raise at the same time as the clock
                        this->ws_gen_timestamp = this->time.get_time() + this->config.ws_trigger_delay + 100;

                        if (!this->time.get_is_enqueued() || this->ws_gen_timestamp < this->time.get_next_event_time())
                        {
                            if (this->time.get_is_enqueued())
                            {
                                this->time.dequeue_from_engine();
                            }
                            this->time.enqueue_to_engine(this->ws_gen_timestamp);
                        }
                    }
                    this->ws_count--;
                }
            }
        }
    }

end:
    this->sync_ongoing = false;
}



void I2s_verif::start(pi_testbench_i2s_verif_start_config_t *config)
{
    this->clk_active = config->start;
    this->prev_frame_start_time = -1;

    if (this->clk_active && this->is_ext_clk)
    {
            this->clk_gen_timestamp = this->time.get_time() + this->clk_period;
            if (!this->time.get_is_enqueued() || this->clk_gen_timestamp < this->time.get_next_event_time())
            {
                this->time.enqueue_to_engine(this->clk_gen_timestamp);
            }
    }
}


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
            this->i2s->top->trace.fatal("Failed to open file filepath %s: %s\n",  filepath.c_str(), strerror(errno));
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
        this->i2s->top->trace.fatal("Failed to open file filepath %s: %s\n",  filepath.c_str(), strerror(errno));
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

#if 0
    // FIXME do not work anymore with multi-channel support
    if (this->period == 0)
        return result;

    if (this->last_data_time == -1)
    {
        this->last_data = result;
        this->last_data_time = this->i2s->top->time.get_time();
    }

    if (this->next_data_time == -1)
    {
        this->next_data = result;
        this->next_data_time = this->last_data_time + this->period;
    }

    // Get samples from the file until the current timestamp fits the window
    while (this->i2s->top->time.get_time() >= this->next_data_time)
    {
        this->last_data_time = this->next_data_time;
        this->last_data = this->next_data;
        this->next_data_time = this->last_data_time + this->period;
        this->next_data = result;
    }

    // Now do the interpolation between the 2 known samples
    float coeff = (float)(this->i2s->top->time.get_time() - this->last_data_time) / (this->next_data_time - this->last_data_time);
    float value = (float)this->last_data + (float)(this->next_data - this->last_data) * coeff;

    this->i2s->top->trace.msg(vp::Trace::LEVEL_TRACE, "Interpolated new sample (value: %d, timestamp: %ld, prev_timestamp: %ld, next_timestamp: %ld, prev_value: %d, next_value: %d)", value, this->i2s->top->time.get_time(), last_data_time, next_data_time, last_data, next_data);
#endif

    return result;
#else
    return 0;
#endif
}


Slot::Slot(Testbench *top, I2s_verif *i2s, int itf, int id) : vp::Block(top, "slot_" + std::to_string(itf)), top(top), i2s(i2s), id(id)
{
    top->traces.new_trace("i2s_verif_itf" + std::to_string(itf) + "_slot" + std::to_string(id), &trace, vp::DEBUG);

    this->config_rx.enabled = false;
    this->rx_started = false;
    this->tx_started = false;
    this->infile = NULL;
    this->instream = NULL;
    this->outfile = NULL;
    this->outstream = NULL;
}


void Slot::setup(pi_testbench_i2s_verif_slot_config_t *config)
{

    if (config->word_size == 0)
    {
        config->word_size = this->i2s->config.word_size;
    }

    if (config->is_rx)
    {
        ::memcpy(&this->config_rx, config, sizeof(pi_testbench_i2s_verif_slot_config_t));
    }
    else
    {
        ::memcpy(&this->config_tx, config, sizeof(pi_testbench_i2s_verif_slot_config_t));
    }

    this->trace.msg(vp::Trace::LEVEL_INFO, "Slot setup (is_rx: %d, enabled: %d, word_size: %d, format: 0x%x)\n",
        config->is_rx, config->enabled, config->word_size, config->format);
}


void Slot::start(pi_testbench_i2s_verif_slot_start_config_t *config, Slot *reuse_slot, int nb_channels, int channel_id)
{
    if (config->type == PI_TESTBENCH_I2S_VERIF_RX_ITER)
    {
        ::memcpy(&this->start_config_rx, config, sizeof(pi_testbench_i2s_verif_slot_start_config_t));

        this->start_config_rx.rx_iter.incr_end &= (1ULL << this->config_rx.word_size) - 1;
        this->start_config_rx.rx_iter.incr_start &= (1ULL << this->config_rx.word_size) - 1;

        if (this->start_config_rx.rx_iter.incr_value >= this->start_config_rx.rx_iter.incr_end)
            this->start_config_rx.rx_iter.incr_value = 0;

        this->rx_started = true;
        this->rx_current_value = this->start_config_rx.rx_iter.incr_start;
        this->rx_pending_bits = 0;
    }
    else if (config->type == PI_TESTBENCH_I2S_VERIF_TX_FILE_DUMPER)
    {
        this->tx_channel_id.push_back(channel_id);

        if (this->tx_channel_id.size() == 1)
        {
            ::memcpy(&this->start_config_tx, config, sizeof(pi_testbench_i2s_verif_slot_start_config_t));

            this->tx_started = true;

            char *filepath = (char *)config + sizeof(pi_testbench_i2s_verif_slot_start_config_t);

            if (reuse_slot)
            {
                this->outstream = reuse_slot->outstream;
            }
            else
            {
                if (config->tx_file_dumper.type == PI_TESTBENCH_I2S_VERIF_TX_FILE_DUMPER_TYPE_RAW || config->tx_file_dumper.type == PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_BIN)
                {
                    this->outstream = new Tx_stream_raw_file(
                        this,
                        filepath,
                        config->tx_file_dumper.width,
                        config->tx_file_dumper.type == PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_BIN,
                        (pi_testbench_i2s_verif_start_config_file_encoding_type_e)config->tx_file_dumper.encoding);
                }
                else
                {
                    this->outstream = new Tx_stream_libsnd_file(this->i2s, (pi_testbench_i2s_verif_start_config_tx_file_dumper_type_e)config->tx_file_dumper.type, filepath, nb_channels, config->tx_file_dumper.width, config->tx_file_dumper.pdm2pcm_is_true, config->tx_file_dumper.conversion_config.cic_r, config->tx_file_dumper.wav_sampling_freq);
                }
            }
            this->outstream->use_count++;

            if (this->i2s->is_pdm)
            {
                this->i2s->pdm_lanes_is_out[this->id / 2] = false;
            }
        }
        if (config->tx_file_dumper.pdm2pcm_is_true){
            this->pdm2pcm = new PdmToPcm(config->tx_file_dumper.conversion_config.cic_r, config->tx_file_dumper.conversion_config.cic_n, config->tx_file_dumper.conversion_config.cic_m, config->tx_file_dumper.conversion_config.cic_shift);
        }
    }
    else if (config->type == PI_TESTBENCH_I2S_VERIF_RX_FILE_READER)
    {
        ::memcpy(&this->start_config_rx, config, sizeof(pi_testbench_i2s_verif_slot_start_config_t));

        this->rx_started = true;
        this->rx_channel_id = channel_id;

        char *filepath = (char *)config + sizeof(pi_testbench_i2s_verif_slot_start_config_t);

        if (reuse_slot)
        {
            this->instream = reuse_slot->instream;
        }
        else
        {
            if (config->rx_file_reader.type == PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_RAW || config->rx_file_reader.type == PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_BIN)
            {
                this->instream = new Rx_stream_raw_file(
                    this,
                    filepath,
                    config->rx_file_reader.width,
                    config->rx_file_reader.type == PI_TESTBENCH_I2S_VERIF_RX_FILE_READER_TYPE_BIN,
                    (pi_testbench_i2s_verif_start_config_file_encoding_type_e)config->rx_file_reader.encoding);
            }
            else
            {
                this->instream = new Rx_stream_libsnd_file(this->i2s, (pi_testbench_i2s_verif_start_config_rx_file_reader_type_e)config->rx_file_reader.type, filepath, nb_channels, config->rx_file_reader.width);
            }
        }
        this->instream->use_count++;

        if (this->i2s->is_pdm)
        {
            this->i2s->pdm_lanes_is_out[this->id / 2] = true;
            if ((this->id == 4) || (this->id == 5))
                this->i2s->ws_value = 0;
        }


        if (config->rx_file_reader.pcm2pdm_is_true){
            this->pcm2pdm = new PcmToPdm(config->rx_file_reader.conversion_config.interpolation_ratio_shift, config->rx_file_reader.conversion_config.interpolation_type);
        }

        this->i2s->data = this->id < 2 ? this->i2s->data & 0xC : this->i2s->data & 0x3;

        this->i2s->itf->sync(2, this->i2s->ws_value, this->i2s->data, this->i2s->is_full_duplex);
    }
}


void Slot::stop(pi_testbench_i2s_verif_slot_stop_config_t *config)
{
    if (config->stop_rx)
    {
        this->rx_started = false;

        if (this->infile)
        {
            fclose(this->infile);
            this->infile = NULL;
        }

        if (this->instream)
        {
            this->instream->use_count--;
            if (this->instream->use_count == 0)
            {
                delete this->instream;
            }
            this->instream = NULL;
        }
    }

    if (config->stop_tx)
    {

        if (this->outfile)
        {
            fclose(this->outfile);
            this->outfile = NULL;
        }

        if (this->outstream)
        {
            this->outstream->use_count--;
            if (this->outstream->use_count == 0)
            {
                delete this->outstream;
            }
            this->outstream = NULL;
        }
    }
    delete this->pcm2pdm;
    delete this->pdm2pcm;
}


void Slot::start_frame()
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Start frame\n");

    if (this->rx_started)
    {
        if (this->instream)
        {
            this->rx_pending_bits = this->i2s->config.word_size;
            this->rx_pending_value = this->instream->get_sample(this->rx_channel_id);
        }
        else
        {
            if (this->start_config_rx.rx_iter.nb_samples > 0 || this->start_config_rx.rx_iter.nb_samples == -1)
            {
                this->rx_pending_bits = this->i2s->config.word_size;
                this->rx_pending_value = this->rx_current_value;
            }
        }

        if (this->rx_pending_bits > 0)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Starting RX sample (sample: 0x%x)\n", this->rx_pending_value);

            int msb_first = (this->config_rx.format >> 0) & 1;
            int left_align = (this->config_rx.format >> 1) & 1;
            int sign_extend = (this->config_rx.format >> 2) & 1;
            int dummy_cycles = this->i2s->config.word_size - this->config_rx.word_size;

            if (this->start_config_rx.type == PI_TESTBENCH_I2S_VERIF_RX_ITER)
            {
                if (this->start_config_rx.rx_iter.incr_end - this->rx_current_value <= this->start_config_rx.rx_iter.incr_value)
                {
                    this->rx_current_value = this->start_config_rx.rx_iter.incr_start;
                }
                else
                {
                    this->rx_current_value += this->start_config_rx.rx_iter.incr_value;
                }
                if (this->start_config_rx.rx_iter.nb_samples > 0)
                {
                    this->start_config_rx.rx_iter.nb_samples--;
                }
            }

            bool changed = false;
            if (!msb_first)
            {
                changed = true;
                uint32_t value = 0;
                for (int i=0; i<this->config_rx.word_size; i++)
                {
                    value = (value << 1) | (this->rx_pending_value & 1);
                    this->rx_pending_value >>= 1;
                }
                this->rx_pending_value = value;
            }

            if (dummy_cycles)
            {
                changed = true;

                if (left_align)
                {
                    this->rx_pending_value = this->rx_pending_value << dummy_cycles;
                }
                else
                {
                    this->rx_pending_value = this->rx_pending_value & (1ULL << this->config_rx.word_size) - 1;
                }
            }

            if (changed)
            {
                this->trace.msg(vp::Trace::LEVEL_DEBUG, "Adapted sample to format (sample: 0x%x)\n", this->rx_pending_value);
            }
        }
    }

    if (this->tx_started && (this->start_config_tx.tx_file_dumper.nb_samples > 0 || this->start_config_tx.tx_file_dumper.nb_samples == -1))
    {
        this->tx_pending_value = 0;
        this->tx_pending_bits = this->i2s->config.word_size;
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Starting TX sample\n");

    }
}

void Slot::send_data(int sd)
{
    if (this->tx_started)
    {
        if (this->tx_pending_bits > 0)
        {
            this->tx_pending_bits--;
            this->tx_pending_value = (this->tx_pending_value << 1) | (sd == 1);

            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sampling bit (bit: %d, value: 0x%lx, remaining_bits: %d)\n", sd, this->tx_pending_value, this->tx_pending_bits);

            if (this->tx_pending_bits == 0)
            {
                int msb_first = (this->config_tx.format >> 0) & 1;
                int left_align = (this->config_tx.format >> 1) & 1;
                int sign_extend = (this->config_tx.format >> 2) & 1;
                int dummy_cycles = this->i2s->config.word_size - this->config_tx.word_size;

                if (dummy_cycles)
                {
                    if (msb_first)
                    {
                        if (left_align)
                        {
                            this->tx_pending_value >>= dummy_cycles;
                        }
                        else
                        {
                        }
                    }
                    else
                    {
                        if (left_align)
                        {
                            uint32_t value = 0;
                            for (int i=0; i<this->i2s->config.word_size; i++)
                            {
                                value = (value << 1) | (this->tx_pending_value & 1);
                                this->tx_pending_value >>= 1;
                            }
                            this->tx_pending_value = value;
                        }
                        else
                        {
                            uint32_t value = 0;
                            for (int i=0; i<this->config_tx.word_size; i++)
                            {
                                value = (value << 1) | (this->tx_pending_value & 1);
                                this->tx_pending_value >>= 1;
                            }
                            this->tx_pending_value = value;
                        }
                    }
                }
                else
                {
                    if (!msb_first)
                    {
                        uint32_t value = 0;
                        for (int i=0; i<this->config_tx.word_size; i++)
                        {
                            value = (value << 1) | (this->tx_pending_value & 1);
                            this->tx_pending_value >>= 1;
                        }
                        this->tx_pending_value = value;
                    }
                }

                this->trace.msg(vp::Trace::LEVEL_DEBUG, "Writing sample (value: 0x%lx)\n", this->tx_pending_value);
                if (this->outstream)
                {
                    for (auto channel_id: this->tx_channel_id)
                    {
                        this->outstream->push_sample(this->tx_pending_value, channel_id);
                    }
                }
                this->tx_pending_bits = this->i2s->config.word_size;
            }
        }
    }
}


int Slot::get_data()
{
    if (this->rx_started)
    {
        int data;

        if (this->rx_pending_bits > 0)
        {
            data = (this->rx_pending_value >> (this->i2s->config.word_size - 1)) & 1;
            this->rx_pending_bits--;
            this->rx_pending_value <<= 1;
        }
        else
        {
            if (this->start_config_rx.rx_iter.nb_samples == 0)
            {
                this->rx_started = false;
            }
            data = 0;
        }

        this->trace.msg(vp::Trace::LEVEL_TRACE, "Getting data (data: %d)\n", data);

        return data;
    }

    return 2;
}


void Slot::pdm_sync(int sd)
{
    // sd : 0 / 1
    if (this->outstream)
    {
        /* Case where a conversion is required to send pcm data instread of pdm*/
        if (this->start_config_tx.tx_file_dumper.pdm2pcm_is_true)
        {
            int input_bit = (sd > 0) ? 1 : -1;
            // producing on pcm sample require several pdm samples
            // convert method says when pcm data is ready to be sent
            if (this->pdm2pcm->convert(input_bit))
            {
                int out = this->pdm2pcm->pcm_output;
                this->outstream->push_sample(out, this->tx_channel_id[0]);
            }
        }
        else
        {
            this->outstream->push_sample(sd, this->tx_channel_id[0]);
        }
    }
}

int64_t Slot::exec()
{
    /* called by pdm_get(), takes care of gap physical constraints
     - called for all the slots of gap9 at each step
     - 2 is a specific value annoiucing an undefined state
     - send pdm data bit by bit*/
    if (close)
    {
        this->i2s->set_pdm_data(this->id / 2, 2);
        this->close = false;
        return 17000;
    }
    else
    {
        /* Case where a conversion is required before sending pdm data*/
        if (this->rx_started && this->start_config_rx.rx_file_reader.pcm2pdm_is_true)
        {
            int data;
            
            // one pcm sample gives several pdm samples
            // pdm samples are then progressively sent 
            if (cmpt >= this->pcm2pdm->output_size)
                cmpt = 0;
            if (cmpt == 0)
            {
                if (this->instream)
                {
                    int32_t pcm_data;
                    pcm_data = this->instream->get_sample(this->rx_channel_id); // here data is a int32
                    // make sure value uses 24 bits MSB
                    if(this->start_config_rx.rx_file_reader.width < 24){
                        int shift = 24 - this->start_config_rx.rx_file_reader.width;
                        pcm_data = pcm_data << shift;
                    }
                    // convert it to pdm
                    got_pcm_data = true;
                    this->pcm2pdm->convert(pcm_data);
                }
                else
                    got_pcm_data = false;
            }
            if (got_pcm_data)
            {
                data = (int)this->pcm2pdm->pdm_output[cmpt++];
                // pdm data must be 0/1
                data = (data <= 0) ? 0 : 1;
            }
            else
            {

                if (this->i2s->pdm_lanes_is_out[this->id / 2])
                    data = 0;
                else
                    data = 2;
            }
            this->i2s->set_pdm_data(this->id / 2, data);
            return -1;
        }

        else
        {
            int data;

            if (this->instream)
            {
                data = this->instream->get_sample(this->rx_channel_id);
            
            }
            else
            {
                if (this->i2s->pdm_lanes_is_out[this->id / 2])
                    data = 0;
                else
                    data = 2;
            }
                
            this->i2s->set_pdm_data(this->id / 2, data);
            return -1;
        }
    }
}


void Slot::pdm_get()
{
    // call exec() and takes care of time delay between sent bits
    this->close = true;
    this->time.enqueue_to_engine(this->time.get_time() + 4000);
}


int64_t I2s_verif::exec()
{
    if (this->clk_active)
    {
        if (this->time.get_time() == this->clk_gen_timestamp)
        {
            this->clk ^= 1;

            this->itf->sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
            this->clk_gen_timestamp = this->time.get_time() + this->clk_period;
            this->itf->sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
        }

    }

    if (this->time.get_time() == this->ws_gen_timestamp)
    {
        if (this->ws_value == 0)
        {
            this->ws_value = 1;
            if (this->ws_delay == 0)
            {
                this->frame_active = true;
                this->active_slot = 0;
                this->pending_bits = this->config.word_size;
            }
            this->ws_gen_timestamp = -1;
        }
        else
        {
            this->ws_value = 0;
            this->ws_gen_timestamp = -1;
        }
        this->itf->sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
    }

    int64_t next_timestamp = -1;

    if (next_timestamp == -1 || this->ws_gen_timestamp < next_timestamp)
    {
        next_timestamp = this->ws_gen_timestamp;
    }

    if (next_timestamp == -1 || this->clk_gen_timestamp < next_timestamp)
    {
        next_timestamp = this->clk_gen_timestamp;
    }

    return next_timestamp == -1 ? -1 : next_timestamp - this->time.get_time();
}
