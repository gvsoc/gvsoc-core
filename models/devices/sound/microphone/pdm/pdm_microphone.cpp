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
#include <string.h>
#ifdef USE_SNDFILE
#include <sndfile.hh>
#endif
#include "pcm_pdm_conversion.hpp"
#include "io_audio.h"



class Microphone_pdm : public vp::Component
{
public:
    Microphone_pdm(vp::ComponentConf &conf);
    ~Microphone_pdm();

protected:
    static void i2s_sync(vp::Block *__this, int sck, int ws, int sd, bool full_duplex);
    void start();
    void reset(bool active);


    void pdm_get();
    int64_t exec();
    void set_pdm_data(int slot, int data);

    vp::Trace trace;
    vp::I2sMaster i2s_itf;

    void restart_input_audio_stream();

    std::string input_filepath;


    bool got_pcm_data = false;
    uint32_t cmpt = 0; // count number of pdm bits to send

    bool close;

    int id;
    int clk = 2;
    int ws_value = 2;
    int prev_sck;
    bool is_full_duplex;
    int data = (2 << 2) | 2;

    bool not_us = true;

    bool sync_ongoing;

    int slot;

    int generated_freq;


    void compute_i2s_freq(int64_t time);
    int64_t i2s_freq = 0;
    int64_t prev_i2s_freq = 0;
    int64_t prev_timestamp = -1;
    int64_t timestamp = -1;


    Rx_stream* audio_stream_in = nullptr;
    PcmToPdm* pcm2pdm = nullptr;
};



Microphone_pdm::~Microphone_pdm(){
    delete audio_stream_in;
    delete pcm2pdm;
}


Microphone_pdm::Microphone_pdm(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->i2s_itf.set_sync_meth(&Microphone_pdm::i2s_sync);
    this->new_master_port("i2s", &this->i2s_itf);
    this->input_filepath = this->get_js_config()->get_child_str("input_filepath");
    this->slot = this->get_js_config()->get_child_int("slot");
    this->generated_freq = this->get_js_config()->get_child_int("generated_freq");
    this->id = slot;

    // mic doesn't control clock and ws
    this->clk = 2;
    this->ws_value = 2;

    int data = (2 << 2) | 2;
}

void Microphone_pdm::restart_input_audio_stream(){
    delete audio_stream_in;
    delete pcm2pdm;

    audio_stream_in = new Signal_generator(generated_freq, this->i2s_freq / 64);
    pcm2pdm = new PcmToPdm(6, 0); // interpolation ratio 64 and interpolation type LINEAR
}


void Microphone_pdm::start()
{
    printf("start\n");
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
            if(_this->slot == 1 || _this->slot == 3){
                _this->not_us = false;
                _this->pdm_get();
            }
            else{
                _this->not_us = true;
                _this->pdm_get();
            }
        }

        else if (sck == 0)
        {
            if(_this->slot == 0 || _this->slot == 2){
                _this->not_us = false;
                _this->pdm_get();
            }
            else{
                _this->not_us = true;
                _this->pdm_get();
            }
        }
        

    }

    _this->sync_ongoing = false;
}


int64_t Microphone_pdm::exec()
{
    if (close)
    {
        this->set_pdm_data(this->id / 2, 2);
        this->close = false;
        return 17000;
    }
    else
{

    if(this->not_us){
        this->set_pdm_data(this->id / 2, data);
        return -1;
    }
        int data;
        
        // one pcm sample gives several pdm samples
        // pdm samples are then progressively sent 
        if(this->pcm2pdm)
        {
            if (cmpt >= this->pcm2pdm->output_size)
                cmpt = 0;
        }
        if (cmpt == 0)
        {
            if (this->audio_stream_in)
            {
                int32_t pcm_data;
                pcm_data = audio_stream_in->get_sample(); // here data is a int32
                // convert it to pdm
                got_pcm_data = true;
                this->pcm2pdm->convert(pcm_data);
            }
            else
            {
                got_pcm_data = false;
                if(this->i2s_freq != this->prev_i2s_freq)
                {
                    this->restart_input_audio_stream();
                }

            }
        }
        if (got_pcm_data)
        {
            data = (int)this->pcm2pdm->pdm_output[cmpt++];
            // pdm data must be 0/1
            data = (data <= 0) ? 0 : 1;
        }
        else
        {
            data = 0;
        }
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending pdm value (data: %d)\n", data);
        this->set_pdm_data(this->id / 2, data);
        return -1;
    }
}



void Microphone_pdm::set_pdm_data(int slot, int data)
{
    
    this->data = (this->data & ~(0x3 << (slot * 2))) | (data << (slot * 2));
    this->clk = 2;
    this->i2s_itf.sync(this->clk, this->ws_value, this->data, this->is_full_duplex);
}


void Microphone_pdm::pdm_get()
{
    this->close = true;
    this->time.enqueue_to_engine(this->time.get_time() + 4000);
}


void Microphone_pdm::compute_i2s_freq(int64_t time){
    prev_timestamp = timestamp;
    timestamp = time;
    this->prev_i2s_freq = this->i2s_freq;
    if(prev_timestamp != -1){
        this->i2s_freq = 1000000000 / (timestamp - prev_timestamp) * 1000;
    }
}