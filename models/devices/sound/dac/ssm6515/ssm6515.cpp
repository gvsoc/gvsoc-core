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
#include <vp/itf/i2c.hpp>
#include <vp/itf/i2s.hpp>
#include <devices/sound/dac/ssm6515/headers_regfields.h>
#include <devices/sound/dac/ssm6515/headers_gvsoc.h>
#include <string.h>
#ifdef USE_SNDFILE
#include <sndfile.hh>
#endif
#include "pcm_pdm_conversion.hpp"

typedef enum
{
  I2C_STATE_WAIT_START,
  I2C_STATE_WAIT_ADDRESS,
  I2C_STATE_GET_DATA,
  I2C_STATE_SAMPLE_DATA,
  I2C_STATE_ACK,
  I2C_STATE_READ_ACK
} I2c_state_e;


typedef enum
{
    SSM6515_MODE_PCM = 0, //!< Normal PCM/SAI operation.
    SSM6515_MODE_PDM = 1, //!< PDM used for input.
}ssm6515_pdm_mode_e;

/**
 * \brief PDM Sample Rate Selection.
 */
typedef enum
{
    SSM6515_PDM_FS_5_6448_TO_6_144   = 0, //!< 5.6448 MHz to 6.144 MHz clock in PDM mode.
    SSM6515_PDM_FS_2_8224_TO_3_072   = 1, //!< 2.8224 MHz to 3.072 MHz clock in PDM mode.
    SSM6515_PDM_FS_11_2896_TO_12_288 = 2, //!< 11.2896 MHz to 12.288 MHz clock in PDM mode.
}ssm6515_pdm_fs_e;


/**
 * \brief DAC Path Sample Rate Selection.
 */
typedef enum
{
    SSM6515_DAC_FS_8KHZ            = 0,
    SSM6515_DAC_FS_12KHZ           = 1,
    SSM6515_DAC_FS_16KHZ           = 2,
    SSM6515_DAC_FS_24KHZ           = 3,
    SSM6515_DAC_FS_32KHZ           = 4,
    SSM6515_DAC_FS_44_1_TO_48KHZ   = 5,
    SSM6515_DAC_FS_88_2_TO_96KHZ   = 6,
    SSM6515_DAC_FS_176_4_TO_192KHZ = 7,
    SSM6515_DAC_FS_384KHZ          = 8,
    SSM6515_DAC_FS_768KHZ          = 9,
}ssm6515_dac_fs_e;




/**
 * \brief Serial Port–Selects LRCLK Polarity.
 */
typedef enum
{
    SSM6515_LRCLK_POL_NORMAL   = 0,
    SSM6515_LRCLK_POL_INVERTED = 1,
}ssm6515_lrclk_pol_e;

/**
 * \brief Serial Port–Selects BCLK Polarity.
 */
typedef enum
{
    SSM6515_BCLK_POL_RISING   = 0, //!< Capture on rising edge.
    SSM6515_BCLK_POL_FALLING  = 1, //!< Capture on falling edge.
}ssm6515_bclk_pol_e;

/**
 * \brief Serial Port–Selects TDM Slot Width.
 */
typedef enum
{
    SSM6515_TDM_32BCLK_PER_SLOT  = 0,
    SSM6515_TDM_16BCLK_PER_SLOT  = 1,
    SSM6515_TDM_24BCLK_PER_SLOT  = 2,
}ssm6515_tdm_slot_width_e;

/**
 * \brief Serial Port–Selects Data Format.
 */
typedef enum
{
    SSM6515_DATA_FORMAT_I2S      = 0,
    SSM6515_DATA_FORMAT_LJ       = 1,
    SSM6515_DATA_FORMAT_DELAY_8  = 2,
    SSM6515_DATA_FORMAT_DELAY_12 = 3,
    SSM6515_DATA_FORMAT_DELAY_16 = 4,
}ssm6515_data_format_e;

/**
 * \brief Serial Port–Selects Stereo or TDM Mode.
 */
typedef enum
{
    SSM6515_SAI_MODE_STEREO = 0,
    SSM6515_SAI_MODE_TDM    = 1,
}ssm6515_sai_mode_e;




class Ssm6515;


