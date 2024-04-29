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
#include <vp/itf/audio.hpp>
#include <devices/sound/dac/ssm6515/headers_regfields.h>
#include <devices/sound/dac/ssm6515/headers_gvsoc.h>
#include <string.h>
#include <string>
#include "pcm_pdm_conversion.hpp"
#include "io_audio.h"


typedef enum
{
    I2S,
    PDM,
    UNKNOWN
} dac_mode_e;

typedef enum
{
    SDI,
    SDO
} dac_sd_wire_e;

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
} ssm6515_pdm_mode_e;

/**
 * \brief PDM Sample Rate Selection.
 */
typedef enum
{
    SSM6515_PDM_FS_5_6448_TO_6_144 = 0,   //!< 5.6448 MHz to 6.144 MHz clock in PDM mode.
    SSM6515_PDM_FS_2_8224_TO_3_072 = 1,   //!< 2.8224 MHz to 3.072 MHz clock in PDM mode.
    SSM6515_PDM_FS_11_2896_TO_12_288 = 2, //!< 11.2896 MHz to 12.288 MHz clock in PDM mode.
} ssm6515_pdm_fs_e;

/**
 * \brief DAC Path Sample Rate Selection.
 */
typedef enum
{
    SSM6515_DAC_FS_8KHZ = 0,
    SSM6515_DAC_FS_12KHZ = 1,
    SSM6515_DAC_FS_16KHZ = 2,
    SSM6515_DAC_FS_24KHZ = 3,
    SSM6515_DAC_FS_32KHZ = 4,
    SSM6515_DAC_FS_44_1_TO_48KHZ = 5,
    SSM6515_DAC_FS_88_2_TO_96KHZ = 6,
    SSM6515_DAC_FS_176_4_TO_192KHZ = 7,
    SSM6515_DAC_FS_384KHZ = 8,
    SSM6515_DAC_FS_768KHZ = 9,
} ssm6515_dac_fs_e;

/**
 * \brief Serial Port–Selects LRCLK Polarity.
 */
typedef enum
{
    SSM6515_LRCLK_POL_NORMAL = 0,
    SSM6515_LRCLK_POL_INVERTED = 1,
} ssm6515_lrclk_pol_e;

/**
 * \brief Serial Port–Selects BCLK Polarity.
 */
typedef enum
{
    SSM6515_BCLK_POL_RISING = 0,  //!< Capture on rising edge.
    SSM6515_BCLK_POL_FALLING = 1, //!< Capture on falling edge.
} ssm6515_bclk_pol_e;

/**
 * \brief Serial Port–Selects TDM Slot Width.
 */
typedef enum
{
    SSM6515_TDM_32BCLK_PER_SLOT = 0,
    SSM6515_TDM_16BCLK_PER_SLOT = 1,
    SSM6515_TDM_24BCLK_PER_SLOT = 2,
} ssm6515_tdm_slot_width_e;

/**
 * \brief Serial Port–Selects Data Format.
 */
typedef enum
{
    SSM6515_DATA_FORMAT_I2S = 0,
    SSM6515_DATA_FORMAT_LJ = 1,
    SSM6515_DATA_FORMAT_DELAY_8 = 2,
    SSM6515_DATA_FORMAT_DELAY_12 = 3,
    SSM6515_DATA_FORMAT_DELAY_16 = 4,
} ssm6515_data_format_e;

/**
 * \brief Serial Port–Selects Stereo or TDM Mode.
 */
typedef enum
{
    SSM6515_SAI_MODE_STEREO = 0,
    SSM6515_SAI_MODE_TDM = 1,
} ssm6515_sai_mode_e;



/***********************************************************
 *                   Class Declaration                     *
 ***********************************************************/

