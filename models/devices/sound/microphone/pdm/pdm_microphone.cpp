/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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

#include <vp/vp.hpp>
#include <vp/itf/i2s.hpp>
#include <vp/itf/audio.hpp>
#include <string.h>
#include "pcm_pdm_conversion.hpp"
#include "io_audio.h"

class Slot;

class Microphone_pdm : public vp::Component
{
    friend class Slot;

public:
    Microphone_pdm(vp::ComponentConf &conf);
    ~Microphone_pdm();

protected:
    std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string req);
    static void set_rate(vp::Block *__this, int rate);
    static void push_sample(vp::Block *__this, double value);
    static void i2s_sync(vp::Block *__this, int sck, int ws, int sd, bool full_duplex);
    void start();
    void reset(bool active);

    void set_pdm_data(int slot, int data);

    void restart_pdm_converteur();
    void restart_input_audio_stream();
    void restart_debug_files();

    int32_t get_sample();
    int32_t saturate(int32_t sample);

    vp::Trace trace;
    vp::I2sMaster i2s_itf;
    vp::AudioSlave audio_in_itf;
    vp::AudioMaster audio_out_itf;

    bool is_full_duplex;
    bool sync_ongoing;
    int id;
    int clk = 2;
    int ws_value = 2;
    int prev_sck;
    int data = (2 << 2) | 2;
    int slot;
    int nb_slots;
    std::vector<Slot *> slots;

    // Inputs
    int generated_freq;
    float amplitude_sine = 1;
    std::string input_file_path;

    int32_t audio_in_itf_sample = 0;
    bool new_audio_in_itf_sample = false;

    bool got_pcm_data = false;
    uint32_t cmpt = 0; // count number of pdm bits to send

    uint32_t pcm_sampling_freq = -1;
    uint32_t audio_world_freq = -1;

    Rx_stream *audio_stream_in = nullptr;
    PcmToPdm *pcm2pdm = nullptr;

    // estimate i2s frequency
    void compute_i2s_freq(int64_t time);
    uint64_t i2s_freq = 0;
    uint64_t prev_i2s_freq[3] = {0};
    uint64_t mean_freq = 0;
    uint64_t prev_mean_freq = 0;
    int64_t prev_timestamp = -1;
    int64_t timestamp = -1;

    // debug
    uint8_t debug_files_enabled = 0;
    Tx_stream *debug_file_audio = nullptr;
    Tx_stream *debug_file_read = nullptr;
    Tx_stream *debug_file_mix = nullptr;
};

// Mic communicate with only one Slot but is connected to 4 Slot
class Slot : public vp::Block
{
    friend class Microphone_pdm;

public:
    Slot(Microphone_pdm *top, int id, bool active);
    void pdm_sync(int sd);
    void pdm_get();
    int64_t exec();

protected:
    Microphone_pdm *top;

private:
    int id;
    vp::Trace trace;
    bool active;
    bool close;
};

Microphone_pdm::~Microphone_pdm()
{
    delete audio_stream_in;
    delete pcm2pdm;
}

Microphone_pdm::Microphone_pdm(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->audio_in_itf.set_set_rate_meth(&Microphone_pdm::set_rate);
    this->audio_in_itf.set_push_sample_meth(&Microphone_pdm::push_sample);
    this->new_slave_port("audio_input", &this->audio_in_itf);

    this->audio_out_itf.set_set_rate_meth(&Microphone_pdm::set_rate);
    this->new_master_port("audio_output", &this->audio_out_itf);

    this->i2s_itf.set_sync_meth(&Microphone_pdm::i2s_sync);
    this->new_master_port("i2s", &this->i2s_itf);
    this->slot = this->get_js_config()->get_child_int("slot");
    this->generated_freq = this->get_js_config()->get_child_int("generated_freq");
    this->id = slot;

    this->pcm2pdm = new PcmToPdm(6, 0); // interpolation ratio 64 and interpolation type LINEAR

    // mic doesn't control clock and ws
    this->clk = 2;
    this->ws_value = 2;

    // data by default undefined
    int data = (2 << 2) | 2;
    this->nb_slots = 4;

    for (int i = 0; i < this->nb_slots; i++)
    {
        this->slots.push_back(new Slot(this, i, i == this->slot));
    }
}