class Outfile_wav {

public:
    Outfile_wav(Ssm6515 *top, std::string file, int width, int wav_sampling_freq);
    ~Outfile_wav();
    void push_sample(uint32_t sample);

private:
#ifdef USE_SNDFILE
    SNDFILE *sndfile;
    SF_INFO sfinfo;
#endif
    int period;
    int width;
    uint32_t pending_channels;
    int32_t *items;
    Ssm6515 *top;
};







class Ssm6515 : public vp::Component
{
    friend class Outfile_wav;
public:
    Ssm6515(vp::ComponentConf &conf);
    ~Ssm6515();

protected:
    static void i2c_sync(vp::Block *__this, int scl, int sda);
    static void i2s_sync(vp::Block *__this, int sck, int ws, int sd, bool full_duplex); // in PDM mode only for the moment
    void i2c_start(unsigned int address, bool is_read);
    void i2c_handle_byte(uint8_t byte);
    void i2c_stop();
    void i2c_get_data();
    void i2c_send_byte(uint8_t byte);

    vp::TimeEvent starting_event;

    void reset(bool);
    void handle_reg_write(uint8_t address, uint8_t value);
    uint8_t handle_reg_read(uint8_t address);

    void handle_status_clr_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_pwr_ctrl_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_spt_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_dac_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_reset_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);


    int get_ws_delay();
    int get_slot_width();


    static void start_dac(vp::Block *__this, vp::TimeEvent *event);
    static void stop_dac(vp::Block *__this, vp::TimeEvent *event);


    void start();
    void reset();

    // i2c
    vp_regmap_i2c_regmap regmap;
    vp::Trace trace;
    vp::I2cMaster i2c_itf;
    vp::I2sMaster i2s_itf;
    vp::ClockMaster clock_cfg;

    unsigned int device_address;

    bool i2c_being_addressed;
    unsigned int i2c_address;
    uint8_t i2c_pending_data;
    bool i2c_is_read;
    I2c_state_e i2c_state;
    int i2c_pending_bits;
    int i2c_prev_sda;
    int i2c_prev_scl;
    unsigned int i2c_pending_send_byte;
    uint8_t reg_address;
    bool waiting_reg_address;
    void reset_output_file();
    void reset_converteur();

    bool reset_pdm2pcm_converter = TRUE;
    bool reset_output_stream = TRUE;




    void pdm_sync(int sd);
    PdmToPcm* pdm2pcm = nullptr;

    Outfile_wav* out_stream = nullptr;
    uint32_t out_bitrate = 32;
    uint32_t pcm_freq = 48000;

    // pdm to pcm conversion
    uint32_t cic_m = 2;
    uint32_t cic_n = 8;
    uint32_t cic_r = 64;
    uint32_t cic_shift = 7;

    // i2s
    uint8_t ws_delay = 1;
    uint8_t current_ws_delay = 0;

    bool is_active = false;
    

    void push_data(int data);


    bool dac_started = FALSE;

    void start_sample();
    void restart_output_stream();

    int32_t current_value;
    std::string output_filepath;



    int64_t i2s_freq = 0;
    int64_t prev_i2s_freq = 0;
    int64_t prev_timestamp = -1;
    int64_t timestamp = -1;

    void compute_i2s_freq(int64_t time);

    void compute_pdm2pcm_params();
    uint32_t get_input_pdm_freq();
    uint32_t get_output_pcm_freq();
    bool sync_ongoing = false;
    bool is_pdm = true;
    int prev_sck;
    int prev_ws;
    int ws;
    int sdio;
    bool is_full_duplex = false;

    int pending_bits = 32;
    int width = 32;


};