class Ssm6515 : public vp::Component
{
public:
    Ssm6515(vp::ComponentConf &conf);
    ~Ssm6515();

protected:
    std::string handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string req);
    static void set_rate(vp::Block *__this, int rate);
    static void i2c_sync(vp::Block *__this, int scl, int sda);
    static void i2s_sync(vp::Block *__this, int sck, int ws, int sd, bool full_duplex); // in PDM mode only for the moment
    static void push_sample(vp::Block *__this, double value);
    void i2c_start(unsigned int address, bool is_read);
    void i2c_handle_byte(uint8_t byte);
    void i2c_stop();
    void i2c_get_data();
    void i2c_send_byte(uint8_t byte);
    void reset(bool);
    void start();

    void handle_reg_write(uint8_t address, uint8_t value);
    uint8_t handle_reg_read(uint8_t address);

    void handle_status_clr_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_pwr_ctrl_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_spt_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_dac_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);
    void handle_reset_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write);

    static void start_dac(vp::Block *__this, vp::TimeEvent *event);
    static void stop_dac(vp::Block *__this, vp::TimeEvent *event);
    vp::TimeEvent starting_event;

    int get_ws_delay();
    int get_slot_width();
    uint8_t is_pdm_mode();
    uint8_t i2s_slot();
    uint8_t is_i2s_mode();
    uint8_t clk_pol();
    uint8_t ws_pol();
    uint32_t get_input_pdm_freq();
    uint32_t get_output_pcm_freq();
    void compute_internal_pcm_freq();

    uint8_t is_manual_mode = 0;

    // i2c
    vp_regmap_i2c_regmap regmap;
    vp::Trace trace;
    vp::I2cMaster i2c_itf;
    vp::I2sMaster i2s_itf;
    vp::AudioSlave audio_in_itf;
    vp::AudioMaster audio_out_itf;
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
    bool dac_started = FALSE;

    // output and converter
    void reset_output_file();
    void reset_converter();
    void restart_output_stream();
    std::string output_filepath;

    bool reset_pdm2pcm_converter = TRUE;
    bool reset_output_stream = TRUE;

    Tx_stream *out_stream = nullptr;
    PdmToPcm *pdm2pcm = nullptr;

    // pdm to pcm conversion
    void compute_pdm2pcm_params();
    uint32_t cic_m = 2;
    uint32_t cic_n = 8;
    uint32_t cic_r = 64;
    uint32_t cic_shift = 7;

    // audio itf
    int32_t saturate(int32_t sample);
    int32_t audio_in_itf_sample = 0;
    bool new_audio_in_itf_sample = false;

    // settings
    dac_sd_wire_e i2s_wire = SDO;
    dac_mode_e manual_mode_set = UNKNOWN;

    uint32_t out_nb_bits = 24;
    uint32_t pcm_freq = 48000;
    int pcm_freq_from_audio = -1;
    int pcm_freq_from_gvcontrol = -1;
    int pcm_freq_from_gap = -1;
    int manual_slot_width = -1;
    int manual_i2s_delay = -1;

    // i2s
    void pdm_sync(int sd);
    void start_sample();
    void push_data(int data);
    void push_sample(uint32_t data);

    bool is_active = false;
    uint8_t ws_delay = 1;
    uint8_t current_ws_delay = 0;
    int32_t internal_pcm_freq = 0;

    int32_t current_value;
    uint32_t manual_outut_pcm_freq = -1;

    uint32_t pdm_dyn_range;

    // debug
    void restart_debug_files();
    Tx_stream *debug_file_audio = nullptr;
    Tx_stream *debug_file_dac = nullptr;
    Tx_stream *debug_file_mix = nullptr;
    uint8_t debug_files_enabled = 0;

    // ------ SAI interface--------//
    bool sync_ongoing = false;
    bool is_pdm = true;
    int prev_sck;
    int prev_ws;
    int ws;
    int sdio;
    bool is_full_duplex = false;

    int pending_bits = 32;
    int width = 32;

    // estimate i2s clock frequency
    void compute_i2s_clk_freq(int64_t time);
    uint64_t i2s_clk_freq = 0;
    uint64_t prev_i2s_clk_freq[3] = {0};
    uint64_t mean_freq = 0;
    uint64_t prev_mean_freq = 0;
    int64_t prev_timestamp = -1;
    int64_t timestamp = -1;
};


/***********************************************************
 *                  Methods Instantiation                  *
 ***********************************************************/