uint32_t unsigned_log2(uint32_t number)
{
    if (number == 0)
    {
        return 0;
    }
    uint32_t pos = 0;

    while ((number & 1) == 0)
    {
        number >>= 1;
        ++pos;
    }
    return pos;
}

void Microphone_pdm::set_rate(vp::Block *__this, int rate)
{
    Microphone_pdm *_this = (Microphone_pdm *)__this;
    _this->trace.msg(vp::Trace::LEVEL_INFO, "Audio world force pcm freq to %d\n", rate);
    _this->audio_world_freq = rate;
}

void Microphone_pdm::push_sample(vp::Block *__this, double audio_value)
{
    Microphone_pdm *_this = (Microphone_pdm *)__this;
    // mic waits for 24 bits variable
    double val_for_mic = audio_value * (double)8388607;
    int32_t value = (int32_t) val_for_mic;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Audio world sample incomming %d\n", value);
    _this->audio_in_itf_sample = value;
    _this->new_audio_in_itf_sample = true;
}

int32_t Microphone_pdm::saturate(int32_t sample)
{
    if (sample > 8388607)
    {
        return 8388607;
    }
    if (sample < -8388607)
    {
        return -8388607;
    }
    return sample;
}


double to_audio_world(int32_t value)
{
    double converted_value = double(value);
    converted_value = converted_value / (double)8388607;
    return converted_value;
}


int32_t Microphone_pdm::get_sample()
{
    // depending on the configuration
    uint32_t mic_sample = this->audio_stream_in->get_sample();
    int32_t external_sample = (int32_t)mic_sample;
    int32_t used_sample;

    if (debug_file_read)
    {
        debug_file_read->push_sample(external_sample);
    }

    if (this->audio_out_itf.is_bound())
    {
        audio_out_itf.push_sample(to_audio_world(saturate(external_sample + audio_in_itf_sample)));
    }
    if (new_audio_in_itf_sample)
    {
        if (debug_file_audio)
        {
            debug_file_audio->push_sample(audio_in_itf_sample);
        }
        trace.msg(vp::Trace::LEVEL_TRACE, "Mixing audio world sample %d to mic sample %d\n", audio_in_itf_sample, external_sample);
        used_sample = saturate(external_sample + audio_in_itf_sample);
    }
    else
    {
        used_sample = saturate(external_sample);
    }

    if (debug_file_mix)
    {
        debug_file_mix->push_sample(used_sample);
    }

    return used_sample;
}

void Microphone_pdm::restart_pdm_converteur()
{
    if (this->i2s_freq > 1000 && pcm_sampling_freq > 0)
    {
        delete pcm2pdm;
        uint32_t interpolation_ratio_shift = unsigned_log2(this->i2s_freq / pcm_sampling_freq);
        if (pcm_sampling_freq * (1 << interpolation_ratio_shift) != this->i2s_freq)
        {
            if (!input_file_path.empty() && audio_world_freq == -1)
            {
                trace.msg(vp::Trace::LEVEL_WARNING, "File sampling rate (%d) can't me modulate to pdm rate %d, interpolation will be used", pcm_sampling_freq, this->i2s_freq);
                try
                {
                    audio_stream_in = new Rx_stream_wav(input_file_path, &pcm_sampling_freq, (uint32_t)this->i2s_freq / 8);
                }
                catch (const std::invalid_argument &e)
                {
                    this->trace.fatal("%s\n", e.what());
                }
                pcm_sampling_freq = this->i2s_freq / 8;
                interpolation_ratio_shift = unsigned_log2(this->i2s_freq / pcm_sampling_freq);
            }
            else if (audio_world_freq != -1)
            {
                this->trace.fatal("Audio world sampling freq (%d) can't me modulate to pdm freq %d\n", pcm_sampling_freq, this->i2s_freq);
            }
            else
            {
                this->trace.fatal("Unexpected error in pdm parameters computation, please check clock settings\n");
            }
        }
        this->trace.msg(vp::Trace::LEVEL_INFO, "Restarting PCM->PDM converter with interpolation ratio shift %d\n", interpolation_ratio_shift);
        pcm2pdm = new PcmToPdm(interpolation_ratio_shift, 0); // interpolation ratio, and interpolation type LINEAR
    }
}