Ssm6515::Ssm6515(vp::ComponentConf &config)
    : vp::Component(config), regmap(*this, "regmap"), starting_event(this, Ssm6515::start_dac)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->i2c_itf.set_sync_meth(&Ssm6515::i2c_sync);
    this->new_master_port("i2c", &this->i2c_itf);

    this->i2s_itf.set_sync_meth(&Ssm6515::i2s_sync);
    this->new_master_port("i2s", &this->i2s_itf);

    this->i2c_state = I2C_STATE_WAIT_START;
    this->i2c_prev_sda = 1;
    this->i2c_prev_scl = 1;
    this->i2c_being_addressed = false;
    this->device_address = this->get_js_config()->get_child_int("i2c_address");
    this->output_filepath = this->get_js_config()->get_child_str("output_filepath");
    this->waiting_reg_address = true;

    this->regmap.build(this, &this->trace);

    this->regmap.status_clr.register_callback(std::bind(&Ssm6515::handle_status_clr_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.pwr_ctrl.register_callback(std::bind(&Ssm6515::handle_pwr_ctrl_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.reset_reg.register_callback(std::bind(&Ssm6515::handle_reset_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.dac_ctrl1.register_callback(std::bind(&Ssm6515::handle_dac_ctrl1_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.spt_ctrl1.register_callback(std::bind(&Ssm6515::handle_spt_ctrl1_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    
    printf("construit\n");

}


void Ssm6515::start()
{
    this->i2c_itf.sync(1, 1);
    printf("start\n");
}


void Ssm6515::i2c_sync(vp::Block *__this, int scl, int sda)
{
    Ssm6515 *_this = (Ssm6515 *)__this;

    _this->trace.msg(vp::Trace::LEVEL_TRACE, "I2C sync (scl: %d, sda: %d)\n", scl, sda);

    int sdo = 1;

    if (scl == 1 && _this->i2c_prev_sda != sda)
    {
        if (_this->i2c_prev_sda == 1)
        {
            _this->trace.msg(vp::Trace::LEVEL_TRACE, "Detected start\n");

            _this->i2c_state = I2C_STATE_WAIT_ADDRESS;
            _this->i2c_address = 0;
            _this->i2c_pending_bits = 8;
        }
        else
        {
            _this->i2c_state = I2C_STATE_WAIT_START;
            _this->i2c_stop();
        }
        goto end;
    }

    if (!_this->i2c_prev_scl && scl)
    {
        switch (_this->i2c_state)
        {
            case I2C_STATE_WAIT_START:
            {
                sdo = 1;
                break;
            }

            case I2C_STATE_WAIT_ADDRESS:
            {
                if (_this->i2c_pending_bits > 1)
                {
                    _this->i2c_address = (_this->i2c_address << 1) | sda;
                    _this->trace.msg(vp::Trace::LEVEL_TRACE, "Received address bit (bit: %d, address: 0x%x, pending_bits: %d)\n", sda, _this->i2c_address, _this->i2c_pending_bits);
                }
                else
                {
                    _this->i2c_is_read = sda;
                }
                _this->i2c_pending_bits--;
                if (_this->i2c_pending_bits == 0)
                {
                    if (_this->i2c_address == _this->device_address)
                    {
                        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Address has matched\n");
                        _this->i2c_start(_this->i2c_address, _this->i2c_is_read);
                        _this->i2c_state = I2C_STATE_ACK;
                    }
                    _this->i2c_pending_bits = 8;
                }
                break;
            }

            case I2C_STATE_SAMPLE_DATA:
            {
                _this->i2c_pending_data = (_this->i2c_pending_data << 1) | sda;
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sampling data (bit: %d, pending_value: 0x%x, pending_bits: %d)\n", sda, _this->i2c_pending_data, _this->i2c_pending_bits);
                _this->i2c_pending_bits--;
                if (_this->i2c_pending_bits == 0)
                {
                    _this->i2c_pending_bits = 8;
                    _this->i2c_handle_byte(_this->i2c_pending_data);
                    _this->i2c_state = I2C_STATE_ACK;
                }
                break;
            }

            case I2C_STATE_ACK: {
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Ack (being_addressed: %d)\n", _this->i2c_being_addressed);
                if (_this->i2c_being_addressed)
                {
                    if (_this->i2c_is_read)
                    {
                        _this->i2c_state = I2C_STATE_GET_DATA;
                        _this->i2c_pending_bits = 8;
                        _this->i2c_get_data();
                    }
                    else
                    {
                        _this->i2c_state = I2C_STATE_SAMPLE_DATA;
                    }
                }
                else
                {
                    _this->i2c_state = I2C_STATE_WAIT_START;
                }

                break;
            }

            case I2C_STATE_READ_ACK: {
                _this->i2c_state = I2C_STATE_WAIT_START;
                break;
            }
        }
    }

    if (_this->i2c_prev_scl && !scl)
    {
        switch (_this->i2c_state)
        {
            case I2C_STATE_ACK:
            {
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Ack (being_addressed: %d)\n", _this->i2c_being_addressed);
                sdo = !_this->i2c_being_addressed;
                break;
            }

            case I2C_STATE_READ_ACK:
            {
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Read ack\n");
                sdo = 0;
                break;
            }

            case I2C_STATE_GET_DATA:
            {
                sdo = (_this->i2c_pending_send_byte >> 7) & 1;
                _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sending bit (bit: %d, pending_value: 0x%x, pending_bits: %d)\n", sdo, _this->i2c_pending_send_byte, _this->i2c_pending_bits);
                _this->i2c_pending_send_byte <<= 1;
                _this->i2c_pending_bits--;
                if (_this->i2c_pending_bits == 0)
                {
                    _this->i2c_state = I2C_STATE_READ_ACK;
                }
                break;
            }
        }
    }

end:
    if (_this->i2c_prev_scl && !scl)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Sync sda (value: %d)\n", sdo);
        _this->i2c_itf.sync(1, sdo);
    }
    _this->i2c_prev_sda = sda;
    _this->i2c_prev_scl = scl;
}

void Ssm6515::i2c_start(unsigned int address, bool is_read)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Received header (address: 0x%x, is_read: %d)\n", address, is_read);

    this->i2c_being_addressed = address == this->device_address;
    if (is_read)
    {
        this->i2c_send_byte(this->handle_reg_read(this->reg_address));
        this->waiting_reg_address = true;
    }
}

void Ssm6515::i2c_handle_byte(uint8_t byte)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Handle byte (value: 0x%x)\n", byte);

    if (this->waiting_reg_address)
    {
        this->reg_address = byte;
        this->waiting_reg_address = false;
    }
    else
    {
        this->handle_reg_write(this->reg_address, byte);
        this->waiting_reg_address = true;
    }
}

void Ssm6515::i2c_stop()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Received stop bit\n");

}

void Ssm6515::i2c_get_data()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Getting data\n");
}

void Ssm6515::i2c_send_byte(uint8_t byte)
{
  this->i2c_pending_send_byte = byte;
}


void Ssm6515::handle_reg_write(uint8_t address, uint8_t value)
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Register write (address: 0x%x, value: 0x%x)\n", address, value);

    // write rgister value
    this->regmap.access(address, 1, &value, true);

}


uint8_t Ssm6515::handle_reg_read(uint8_t address)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Register read (address: 0x%x)\n", address);

    uint8_t value = 0xFF;

    // read register and send value
    this->regmap.access(address, 1, &value, false);

    return value;
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Ssm6515(config);
}



// callback called when accessing status_clr register
void Ssm6515::handle_status_clr_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.status_clr.update(reg_offset, size, value, is_write);

    if (this->regmap.status_clr.get()){
        // reset all
        this->regmap.status.reset(true);
        
    }
}

// callback called when accessing status_clr register
void Ssm6515::handle_pwr_ctrl_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.pwr_ctrl.update(reg_offset, size, value, is_write);

    if (this->regmap.pwr_ctrl.spwdn_get() == 0){
        // start dac
        // 26 ms according to documentation
        this->starting_event.enqueue(26000000000);

        
    }
}

