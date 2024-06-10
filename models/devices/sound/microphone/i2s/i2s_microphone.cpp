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


#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/i2s.hpp>
#include <vp/itf/clock.hpp>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "io_audio.h"


class Microphone : public vp::Component
{

public:
    Microphone(vp::ComponentConf &conf);
    ~Microphone();

protected:
    static void sync(vp::Block *__this, int sck, int ws, int sd, bool is_full_duplex);
    void start_sample();
    int pop_data();
    int get_data();
    void start();

    vp::I2sMaster i2s_itf;
    int channel_ws;         // Word-Select of this microphone. It will send data whe the ws corresponds to this one
    int prev_ws;            // Value of the previous ws. Updated at each clock cycle to detect the beginning of the frame
    int ws_delay;           // Tells after how many cycles the first data is sampled when the ws of this channel becomes active
    int current_ws_delay = -1;   // Remaining cycles before the first data will be sampled. It is initialized with ws_delay when the frame starts
    int current_value;      // Value of the current sample being sent
    int pending_bits;       // Number of bits of the current sample which remain to be sent
    int width;              // Number of bits of the samples
    int frequency;          // Sampling frequency
    bool is_active;         // Set to true when the word-select of this microphone is detected. The microphone is sending samples when this is true;
    bool enabled;
    int prev_sck;
    int generated_freq = 0;
    int input_stream_width = 24;

    Rx_stream *instream = nullptr;
    std::string input_file_path;

    void init_input_stream();


    int clk;
    int data;
    int sdio;
    int ws;
    int ws_count;
    int ws_value;
    bool frame_active;


    bool rx_started;
    int rx_pending_bits;
    int rx_current_value;
    uint32_t rx_pending_value;


    void start_frame();

    vp::Trace trace;

};


Microphone::~Microphone()
{
    // delete instream;
}

int32_t normalize_int(int32_t value, int actual_width, int wanted_width)
{
    double new_val = 0;
    double current_val = (double) value;
    double devider = (actual_width == 32) ? (double)(~(1 << (actual_width - 1))) : (double)(1 << (actual_width - 1));
    double mult = (wanted_width == 32) ? (double)(~(1 << (wanted_width - 1))) : (double)(1 << (wanted_width - 1));

    new_val = current_val /  devider * mult;
    return (int32_t) new_val;
}

void Microphone::init_input_stream()
{
    // delete instream;
    if(!input_file_path.empty())
    {
        uint32_t file_freq;
        try
        {      
            instream = new Rx_stream_wav(this->input_file_path, &file_freq, this->frequency);
        }
        catch (const std::invalid_argument &e)
        {
            this->trace.fatal("%s\n", e.what());
        }
    }
    else
    {
        instream = new Signal_generator(this->generated_freq, 1, this->frequency);
    }
}

void Microphone::start()
{
}

Microphone::Microphone(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->i2s_itf.set_sync_meth(&Microphone::sync);
    this->new_master_port("i2s", &this->i2s_itf);


    this->channel_ws = this->get_js_config()->get_child_str("channel") != "left";
    this->prev_ws = 0;
    this->ws_delay = this->get_js_config()->get_int("ws-delay");
    this->width = this->get_js_config()->get_int("width");
    this->frequency = this->get_js_config()->get_int("frequency");
    this->generated_freq = this->get_js_config()->get_child_int("generated_freq");

    this->input_file_path = this->get_js_config()->get_child_str("input_filepath");
    this->is_active = false;
    
    this->current_ws_delay = 0;
    this->ws_delay = 1;
    this->pending_bits = -1;
    this->prev_sck = 0;

}


void Microphone::start_sample()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Starting sample\n");
    this->pending_bits = this->width;
    this->current_value = this->get_data();
}





void Microphone::start_frame()
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Start frame\n");

            this->rx_pending_bits = this->width;
        if (this->instream)
        {
            int32_t got_value = (int32_t) this->instream->get_sample();
            this->rx_pending_value = (uint32_t) normalize_int(got_value, this->input_stream_width, this->width);
        }else{
             this->rx_pending_value = 0xffff;
             init_input_stream();
        }
        if (this->rx_pending_bits > 0)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Starting RX sample (sample: %i)\n", (int32_t) this->rx_pending_value);

            int msb_first = 1;


            bool changed = false;
            if (!msb_first)
            {
                uint32_t value = 0;
                for (int i=0; i< this->width; i++)
                {
                    value = (value << 1) | (this->rx_pending_value & 1);
                    this->rx_pending_value >>= 1;
                }
                this->rx_pending_value = value;
            }

        }

}

int Microphone::get_data()
{
    int data;

        if (this->rx_pending_bits > 0)
        {

        this->trace.msg(vp::Trace::LEVEL_TRACE, "pending value (data: %d)\n", this->rx_pending_value);
            data = (this->rx_pending_value >> (this->width - 1)) & 1;
            this->rx_pending_bits--;
            this->rx_pending_value <<= 1;
        }
        else
        {
            data = 0;
        }

        this->trace.msg(vp::Trace::LEVEL_TRACE, "Getting data (data: %d)\n", data);

        return data;
}




void Microphone::sync(vp::Block *__this, int sck, int ws, int sdio, bool is_full_duplex)
{
    Microphone *_this = (Microphone *)__this;

    _this->sdio = sdio;

    _this->ws = ws;

    int sd = sdio >> 2; // i2s => full duplex

    if (sck != _this->prev_sck)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "I2S edge (sck: %d, ws: %d, sdo: %d)\n", sck, ws, sd);

        _this->prev_sck = sck;

        if (!sck)
        {
            // _this->compute_i2s_clk_freq(_this->time.get_time());
            // If there is a delay, decrease it
            if (_this->current_ws_delay > 0)
            {
                _this->current_ws_delay--;
                if (_this->current_ws_delay == 0)
                {

                    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Activating channel !!!!!!!\n");
                    // And reset the sample
                    _this->is_active = true;
                    _this->pending_bits = _this->width;
                    _this->start_frame();
                }
            }

            if (_this->is_active)
            {
                int data = _this->get_data();
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Bit to send (value: %d)\n", data);
                data = data | (2 << 2);
                // _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending bit\n");
                _this->i2s_itf.sync(2, 2, data, 2);
                _this->pending_bits--;

                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Pending bit %d\n", _this->pending_bits);

                if (_this->pending_bits == 0)
                {
                    // _this->is_active = false;
                }

            }

            // The channel is the one of this dac
            if (_this->prev_ws != ws && ws == _this->channel_ws)
            {
                // If the WS just changed, apply the delay before starting sending
                _this->current_ws_delay = _this->ws_delay;
                if (_this->current_ws_delay == 0)
                {
                    _this->is_active = true;
                }
            }
            _this->prev_ws = ws;
        }

    }

    _this->prev_sck = sck;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Microphone(config);
}