void Microphone_pdm::restart_debug_files()
{
    if (debug_files_enabled)
    {
        this->trace.msg(vp::Trace::LEVEL_INFO, "Init debug files\n");
        delete debug_file_read;
        delete debug_file_audio;
        delete debug_file_mix;
        try
        {
            this->debug_file_read = new Tx_stream_wav(get_name() + "_external_input_debug.wav", 24, pcm_sampling_freq, pcm_sampling_freq);
            this->debug_file_audio = new Tx_stream_wav(get_name() + "_audio_connection_input_debug.wav", 24, pcm_sampling_freq, pcm_sampling_freq);
            this->debug_file_mix = new Tx_stream_wav(get_name() + "_used_data_debug.wav", 24, pcm_sampling_freq, pcm_sampling_freq);
        }
        catch (const std::invalid_argument &e)
        {
            this->trace.fatal("%s\n", e.what());
        }
    }
}

void Microphone_pdm::restart_input_audio_stream()
{
    delete audio_stream_in;
    // if not manually define, take a sampling freq adapted
    if (!input_file_path.empty())
    {

        this->trace.msg(vp::Trace::LEVEL_INFO, "Input stream is a file.\n");
        if (audio_world_freq != -1)
        {
            try
            {
                audio_stream_in = new Rx_stream_wav(input_file_path, &pcm_sampling_freq, audio_world_freq);
                pcm_sampling_freq = audio_world_freq;
            }
            catch (const std::invalid_argument &e)
            {
                this->trace.fatal("%s\n", e.what());
            }
        }
        else
        {
            try
            {
                audio_stream_in = new Rx_stream_wav(input_file_path, &pcm_sampling_freq);
            }
            catch (const std::invalid_argument &e)
            {
                this->trace.fatal("%s\n", e.what());
            }
        }
    }
    else
    {
        this->trace.msg(vp::Trace::LEVEL_INFO, "Input stream is a generated sine with frequency %d\n", generated_freq);
        pcm_sampling_freq = (audio_world_freq != -1) ? audio_world_freq : (this->i2s_freq / 4);
        audio_stream_in = new Signal_generator(generated_freq, amplitude_sine, pcm_sampling_freq);
    }
    restart_pdm_converteur();
    restart_debug_files();
}

void Microphone_pdm::start()
{
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Microphone_pdm(config);
}

void Microphone_pdm::reset(bool active)
{
}

void Microphone_pdm::i2s_sync(vp::Block *__this, int sck, int ws, int sdio, bool is_full_duplex)
{
    Microphone_pdm *_this = (Microphone_pdm *)__this;

    if (_this->sync_ongoing)
        return;

    _this->sync_ongoing = true;

    _this->clk = sck;
    _this->is_full_duplex = is_full_duplex;

    if (sck != _this->prev_sck)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "I2S edge (sck: %d) \n", sck);

        _this->prev_sck = sck;

        if (sck == 1)
        {
            _this->compute_i2s_freq(_this->time.get_time());
            _this->slots[1]->pdm_get();
            _this->slots[3]->pdm_get();
        }
        else if (sck == 0)
        {
            _this->slots[0]->pdm_get();
            _this->slots[2]->pdm_get();
        }
    }
    _this->sync_ongoing = false;
}