void Ssm6515::handle_dac_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.dac_ctrl1.update(reg_offset, size, value, is_write);
    if(this->get_output_pcm_freq() != this->pcm_freq){
        this->pcm_freq = this->get_output_pcm_freq();
        this->reset_pdm2pcm_converter = TRUE;
        this->reset_output_stream = TRUE;
    }

}

void Ssm6515::handle_spt_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.spt_ctrl1.update(reg_offset, size, value, is_write);
    if(this->get_slot_width()  != this->width){
        this->width = this->get_slot_width();
        this->reset_pdm2pcm_converter = TRUE;
        this->reset_output_stream = TRUE;
    }

    this->ws_delay = this->get_ws_delay();
}


void Ssm6515::reset(bool active){
    this->regmap.reset(active);
}


// callback called when accessing status_clr register
void Ssm6515::handle_reset_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.reset_reg.update(reg_offset, size, value, is_write);

    if (this->regmap.reset_reg.soft_full_reset_get() == 1){
        // reset all registers
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Reseting registers\n");
        this->regmap.reset(TRUE);
    }
}


// --------------------------------------
// ------------- i2s part ---------------
// --------------------------------------




void Ssm6515::i2s_sync(vp::Block *__this, int sck, int ws, int sdio, bool is_full_duplex)
{
    Ssm6515 *_this = (Ssm6515 *)__this;

    if (_this->sync_ongoing)
        return;
    
    if(_this->regmap.spt_ctrl1.spt_bclk_pol_get()){
        printf("chqnge pol\n");
        sck = sck ^ 1;
    }
    if(_this->regmap.spt_ctrl1.spt_lrclk_pol_get()){
        printf("chqnge pol 2\n");
        sck = sck ^ 1;
    }

    _this->sync_ongoing = true;

    _this->sdio = sdio;

    _this->ws = ws;

    int sd = is_full_duplex ? sdio >> 2 : sdio & 0x3;
    //sd = (sdio >> 2);

    if (sck != _this->prev_sck)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "I2S edge (sck: %d, ws: %d, sdo: %d)\n", sck, ws, sd);
        
        _this->prev_sck = sck;

        

        if (_this->regmap.pdm_ctrl.pdm_mode_get())
        {
            if (sck == 1)
            {
                _this->compute_i2s_freq( _this->time.get_time());
                _this->pdm_sync(sdio & 0x3);
            }
        }
        else if (_this->regmap.spt_ctrl1.spt_sai_mode_get() == 0){

            if (sck)
            {
                _this->compute_i2s_freq( _this->time.get_time());
                // If there is a delay, decrease it
                if (_this->current_ws_delay > 0)
                {
                    _this->current_ws_delay--;
                    if (_this->current_ws_delay == 0)
                    {
                        // And reset the sample
                        _this->is_active = true;
                        _this->start_sample();
                    }
                }

                if (_this->is_active)
                {
                    _this->push_data(sd);
                }

                // The channel is the one of this microphone
                if (_this->prev_ws != ws && ws == _this->regmap.spt_ctrl2.spt_slot_sel_get())
                {
                    if (!_this->is_active)
                    {
                        _this->trace.msg(vp::Trace::LEVEL_TRACE, "Activating channel\n");
                    }

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
        else{
            _this->trace.msg(vp::Trace::LEVEL_WARNING, "TDM mode not supported by model\n");
        }

    }

    _this->sync_ongoing = false;
}
























void Ssm6515::pdm_sync(int sd)
{
    // sd : 0 / 1
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Pdm data : %d)\n", sd);
    /* Case where a conversion is required to send pcm data instread of pdm*/
    if (this->pdm2pcm && !this->reset_pdm2pcm_converter)
    {
        int input_bit = (sd > 0) ? 1 : -1;
        // producing on pcm sample require several pdm samples
        // convert method says when pcm data is ready to be sent
        if (this->pdm2pcm->convert(input_bit))
        {
            int out = this->pdm2pcm->pcm_output;
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Pcm data : %d)\n", out);
            if(this->out_stream && !this->reset_output_stream){
                this->out_stream->push_sample(out);
            }
            else{
                if(this->i2s_freq != this->prev_i2s_freq){
                reset_output_file();
                }
            }
            

        }
    }
    else{
        reset_converteur();
    }
}


