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
#include <vp/itf/audio.hpp>
#include "io_audio.h"
#include <limits>
#include <fstream>
#include <vector>



class DelayLine {
public:
    DelayLine(int size) : size_(size), buffer_(size, 0) {}

    void push(double sample) {
        for (int i = size_ - 1; i > 0; --i) {
            buffer_[i] = buffer_[i - 1];
        }
        buffer_[0] = sample;
    }

    double operator[](int index) const {
        return buffer_[index];
    }

private:
    int size_;
    std::vector<double> buffer_;
};


class TransferFunction : public vp::Component
{
public:
    TransferFunction(vp::ComponentConf &conf);

    void start() override;

private:
    static void set_rate_out(vp::Block *__this, int rate);
    static void set_rate_in(vp::Block *__this, int rate);
    static void push_sample(vp::Block *__this, double value);

    vp::Trace trace;
    vp::AudioMaster audio_out_itf;
    vp::AudioSlave audio_in_itf;


    std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string req);

    Tx_stream* input_debug_stream = nullptr;
    Tx_stream* output_debug_stream = nullptr;

    uint8_t debug_files_enabled = 0;

    int sample_rate;
    int64_t period;

    DelayLine* delay_line;

    std::string debug_file_path = "";

    double apply(double sample);

    int64_t next_sample_timestamp = -1;

    double current_sample = 0;

    std::vector<double> filter = {0.5, 0.5};

};

double TransferFunction::apply(double sample) {
    double output = 0;
    int filterLength = filter.size();

    delay_line->push(sample);

    // apply convolution
    for (int i = 0; i < filterLength; ++i) {
        output += filter[i] * (*delay_line)[i];
    }

    return output;
}


TransferFunction::TransferFunction(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->audio_in_itf.set_set_rate_meth(&TransferFunction::set_rate_in);
    this->audio_in_itf.set_push_sample_meth(&TransferFunction::push_sample);
    this->new_slave_port("audio_in", &this->audio_in_itf);

    this->audio_out_itf.set_set_rate_meth(&TransferFunction::set_rate_out);
    this->new_master_port("audio_out", &this->audio_out_itf);

    this->sample_rate = this->get_js_config()->get_child_int("sample_rate");
    this->period = 1000000000000ULL / this->sample_rate;

    delay_line = new DelayLine(filter.size());

}

void TransferFunction::start()
{
    this->audio_in_itf.set_rate(this->sample_rate);
    this->audio_out_itf.set_rate(this->sample_rate);
}

void TransferFunction::set_rate_in(vp::Block *__this, int rate)
{
}

void TransferFunction::push_sample(vp::Block *__this, double value)
{
    TransferFunction *_this = (TransferFunction *)__this;
    if( _this->input_debug_stream){
        int32_t val_for_file = (int32_t)(value * (double)8388607);
        _this->input_debug_stream->push_sample(val_for_file);
    }
    // In order to take into account all input interfaces, especially when samples
    // arrive slightly shifted, we keep track of the timestamp of the next sample.
    // We cumulate all samples with timestamps under the next sample one, and we send them all as
    // soon as the next sample timestamp is detected.
    if (_this->time.get_time() >= _this->next_sample_timestamp)
    {
        if(_this->next_sample_timestamp == -1)
        {
            _this->next_sample_timestamp = _this->time.get_time() + _this->period;
        }
        else
        {
            _this->next_sample_timestamp += _this->period;
        }
        if (_this->next_sample_timestamp != -1)
        {   
            double output_sample = _this->apply(_this->current_sample);
            if( _this->output_debug_stream){
                double val_for_file = (int32_t)(output_sample * (double)(8388607));
                _this->output_debug_stream->push_sample(val_for_file);
            }
            _this->audio_out_itf.push_sample(output_sample);

        }

        _this->current_sample = 0;
    }

    _this->current_sample += value;


}


void TransferFunction::set_rate_out(vp::Block *__this, int rate)
{
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new TransferFunction(config);
}



std::string TransferFunction::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{
    int error = 0;
    // Printing the vector of strings
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "rate")
        {
            this->sample_rate = atoi(args[i+1].c_str());
            this->audio_in_itf.set_rate(this->sample_rate);
            this->audio_out_itf.set_rate(this->sample_rate);
            this->period = 1000000000000ULL / this->sample_rate;
        }
        else if (args[i] == "filter")
        {
            filter.clear();
            std::ifstream file_filt(args[i+1]);
            if (file_filt)
            {
                double val;
                while (file_filt >> val) {
                    filter.push_back(val);
                }
                file_filt.close();
            }
            delete delay_line;
            delay_line = new DelayLine(filter.size());
            break;
        }
        else if(args[i] == "debug_files"){
            debug_files_enabled = atoi(args[i+1].c_str());
        }
    }

    delete input_debug_stream;
    delete output_debug_stream;
    if(debug_files_enabled)
    {
        try
        {
            input_debug_stream = new Tx_stream_wav(get_name() + "_input_debug.wav", 24, this->sample_rate, this->sample_rate);
            output_debug_stream = new Tx_stream_wav(get_name() + "_output_debug.wav", 24, this->sample_rate, this->sample_rate);
        }catch (const std::invalid_argument& e)
        {
            this->trace.fatal("%s\n", e.what());
        }
    }

    if(error){
        return "err=" + std::to_string(error);
    }
    return "err=1";
}