Slot::Slot(Microphone_pdm *top, int id, bool active) : vp::Block(top, "slot_" + std::to_string(id)), top(top), id(id), active(active)
{
    top->traces.new_trace("slot" + std::to_string(id), &trace, vp::DEBUG);
}

int64_t Slot::exec()
{
    if (close)
    {
        this->top->set_pdm_data(this->id / 2, 2);
        this->close = false;
        return 17000;
    }
    else
    {
        if (!this->active)
        {
            this->top->set_pdm_data(this->id / 2, 2);
            return -1;
        }
        int data_to_send;

        // one pcm sample gives several pdm samples
        // pdm samples are then progressively sent
        if (this->top->pcm2pdm)
        {
            if (this->top->cmpt >= this->top->pcm2pdm->output_size)
            {
                this->top->cmpt = 0;
            }
            if (this->top->cmpt == 0)
            {
                int32_t pcm_data;
                if (this->top->audio_stream_in)
                {
                    pcm_data = this->top->get_sample(); // here data is a int32
                }
                else
                {
                    pcm_data = 0; // default value
                }
                this->trace.msg(vp::Trace::LEVEL_DEBUG, "Next PCM sample : %d\n", pcm_data);
                // convert it to pdm
                this->top->pcm2pdm->convert(pcm_data);
            }
            data_to_send = (int)this->top->pcm2pdm->pdm_output[this->top->cmpt++];
            // pdm data must be 0/1
            data_to_send = (data_to_send <= 0) ? 0 : 1;
        }
        else
        {
            data_to_send = 0;
        }
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending pdm value (data: %d)\n", data_to_send);
        this->top->set_pdm_data(this->top->id / 2, data_to_send);
        return -1;
    }
}

void Microphone_pdm::set_pdm_data(int slot, int dat)
{
    this->data = (this->data & ~(0x3 << (slot * 2))) | (dat << (slot * 2));
    this->clk = 2;
    this->i2s_itf.sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
}

void Slot::pdm_get()
{
    this->close = true;
    this->time.enqueue_to_engine(this->time.get_time() + 4000);
}

uint64_t compute_mean_freq(uint64_t array[3])
{
    return (array[0] + array[1] + array[2]) / 3;
}

void Microphone_pdm::compute_i2s_freq(int64_t time)
{
    prev_timestamp = timestamp;
    timestamp = time;
    uint64_t int_i2s_freq = 0;
    if (prev_timestamp != -1)
    {
        int_i2s_freq = 1000000000 / (timestamp - prev_timestamp) * 1000;
    }
    prev_i2s_freq[0] = prev_i2s_freq[1];
    prev_i2s_freq[1] = prev_i2s_freq[2];
    prev_i2s_freq[2] = int_i2s_freq;
    if (compute_mean_freq(prev_i2s_freq) == prev_i2s_freq[2])
    {
        prev_mean_freq = mean_freq;
        mean_freq = prev_i2s_freq[2];
        if (mean_freq != prev_mean_freq)
        {
            this->i2s_freq = mean_freq;
            this->trace.msg(vp::Trace::LEVEL_WARNING, "Change detected in SAI frequency, new frequency is %d kHz, previous one was %d kHz\n", mean_freq, prev_mean_freq);
            // do all you need if i2s freq change
            this->restart_input_audio_stream();
        }
    }
    this->i2s_freq = mean_freq;
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "i2s frequency : %d kHz\n", this->i2s_freq);
}

std::string Microphone_pdm::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{

    int error = 0;
    if (args[0] == "set_input_file")
    {
        input_file_path = args[1];
    }
    else if (args[0] == "set_generated_sine")
    {
        generated_freq = atoi(args[2].c_str());
        amplitude_sine = atof(args[4].c_str());
    }
    else if (args[0] == "debug")
    {
        debug_files_enabled = atoi(args[1].c_str());
    }
    if (error)
    {
        return "err=" + std::to_string(error);
    }
    return "err=1";
}