void Ssm6515::reset_converteur(){
    printf("reset converteur \n");
    delete pdm2pcm;
    this->compute_pdm2pcm_params();
    this->pdm2pcm = new PdmToPcm(this->cic_r,this->cic_n, this->cic_m, this->cic_shift);
    this->reset_pdm2pcm_converter = FALSE;
    printf("reset converteur done\n");
}

void Ssm6515::reset_output_file(){
    printf("reset outstream \n");
    delete out_stream;
    int file_samplerate = this->pcm_freq;
    if(this->regmap.pdm_ctrl.pdm_mode_get()){
        file_samplerate = this->i2s_freq / this->cic_r;
    }
    else{
        file_samplerate = this->i2s_freq / this->width / 2;
    }
    this->out_stream = new Outfile_wav(this, this->output_filepath, this->out_bitrate, file_samplerate);
    this->reset_output_stream  = FALSE;
    printf("reset stream done \n");
}


uint32_t unsigned_log2(uint32_t number){
    if(number == 0){
        return 0;
    }
    uint32_t pos = 0;

    while ((number & 1) == 0) {
        number >>= 1;
        ++pos;
    }

    return pos;
}


uint32_t Ssm6515::get_output_pcm_freq(){
    uint32_t outut_pcm_freq = 48000;
    switch(this->regmap.dac_ctrl1.dac_fs_get()){
        case SSM6515_DAC_FS_8KHZ:
            outut_pcm_freq = 8000;
            break;
        case SSM6515_DAC_FS_12KHZ:
            outut_pcm_freq = 12000;
            break;
        case SSM6515_DAC_FS_16KHZ:
            outut_pcm_freq = 16000;
            break;
        case SSM6515_DAC_FS_24KHZ:
            outut_pcm_freq = 24000;
            break;
        case SSM6515_DAC_FS_32KHZ:
            outut_pcm_freq = 32000;
            break;
        case SSM6515_DAC_FS_44_1_TO_48KHZ:
            outut_pcm_freq = 48000;
            break;
        case SSM6515_DAC_FS_88_2_TO_96KHZ:
            outut_pcm_freq = 96000;
            break;
        case SSM6515_DAC_FS_176_4_TO_192KHZ:
            outut_pcm_freq = 192000;
            break;
        case SSM6515_DAC_FS_384KHZ:
            outut_pcm_freq = 384000;
            break;
        case SSM6515_DAC_FS_768KHZ:
            outut_pcm_freq = 768000;
            break;
    }
    return outut_pcm_freq;
}