Ssm6515::Ssm6515(vp::ComponentConf &config)
    : vp::Component(config), regmap(*this, "regmap"), starting_event(this, Ssm6515::start_dac)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->i2c_itf.set_sync_meth(&Ssm6515::i2c_sync);
    this->new_master_port("i2c", &this->i2c_itf);

    this->i2s_itf.set_sync_meth(&Ssm6515::i2s_sync);
    this->new_master_port("i2s", &this->i2s_itf);

    this->audio_out_itf.set_set_rate_meth(&Ssm6515::set_rate);
    this->new_master_port("audio_output", &this->audio_out_itf);

    this->audio_in_itf.set_set_rate_meth(&Ssm6515::set_rate);
    this->audio_in_itf.set_push_sample_meth(&Ssm6515::push_sample);
    this->new_slave_port("audio_input", &this->audio_in_itf);

    this->i2c_state = I2C_STATE_WAIT_START;
    this->i2c_prev_sda = 1;
    this->i2c_prev_scl = 1;
    this->i2c_being_addressed = false;
    this->device_address = this->get_js_config()->get_child_int("i2c_address");
    this->output_filepath = this->get_js_config()->get_child_str("output_filepath");
    if (this->get_js_config()->get_child_str("sd") == "sdi")
    {
        this->i2s_wire = SDI;
    }
    else
    {
        this->i2s_wire = SDO;
    }
    this->waiting_reg_address = true;

    this->regmap.build(this, &this->trace);

    this->regmap.status_clr.register_callback(std::bind(&Ssm6515::handle_status_clr_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.pwr_ctrl.register_callback(std::bind(&Ssm6515::handle_pwr_ctrl_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.reset_reg.register_callback(std::bind(&Ssm6515::handle_reset_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.dac_ctrl1.register_callback(std::bind(&Ssm6515::handle_dac_ctrl1_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
    this->regmap.spt_ctrl1.register_callback(std::bind(&Ssm6515::handle_spt_ctrl1_access, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), true);
}

void Ssm6515::start()
{
    this->i2c_itf.sync(1, 1);
}

void Ssm6515::set_rate(vp::Block *__this, int rate)
{
    Ssm6515 *_this = (Ssm6515 *)__this;
    _this->trace.msg(vp::Trace::LEVEL_INFO, "Rate set by audio connection %d\n", rate);
    _this->pcm_freq_from_audio = rate;
    _this->compute_internal_pcm_freq();
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

        case I2C_STATE_ACK:
        {
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

        case I2C_STATE_READ_ACK:
        {
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

    if (this->regmap.status_clr.get())
    {
        // reset all
        this->regmap.status.reset(true);
    }
}

// callback called when accessing status_clr register
void Ssm6515::handle_pwr_ctrl_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.pwr_ctrl.update(reg_offset, size, value, is_write);

    if (this->regmap.pwr_ctrl.spwdn_get() == 0)
    {
        // start dac
        // 26 ms according to documentation
        this->starting_event.enqueue(26000000000);
    }
}

void Ssm6515::handle_dac_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.dac_ctrl1.update(reg_offset, size, value, is_write);
    if (this->get_output_pcm_freq() != this->pcm_freq)
    {
        this->pcm_freq = this->get_output_pcm_freq();
        this->reset_pdm2pcm_converter = TRUE;
        this->reset_output_stream = TRUE;
    }
}

void Ssm6515::handle_spt_ctrl1_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.spt_ctrl1.update(reg_offset, size, value, is_write);
    if (this->get_slot_width() != this->width)
    {
        this->width = this->get_slot_width();
        this->reset_pdm2pcm_converter = TRUE;
        this->reset_output_stream = TRUE;
    }

    this->ws_delay = this->get_ws_delay();
}

void Ssm6515::reset(bool active)
{
    this->regmap.reset(active);
}

// callback called when accessing status_clr register
void Ssm6515::handle_reset_access(uint64_t reg_offset, int size, uint8_t *value, bool is_write)
{
    this->regmap.reset_reg.update(reg_offset, size, value, is_write);

    if (this->regmap.reset_reg.soft_full_reset_get() == 1)
    {
        // reset all registers
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Reseting registers\n");
        this->regmap.reset(TRUE);
    }
}

// --------------------------------------
// ------------- i2s part ---------------
// --------------------------------------

uint8_t Ssm6515::is_pdm_mode()
{
    // if(no_external_config)
    if (manual_mode_set == PDM)
    {
        return 1;
    }
    return this->regmap.pdm_ctrl.pdm_mode_get();
}
uint8_t Ssm6515::i2s_slot()
{
    // if(no_external_config)
    return this->regmap.spt_ctrl2.spt_slot_sel_get();
}
uint8_t Ssm6515::is_i2s_mode()
{
    // if(no_external_config)

    if (manual_mode_set == I2S)
    {
        return 0;
    }
    return regmap.spt_ctrl1.spt_sai_mode_get();
}
uint8_t Ssm6515::clk_pol()
{
    // if(no_external_config)
    return regmap.spt_ctrl1.spt_bclk_pol_get();
}
uint8_t Ssm6515::ws_pol()
{
    // if(no_external_config)
    return regmap.spt_ctrl1.spt_lrclk_pol_get();
}

void Ssm6515::i2s_sync(vp::Block *__this, int sck, int ws, int sdio, bool is_full_duplex)
{
    Ssm6515 *_this = (Ssm6515 *)__this;

    if (_this->sync_ongoing)
        return;

    if (_this->clk_pol())
    { // Bit Clock
        sck = sck ^ 1;
    }
    if (_this->ws_pol())
    { // left right clk polarity
        ws = ws ^ 1;
    }

    _this->sync_ongoing = true;

    _this->sdio = sdio;

    _this->ws = ws;

    int sd = _this->i2s_wire == SDO ? sdio >> 2 : sdio & 0x3;

    if (sck != _this->prev_sck)
    {
        _this->trace.msg(vp::Trace::LEVEL_TRACE, "I2S edge (sck: %d, ws: %d, sdo: %d)\n", sck, ws, sd);

        _this->prev_sck = sck;

        if (_this->is_pdm_mode())
        {
            if (sck == 1)
            {
                _this->compute_i2s_clk_freq(_this->time.get_time());
                _this->pdm_sync(sd);
            }
        }
        else if (_this->is_i2s_mode() == 0)
        {

            if (sck)
            {
                _this->compute_i2s_clk_freq(_this->time.get_time());
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

                // The channel is the one of this dac
                if (_this->prev_ws != ws && ws == _this->i2s_slot())
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
        else
        {
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
            if (this->out_stream && !this->reset_output_stream)
            {
                this->push_sample(out);
            }
            else
            {
                reset_output_file();
            }
        }
    }
    else
    {
        reset_converter();
    }
}

void Ssm6515::reset_converter()
{
    delete pdm2pcm;
    if (this->internal_pcm_freq > 0)
    {
        this->trace.msg(vp::Trace::LEVEL_INFO, "Reset PDM -> PCM converter\n");
        this->compute_pdm2pcm_params();
        this->pdm2pcm = new PdmToPcm(this->cic_r, this->cic_n, this->cic_m, this->cic_shift);
        this->reset_pdm2pcm_converter = FALSE;
    }
    else
    {
        this->reset_pdm2pcm_converter = TRUE;
    }
}

void Ssm6515::reset_output_file()
{
    if (this->i2s_clk_freq > 0)
    {
        get_slot_width();
        this->trace.msg(vp::Trace::LEVEL_INFO, "Reset output file\n");
        delete out_stream;
        try
        {
            this->out_stream = new Tx_stream_wav(this->output_filepath, this->out_nb_bits, get_output_pcm_freq(), internal_pcm_freq);
        }
        catch (const std::invalid_argument &e)
        {
            this->trace.fatal("%s\n", e.what());
        }
        this->reset_output_stream = FALSE;
        this->trace.msg(vp::Trace::LEVEL_INFO, "Reset output file\n");
        restart_debug_files();
    }
    else
    {
        this->reset_output_stream = TRUE;
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

uint32_t Ssm6515::get_output_pcm_freq()
{
    uint32_t outut_pcm_freq = 48000;
    switch (this->regmap.dac_ctrl1.dac_fs_get())
    {
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
    if (manual_outut_pcm_freq != -1)
    {
        outut_pcm_freq = manual_outut_pcm_freq;
    }

    return outut_pcm_freq;
}

uint32_t Ssm6515::get_input_pdm_freq()
{
    uint32_t in_pdmfreq = 3072000;
    switch (this->regmap.pdm_ctrl.pdm_fs_get())
    {
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

int Ssm6515::get_ws_delay()
{
    int delay = 1;
    switch (this->regmap.spt_ctrl1.spt_data_format_get())
    {
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
    if (manual_i2s_delay != -1)
    {
        delay = manual_i2s_delay;
    }
    return delay;
}

int Ssm6515::get_slot_width()
{

    int slot_width = 1;
    switch (this->regmap.spt_ctrl1.spt_data_format_get())
    {
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
    if (manual_slot_width != -1)
    {
        slot_width = manual_slot_width;
    }
    this->out_nb_bits = slot_width;
    return slot_width;
}

void Ssm6515::compute_internal_pcm_freq()
{
    if((int32_t)i2s_clk_freq > 0)
    {
        if (this->is_pdm_mode())
        {
            if (pcm_freq_from_audio > 0)
            {
                this->internal_pcm_freq = pcm_freq_from_audio;
            }
            else
            {
                this->internal_pcm_freq = (int32_t)i2s_clk_freq / 16;
            }
        }
        else
        {
            this->internal_pcm_freq = (int32_t)i2s_clk_freq / this->width / 2;
            if (pcm_freq_from_audio > 0)
            {
                if(this->internal_pcm_freq != pcm_freq_from_audio)
                {
                    this->trace.fatal("Sai frequency and asked PCM frequency from audio world are uncompatible\n");
                }
            }
        }
    }
}

void Ssm6515::compute_pdm2pcm_params()
{
    // TODO : for manual mode take frequency from i2s freq and not from default config
    uint32_t in_pdmfreq = get_input_pdm_freq();
    uint32_t samplerate = internal_pcm_freq;
    this->cic_r = in_pdmfreq / samplerate;
    if (samplerate * cic_r != in_pdmfreq)
    {
        this->trace.fatal("Internal required PCM sampling freq doesn't match with PDM clk frequency\n");
    }
    this->cic_shift = cic_n * unsigned_log2(this->cic_r * this->cic_m) - this->out_nb_bits - 1; // - 1 to compensate the 6 db lost  in the modulation process.
    if(cic_shift > 32)
    {
        cic_shift = 0;
    }
    if((cic_n * unsigned_log2(this->cic_r * this->cic_m) - this->cic_shift - 1) < this->out_nb_bits)
    {
        this->trace.msg(vp::Trace::LEVEL_WARNING,"Demodulated signal has a dynamic range of %d whereas %d are asked.This will lead in a reduced amplitude\n",
                        cic_n * unsigned_log2(this->cic_r * this->cic_m) - this->cic_shift - 1, this->out_nb_bits);
    }
    pdm_dyn_range = (cic_n * unsigned_log2(this->cic_r * this->cic_m) - this->cic_shift - 1);
    this->trace.msg(vp::Trace::LEVEL_INFO, "Computed new PDM -> PCM params : cic_r %d, cic_shift %d, cic_n %d, cic_m %d\n", cic_r, cic_shift, cic_n, cic_m);
}

Ssm6515::~Ssm6515()
{
    delete pdm2pcm;
    delete out_stream;

    delete debug_file_dac;
    delete debug_file_audio;
    delete debug_file_mix;
}

void Ssm6515::restart_output_stream()
{
    delete pdm2pcm;
    delete out_stream;
    try
    {
        this->out_stream = new Tx_stream_wav(this->output_filepath, this->out_nb_bits, get_output_pcm_freq(), internal_pcm_freq);
    }
    catch (const std::invalid_argument &e)
    {
        this->trace.fatal("%s\n", e.what());
    }
    this->pdm2pcm = new PdmToPcm(this->cic_r, this->cic_n, this->cic_m, this->cic_shift);
}

void Ssm6515::push_data(int data)
{
    if (this->pending_bits == 0)
        return;

    // Shift bits from MSB
    this->current_value |= (data << (this->pending_bits - 1));
    this->pending_bits--;

    this->trace.msg(vp::Trace::LEVEL_TRACE, "Writing data (value: %d) | pending bits %d | data %d \n", data, this->pending_bits, this->current_value);

    // Sample new data if all bits have been shifted
    if (this->pending_bits == 0)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Full sample got (value: %i)\n", this->current_value);
        this->pending_bits = this->width;
        if (this->out_stream && !this->reset_output_stream)
        {
            this->push_sample(this->current_value);
        }
        else
        {
            reset_output_file();
        }
        this->is_active = FALSE;
    }
}

void Ssm6515::start_sample()
{
    this->pending_bits = this->width;
    this->trace.msg(vp::Trace::LEVEL_INFO, "Starting sample, %i bits pending\n", this->pending_bits);
    this->current_value = 0;
}


// convert double audio_world value to int with the correct width
int32_t double_to_dac_int_width(double value, int width)
{
    double converted = value * (double)(~(1<<width-1));
    return (int32_t) converted;
}

void Ssm6515::push_sample(vp::Block *__this, double audio_value)
{
    Ssm6515 *_this = (Ssm6515 *)__this;
    // mic waits for 24 bits variable
    int32_t value = double_to_dac_int_width(audio_value, _this->out_nb_bits);
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Audio world sample incomming %d\n", value);
    _this->audio_in_itf_sample = (int32_t)value;
    _this->new_audio_in_itf_sample = true;
}

int32_t Ssm6515::saturate(int32_t sample)
{
    int32_t maxi, mini;
    maxi = (width == 32) ? (~(1 << (out_nb_bits - 1))) : (1 << (out_nb_bits - 1));
    mini = (width == 32) ? (1 << (out_nb_bits - 1)) : (~(1 << (out_nb_bits - 1)));

    if (sample > maxi)
    {
        return maxi;
    }
    if (sample < mini)
    {
        return mini;
    }
    return sample;
}


double to_audio_world(int32_t value, int width)
{
    double converted_value = double(value);
    double devider = (width == 32) ? (double)(~(1 << (width - 1))) : (double)(1 << (width - 1));
    converted_value = converted_value / devider;
    return converted_value;
}

// in PDM -> PCM demod, required width an obtained one can be uncompatible
// in this case signal must be amplified.
int32_t normalize_int(int32_t value, int actual_width, int wanted_width)
{
    double new_val = 0;
    double current_val = (double) value;
    double devider = (actual_width == 32) ? (double)(~(1 << (actual_width - 1))) : (double)(1 << (actual_width - 1));
    double mult = (wanted_width == 32) ? (double)(~(1 << (wanted_width - 1))) : (double)(1 << (wanted_width - 1));

    new_val = current_val /  devider * mult;
    return (int32_t) new_val;
}

void Ssm6515::push_sample(uint32_t data)
{
    // normalize to waited sample_width if pdm demod doesn't allow it
    if (this->is_pdm_mode())
    {
        data = normalize_int(data, pdm_dyn_range, out_nb_bits);
    }

    if (debug_file_dac)
    {
        debug_file_dac->push_sample(data);
    }

    if (this->audio_out_itf.is_bound())
    {
        this->audio_out_itf.push_sample(to_audio_world(saturate(data), out_nb_bits));
    }
    int32_t internal_sample = (int32_t)data;

    if (new_audio_in_itf_sample)
    {
        audio_in_itf_sample = audio_in_itf_sample;

        trace.msg(vp::Trace::LEVEL_TRACE, "Mixing audio world sample %d to dac sample %d\n", audio_in_itf_sample, internal_sample);

        out_stream->push_sample(saturate(internal_sample + audio_in_itf_sample));

        if (debug_file_audio)
        {
            debug_file_audio->push_sample(audio_in_itf_sample);
        }

        if (debug_file_mix)
        {
            debug_file_mix->push_sample(data + audio_in_itf_sample);
        }
    }
    else
    {
        out_stream->push_sample(data);

        if (debug_file_mix)
        {
            debug_file_mix->push_sample(data);
        }
    }
}

void Ssm6515::restart_debug_files()
{
    if (debug_files_enabled)
    {
        this->trace.msg(vp::Trace::LEVEL_INFO, "Init debug files\n");
        delete debug_file_dac;
        delete debug_file_audio;
        delete debug_file_mix;
        try
        {
            this->debug_file_dac = new Tx_stream_wav(get_name() + "_data_from_dac_debug.wav", this->out_nb_bits, internal_pcm_freq, internal_pcm_freq);
            this->debug_file_audio = new Tx_stream_wav(get_name() + "_data_from_audio_connection_debug.wav", this->out_nb_bits, internal_pcm_freq, internal_pcm_freq);
            this->debug_file_mix = new Tx_stream_wav(get_name() + "_data_mixed_debug.wav", this->out_nb_bits, internal_pcm_freq, internal_pcm_freq);
        }
        catch (const std::invalid_argument &e)
        {
            this->trace.fatal("%s\n", e.what());
        }
    }
}

uint64_t compute_mean_freq(uint64_t array[3])
{
    return (array[0] + array[1] + array[2]) / 3;
}

void Ssm6515::compute_i2s_clk_freq(int64_t time)
{
    prev_timestamp = timestamp;
    timestamp = time;
    uint64_t int_i2s_freq = 0;
    if (prev_timestamp != -1)
    {
        int_i2s_freq = 1000000000 / (timestamp - prev_timestamp) * 1000;
    }

    prev_i2s_clk_freq[0] = prev_i2s_clk_freq[1];
    prev_i2s_clk_freq[1] = prev_i2s_clk_freq[2];
    prev_i2s_clk_freq[2] = int_i2s_freq;
    if (compute_mean_freq(prev_i2s_clk_freq) == prev_i2s_clk_freq[2])
    {
        prev_mean_freq = mean_freq;
        mean_freq = prev_i2s_clk_freq[2];
        if (mean_freq != prev_mean_freq)
        {
            this->i2s_clk_freq = mean_freq;
            this->trace.msg(vp::Trace::LEVEL_WARNING, "Change detected in SAI frequency, new frequency is %d Hz, previous one was %d Hz\n", mean_freq, prev_mean_freq);
            // do all you need if i2s freq change
            compute_internal_pcm_freq();
            reset_converter();
            reset_output_file();
        }
    }
    this->i2s_clk_freq = mean_freq;
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "i2s frequency : %d Hz\n", this->i2s_clk_freq);
}

void Ssm6515::start_dac(vp::Block *__this, vp::TimeEvent *event)
{
    Ssm6515 *_this = (Ssm6515 *)__this;
    _this->dac_started = TRUE;
}

void Ssm6515::stop_dac(vp::Block *__this, vp::TimeEvent *event)
{
    Ssm6515 *_this = (Ssm6515 *)__this;
    delete _this->pdm2pcm;
    delete _this->out_stream;
}

std::string Ssm6515::handle_command(gv::GvProxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{
    int error = 0;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == "mode")
        {
            if (args[i + 1] == "pdm" || args[i + 1] == "PDM")
            {
                manual_mode_set = PDM;
            }
            else if (args[i + 1] == "i2s" || args[i + 1] == "I2S")
            {
                manual_mode_set = I2S;
                manual_i2s_delay = 1;
            }
            else
            {
                this->trace.msg(vp::Trace::LEVEL_ERROR, "Unknown mode %s for dac, please use i2s or pdm mode\n", args[i + 1]);
            }
        }
        if (args[i] == "pcm_freq")
        {
            manual_outut_pcm_freq = atoi(args[i + 1].c_str());
        }
        if (args[i] == "width")
        {
            manual_slot_width = atoi(args[i + 1].c_str());
        }

        if (args[i] == "debug")
        {
            debug_files_enabled = atoi(args[i + 1].c_str());
        }

        if (args[i] == "file_path")
        {
            this->output_filepath = args[i + 1];
        }
    }

    if (error)
    {
        return "err=" + std::to_string(error);
    }
    return "err=1";
}