// void Ssm6515::compute_i2s_freq(){

// }

uint32_t Ssm6515::get_input_pdm_freq(){
    uint32_t in_pdmfreq = 3072000;
    switch(this->regmap.pdm_ctrl.pdm_fs_get()){
            case SSM6515_PDM_FS_5_6448_TO_6_144:
                in_pdmfreq = 6144000;
                break;
            case SSM6515_PDM_FS_2_8224_TO_3_072:
                in_pdmfreq = 3072000;
                break;
            case SSM6515_PDM_FS_11_2896_TO_12_288:
                in_pdmfreq = 12288000;
                break;
        }
    return in_pdmfreq;
}


int Ssm6515::get_ws_delay(){
    int delay = 1;
    switch(this->regmap.spt_ctrl1.spt_data_format_get()){
            case SSM6515_DATA_FORMAT_I2S:
                delay = 1;
                break;
            case SSM6515_DATA_FORMAT_LJ:
                delay = 0;
                break;
            case SSM6515_DATA_FORMAT_DELAY_12:
                delay = 12;
                break;
            case SSM6515_DATA_FORMAT_DELAY_16:
                delay = 16;
                break;
        }
    return delay;
}


int Ssm6515::get_slot_width(){
    int slot_width = 1;
    switch(this->regmap.spt_ctrl1.spt_data_format_get()){
            case SSM6515_TDM_32BCLK_PER_SLOT:
                slot_width = 32;
                break;
            case SSM6515_TDM_16BCLK_PER_SLOT:
                slot_width = 16;
                break;
            case SSM6515_TDM_24BCLK_PER_SLOT:
                slot_width = 24;
                break;
        }
    return slot_width;
}


void Ssm6515::compute_pdm2pcm_params()
{
    uint32_t in_pdmfreq = get_input_pdm_freq();
    uint32_t samplerate = get_output_pcm_freq();

    this->cic_r = in_pdmfreq / samplerate;
    this->cic_shift = cic_n * unsigned_log2(this->cic_r * this->cic_m) - this->out_bitrate + 1;
}



Ssm6515::~Ssm6515()
{
    delete pdm2pcm;
    delete out_stream;
}

void Ssm6515::restart_output_stream(){
    delete pdm2pcm;
    delete out_stream;
    this->out_stream = new Outfile_wav(this, this->output_filepath, this->out_bitrate, this->pcm_freq);
    this->pdm2pcm = new PdmToPcm(this->cic_r,this->cic_n, this->cic_m, this->cic_shift);
}


void Ssm6515::push_data(int data)
{
    if (this->pending_bits == 0)
        return;

    // Shift bits from MSB
    this->current_value |= (data << (this->pending_bits - 1));
    this->pending_bits--;

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Writing data (value: %d) | pending bits %d | dqta %d \n", data,  this->pending_bits, this->current_value);

    // Sample new data if all bits have been shifted
    if (this->pending_bits == 0)
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Full sample got (value: %i)\n", this->current_value);
        this->pending_bits = this->width;
        if(this->out_stream && !this->reset_output_stream){
            this->out_stream->push_sample(this->current_value);
        }
        else{
            if(this->i2s_freq == this->prev_i2s_freq){
                reset_output_file();
            }
        }
        this->is_active = FALSE;

    }
}


void Ssm6515::start_sample()
{
    this->trace.msg(vp::Trace::LEVEL_TRACE, "Starting sample\n");
    this->pending_bits = this->width;
    printf("pendings %i\n", this->pending_bits);
    this->current_value = 0;
}






Outfile_wav::Outfile_wav(Ssm6515 *top, std::string filepath, int width, int wav_sampling_freq)
: top(top), width(width)
{
#ifdef USE_SNDFILE

    unsigned int pcm_width;


    switch (this->width)
    {
        case 8: pcm_width = SF_FORMAT_PCM_S8; break;
        case 16: pcm_width = SF_FORMAT_PCM_16; break;
        case 24: pcm_width = SF_FORMAT_PCM_24; break;
        case 32:
        default:
            pcm_width = SF_FORMAT_PCM_32; break;
        }

        this->sfinfo.format = pcm_width | SF_FORMAT_WAV;
        this->sfinfo.samplerate = wav_sampling_freq;
        this->sfinfo.channels = 1;
        this->sndfile = sf_open(filepath.c_str(), SFM_WRITE, &this->sfinfo);
        if (this->sndfile == NULL)
        {
            throw std::invalid_argument(("Failed to open file " + filepath + ": " + strerror(errno)).c_str());
    }
    this->period = 1000000000000UL / this->sfinfo.samplerate;

    this->pending_channels = 0;
    this->items = new int32_t;
    memset(this->items, 0, sizeof(uint32_t)*this->sfinfo.channels);
#else
    this->top->trace.fatal("Unable to open file (%s), libsndfile support is not active\n", filepath.c_str());
    return;

#endif
}


Outfile_wav::~Outfile_wav()
{
#ifdef USE_SNDFILE
    if (this->pending_channels)
    {
        sf_writef_int(this->sndfile, (const int *)this->items, 1);
    }
    sf_close(this->sndfile);
#endif
}

void Outfile_wav::push_sample(uint32_t data)
{
#ifdef USE_SNDFILE
    if (this->pending_channels  == 1)
    {
        sf_writef_int(this->sndfile, (const int *)this->items, 1);
        this->pending_channels = 0;
        memset(this->items, 0, sizeof(uint32_t)*this->sfinfo.channels);
    }

    data <<= (32 - this->width);
    items[0] = data;
    this->pending_channels = 1;
#endif
}



void Ssm6515::compute_i2s_freq(int64_t time){
    prev_timestamp = timestamp;
    timestamp = time;
    this->prev_i2s_freq = this->i2s_freq;
    printf("i2s freq %i\n", (int32_t)this->prev_i2s_freq);
    if(prev_timestamp != -1){
        this->i2s_freq = 1000000000 / (timestamp - prev_timestamp) * 1000;
    }
}



void Ssm6515::start_dac(vp::Block *__this, vp::TimeEvent *event){
    Ssm6515 *_this = (Ssm6515 *)__this;
    _this->dac_started = TRUE;
}

void Ssm6515::stop_dac(vp::Block *__this, vp::TimeEvent *event){
    Ssm6515 *_this = (Ssm6515 *)__this;
    delete _this->pdm2pcm;
    delete _this->out_stream;
}