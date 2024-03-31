
/* THIS FILE HAS BEEN GENERATED, DO NOT MODIFY IT.
 */

/*
 * Copyright (C) 2020 GreenWaves Technologies
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

#ifndef __ARCHI_I2C_REGMAP_GVSOC__
#define __ARCHI_I2C_REGMAP_GVSOC__

#if !defined(LANGUAGE_ASSEMBLY) && !defined(__ASSEMBLER__)

#include <stdint.h>

#endif




//
// REGISTERS STRUCTS
//

#ifdef __GVSOC__

class vp_i2c_regmap_vendor_id : public vp::Register<uint8_t>
{
public:
    inline void vendor_set(uint8_t value) { this->set_field(value, I2C_REGMAP_VENDOR_ID_VENDOR_BIT, I2C_REGMAP_VENDOR_ID_VENDOR_WIDTH); }
    inline uint8_t vendor_get() { return this->get_field(I2C_REGMAP_VENDOR_ID_VENDOR_BIT, I2C_REGMAP_VENDOR_ID_VENDOR_WIDTH); }
    vp_i2c_regmap_vendor_id(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "VENDOR_ID";
        this->offset = 0x0;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x0;
        this->reset_val = 0x41;
        this->regfields.push_back(new vp::regfield("VENDOR", 0, 8));
    }
};

class vp_i2c_regmap_device_id1 : public vp::Register<uint8_t>
{
public:
    inline void device1_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DEVICE_ID1_DEVICE1_BIT, I2C_REGMAP_DEVICE_ID1_DEVICE1_WIDTH); }
    inline uint8_t device1_get() { return this->get_field(I2C_REGMAP_DEVICE_ID1_DEVICE1_BIT, I2C_REGMAP_DEVICE_ID1_DEVICE1_WIDTH); }
    vp_i2c_regmap_device_id1(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "DEVICE_ID1";
        this->offset = 0x1;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x0;
        this->reset_val = 0x65;
        this->regfields.push_back(new vp::regfield("DEVICE1", 0, 8));
    }
};

class vp_i2c_regmap_device_id2 : public vp::Register<uint8_t>
{
public:
    inline void device2_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DEVICE_ID2_DEVICE2_BIT, I2C_REGMAP_DEVICE_ID2_DEVICE2_WIDTH); }
    inline uint8_t device2_get() { return this->get_field(I2C_REGMAP_DEVICE_ID2_DEVICE2_BIT, I2C_REGMAP_DEVICE_ID2_DEVICE2_WIDTH); }
    vp_i2c_regmap_device_id2(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "DEVICE_ID2";
        this->offset = 0x2;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x0;
        this->reset_val = 0x15;
        this->regfields.push_back(new vp::regfield("DEVICE2", 0, 8));
    }
};

class vp_i2c_regmap_revision : public vp::Register<uint8_t>
{
public:
    inline void rev_set(uint8_t value) { this->set_field(value, I2C_REGMAP_REVISION_REV_BIT, I2C_REGMAP_REVISION_REV_WIDTH); }
    inline uint8_t rev_get() { return this->get_field(I2C_REGMAP_REVISION_REV_BIT, I2C_REGMAP_REVISION_REV_WIDTH); }
    vp_i2c_regmap_revision(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "REVISION";
        this->offset = 0x3;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x0;
        this->reset_val = 0x2;
        this->regfields.push_back(new vp::regfield("Rev", 0, 8));
    }
};

class vp_i2c_regmap_pwr_ctrl : public vp::Register<uint8_t>
{
public:
    inline void spwdn_set(uint8_t value) { this->set_field(value, I2C_REGMAP_PWR_CTRL_SPWDN_BIT, I2C_REGMAP_PWR_CTRL_SPWDN_WIDTH); }
    inline uint8_t spwdn_get() { return this->get_field(I2C_REGMAP_PWR_CTRL_SPWDN_BIT, I2C_REGMAP_PWR_CTRL_SPWDN_WIDTH); }
    inline void apwdn_en_set(uint8_t value) { this->set_field(value, I2C_REGMAP_PWR_CTRL_APWDN_EN_BIT, I2C_REGMAP_PWR_CTRL_APWDN_EN_WIDTH); }
    inline uint8_t apwdn_en_get() { return this->get_field(I2C_REGMAP_PWR_CTRL_APWDN_EN_BIT, I2C_REGMAP_PWR_CTRL_APWDN_EN_WIDTH); }
    inline void lim_en_set(uint8_t value) { this->set_field(value, I2C_REGMAP_PWR_CTRL_LIM_EN_BIT, I2C_REGMAP_PWR_CTRL_LIM_EN_WIDTH); }
    inline uint8_t lim_en_get() { return this->get_field(I2C_REGMAP_PWR_CTRL_LIM_EN_BIT, I2C_REGMAP_PWR_CTRL_LIM_EN_WIDTH); }
    vp_i2c_regmap_pwr_ctrl(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "PWR_CTRL";
        this->offset = 0x4;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x13;
        this->reset_val = 0x1;
        this->regfields.push_back(new vp::regfield("SPWDN", 0, 1));
        this->regfields.push_back(new vp::regfield("APWDN_EN", 1, 1));
        this->regfields.push_back(new vp::regfield("LIM_EN", 4, 1));
    }
};

class vp_i2c_regmap_clk_ctrl : public vp::Register<uint8_t>
{
public:
    inline void bclk_rate_set(uint8_t value) { this->set_field(value, I2C_REGMAP_CLK_CTRL_BCLK_RATE_BIT, I2C_REGMAP_CLK_CTRL_BCLK_RATE_WIDTH); }
    inline uint8_t bclk_rate_get() { return this->get_field(I2C_REGMAP_CLK_CTRL_BCLK_RATE_BIT, I2C_REGMAP_CLK_CTRL_BCLK_RATE_WIDTH); }
    vp_i2c_regmap_clk_ctrl(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "CLK_CTRL";
        this->offset = 0x5;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x1f;
        this->reset_val = 0x0;
        this->regfields.push_back(new vp::regfield("BCLK_RATE", 0, 5));
    }
};

class vp_i2c_regmap_pdm_ctrl : public vp::Register<uint8_t>
{
public:
    inline void pdm_mode_set(uint8_t value) { this->set_field(value, I2C_REGMAP_PDM_CTRL_PDM_MODE_BIT, I2C_REGMAP_PDM_CTRL_PDM_MODE_WIDTH); }
    inline uint8_t pdm_mode_get() { return this->get_field(I2C_REGMAP_PDM_CTRL_PDM_MODE_BIT, I2C_REGMAP_PDM_CTRL_PDM_MODE_WIDTH); }
    inline void pdm_fs_set(uint8_t value) { this->set_field(value, I2C_REGMAP_PDM_CTRL_PDM_FS_BIT, I2C_REGMAP_PDM_CTRL_PDM_FS_WIDTH); }
    inline uint8_t pdm_fs_get() { return this->get_field(I2C_REGMAP_PDM_CTRL_PDM_FS_BIT, I2C_REGMAP_PDM_CTRL_PDM_FS_WIDTH); }
    inline void pdm_chan_sel_set(uint8_t value) { this->set_field(value, I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_BIT, I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_WIDTH); }
    inline uint8_t pdm_chan_sel_get() { return this->get_field(I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_BIT, I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_WIDTH); }
    inline void pdm_filt_set(uint8_t value) { this->set_field(value, I2C_REGMAP_PDM_CTRL_PDM_FILT_BIT, I2C_REGMAP_PDM_CTRL_PDM_FILT_WIDTH); }
    inline uint8_t pdm_filt_get() { return this->get_field(I2C_REGMAP_PDM_CTRL_PDM_FILT_BIT, I2C_REGMAP_PDM_CTRL_PDM_FILT_WIDTH); }
    vp_i2c_regmap_pdm_ctrl(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "PDM_CTRL";
        this->offset = 0x6;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x3f;
        this->reset_val = 0x20;
        this->regfields.push_back(new vp::regfield("PDM_MODE", 0, 1));
        this->regfields.push_back(new vp::regfield("PDM_FS", 1, 2));
        this->regfields.push_back(new vp::regfield("PDM_CHAN_SEL", 3, 1));
        this->regfields.push_back(new vp::regfield("PDM_FILT", 4, 2));
    }
};

class vp_i2c_regmap_dac_ctrl1 : public vp::Register<uint8_t>
{
public:
    inline void dac_fs_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL1_DAC_FS_BIT, I2C_REGMAP_DAC_CTRL1_DAC_FS_WIDTH); }
    inline uint8_t dac_fs_get() { return this->get_field(I2C_REGMAP_DAC_CTRL1_DAC_FS_BIT, I2C_REGMAP_DAC_CTRL1_DAC_FS_WIDTH); }
    inline void dac_pwr_mode_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_BIT, I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_WIDTH); }
    inline uint8_t dac_pwr_mode_get() { return this->get_field(I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_BIT, I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_WIDTH); }
    inline void dac_ibias_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_BIT, I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_WIDTH); }
    inline uint8_t dac_ibias_get() { return this->get_field(I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_BIT, I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_WIDTH); }
    vp_i2c_regmap_dac_ctrl1(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "DAC_CTRL1";
        this->offset = 0x7;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x15;
        this->regfields.push_back(new vp::regfield("DAC_FS", 0, 4));
        this->regfields.push_back(new vp::regfield("DAC_PWR_MODE", 4, 2));
        this->regfields.push_back(new vp::regfield("DAC_IBIAS", 6, 2));
    }
};

class vp_i2c_regmap_dac_ctrl2 : public vp::Register<uint8_t>
{
public:
    inline void dac_mute_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL2_DAC_MUTE_BIT, I2C_REGMAP_DAC_CTRL2_DAC_MUTE_WIDTH); }
    inline uint8_t dac_mute_get() { return this->get_field(I2C_REGMAP_DAC_CTRL2_DAC_MUTE_BIT, I2C_REGMAP_DAC_CTRL2_DAC_MUTE_WIDTH); }
    inline void dac_vol_mode_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_BIT, I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_WIDTH); }
    inline uint8_t dac_vol_mode_get() { return this->get_field(I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_BIT, I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_WIDTH); }
    inline void dac_more_filt_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_BIT, I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_WIDTH); }
    inline uint8_t dac_more_filt_get() { return this->get_field(I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_BIT, I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_WIDTH); }
    inline void dac_vol_zc_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_BIT, I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_WIDTH); }
    inline uint8_t dac_vol_zc_get() { return this->get_field(I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_BIT, I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_WIDTH); }
    inline void dac_hard_vol_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_BIT, I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_WIDTH); }
    inline uint8_t dac_hard_vol_get() { return this->get_field(I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_BIT, I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_WIDTH); }
    inline void dac_perf_mode_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_BIT, I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_WIDTH); }
    inline uint8_t dac_perf_mode_get() { return this->get_field(I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_BIT, I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_WIDTH); }
    inline void dac_invert_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL2_DAC_INVERT_BIT, I2C_REGMAP_DAC_CTRL2_DAC_INVERT_WIDTH); }
    inline uint8_t dac_invert_get() { return this->get_field(I2C_REGMAP_DAC_CTRL2_DAC_INVERT_BIT, I2C_REGMAP_DAC_CTRL2_DAC_INVERT_WIDTH); }
    vp_i2c_regmap_dac_ctrl2(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "DAC_CTRL2";
        this->offset = 0x8;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x13;
        this->regfields.push_back(new vp::regfield("DAC_MUTE", 0, 1));
        this->regfields.push_back(new vp::regfield("DAC_VOL_MODE", 1, 2));
        this->regfields.push_back(new vp::regfield("DAC_MORE_FILT", 3, 1));
        this->regfields.push_back(new vp::regfield("DAC_VOL_ZC", 4, 1));
        this->regfields.push_back(new vp::regfield("DAC_HARD_VOL", 5, 1));
        this->regfields.push_back(new vp::regfield("DAC_PERF_MODE", 6, 1));
        this->regfields.push_back(new vp::regfield("DAC_INVERT", 7, 1));
    }
};

class vp_i2c_regmap_dac_ctrl3 : public vp::Register<uint8_t>
{
public:
    inline void dac_hpf_en_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_BIT, I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_WIDTH); }
    inline uint8_t dac_hpf_en_get() { return this->get_field(I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_BIT, I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_WIDTH); }
    inline void dac_hpf_fc_set(uint8_t value) { this->set_field(value, I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_BIT, I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_WIDTH); }
    inline uint8_t dac_hpf_fc_get() { return this->get_field(I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_BIT, I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_WIDTH); }
    vp_i2c_regmap_dac_ctrl3(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "DAC_CTRL3";
        this->offset = 0x9;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xf1;
        this->reset_val = 0xd0;
        this->regfields.push_back(new vp::regfield("DAC_HPF_EN", 0, 1));
        this->regfields.push_back(new vp::regfield("DAC_HPF_FC", 4, 4));
    }
};

class vp_i2c_regmap_dac_vol : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_dac_vol(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "DAC_VOL";
        this->offset = 0xa;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x40;
    }
};

class vp_i2c_regmap_dac_clip : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_dac_clip(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "DAC_CLIP";
        this->offset = 0xb;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0xff;
    }
};

class vp_i2c_regmap_spt_ctrl1 : public vp::Register<uint8_t>
{
public:
    inline void spt_sai_mode_set(uint8_t value) { this->set_field(value, I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_BIT, I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_WIDTH); }
    inline uint8_t spt_sai_mode_get() { return this->get_field(I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_BIT, I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_WIDTH); }
    inline void spt_data_format_set(uint8_t value) { this->set_field(value, I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_BIT, I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_WIDTH); }
    inline uint8_t spt_data_format_get() { return this->get_field(I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_BIT, I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_WIDTH); }
    inline void spt_slot_width_set(uint8_t value) { this->set_field(value, I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_BIT, I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_WIDTH); }
    inline uint8_t spt_slot_width_get() { return this->get_field(I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_BIT, I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_WIDTH); }
    inline void spt_bclk_pol_set(uint8_t value) { this->set_field(value, I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_BIT, I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_WIDTH); }
    inline uint8_t spt_bclk_pol_get() { return this->get_field(I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_BIT, I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_WIDTH); }
    inline void spt_lrclk_pol_set(uint8_t value) { this->set_field(value, I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_BIT, I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_WIDTH); }
    inline uint8_t spt_lrclk_pol_get() { return this->get_field(I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_BIT, I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_WIDTH); }
    vp_i2c_regmap_spt_ctrl1(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "SPT_CTRL1";
        this->offset = 0xc;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x0;
        this->regfields.push_back(new vp::regfield("SPT_SAI_MODE", 0, 1));
        this->regfields.push_back(new vp::regfield("SPT_DATA_FORMAT", 1, 3));
        this->regfields.push_back(new vp::regfield("SPT_SLOT_WIDTH", 4, 2));
        this->regfields.push_back(new vp::regfield("SPT_BCLK_POL", 6, 1));
        this->regfields.push_back(new vp::regfield("SPT_LRCLK_POL", 7, 1));
    }
};

class vp_i2c_regmap_spt_ctrl2 : public vp::Register<uint8_t>
{
public:
    inline void spt_slot_sel_set(uint8_t value) { this->set_field(value, I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_BIT, I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_WIDTH); }
    inline uint8_t spt_slot_sel_get() { return this->get_field(I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_BIT, I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_WIDTH); }
    vp_i2c_regmap_spt_ctrl2(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "SPT_CTRL2";
        this->offset = 0xd;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x1f;
        this->reset_val = 0x0;
        this->regfields.push_back(new vp::regfield("SPT_SLOT_SEL", 0, 5));
    }
};

class vp_i2c_regmap_amp_ctrl : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_amp_ctrl(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "AMP_CTRL";
        this->offset = 0xe;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x14;
    }
};

class vp_i2c_regmap_lim_ctrl : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_lim_ctrl(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "LIM_CTRL";
        this->offset = 0xf;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x22;
    }
};

class vp_i2c_regmap_lim_ctrl2 : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_lim_ctrl2(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "LIM_CTRL2";
        this->offset = 0x10;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0xa;
    }
};

class vp_i2c_regmap_fault_ctrl : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_fault_ctrl(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "FAULT_CTRL";
        this->offset = 0x11;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x0;
    }
};

class vp_i2c_regmap_status_clr : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_status_clr(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "STATUS_CLR";
        this->offset = 0x12;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x0;
    }
};

class vp_i2c_regmap_status : public vp::Register<uint8_t>
{
public:
    vp_i2c_regmap_status(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "STATUS";
        this->offset = 0x13;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0xff;
        this->reset_val = 0x0;
    }
};

class vp_i2c_regmap_reset_reg : public vp::Register<uint8_t>
{
public:
    inline void soft_reset_set(uint8_t value) { this->set_field(value, I2C_REGMAP_RESET_REG_SOFT_RESET_BIT, I2C_REGMAP_RESET_REG_SOFT_RESET_WIDTH); }
    inline uint8_t soft_reset_get() { return this->get_field(I2C_REGMAP_RESET_REG_SOFT_RESET_BIT, I2C_REGMAP_RESET_REG_SOFT_RESET_WIDTH); }
    inline void soft_full_reset_set(uint8_t value) { this->set_field(value, I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_BIT, I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_WIDTH); }
    inline uint8_t soft_full_reset_get() { return this->get_field(I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_BIT, I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_WIDTH); }
    vp_i2c_regmap_reset_reg(vp::Block &top, std::string name) : vp::Register<uint8_t>(top, name, 8, true, 0)
    {
        this->name = "RESET_REG";
        this->offset = 0x14;
        this->width = 8;
        this->do_reset = 1;
        this->write_mask = 0x11;
        this->reset_val = 0x0;
        this->regfields.push_back(new vp::regfield("SOFT_RESET", 0, 1));
        this->regfields.push_back(new vp::regfield("SOFT_FULL_RESET", 4, 1));
    }
};


class vp_regmap_i2c_regmap : public vp::regmap
{
public:
    vp_i2c_regmap_vendor_id vendor_id;
    vp_i2c_regmap_device_id1 device_id1;
    vp_i2c_regmap_device_id2 device_id2;
    vp_i2c_regmap_revision revision;
    vp_i2c_regmap_pwr_ctrl pwr_ctrl;
    vp_i2c_regmap_clk_ctrl clk_ctrl;
    vp_i2c_regmap_pdm_ctrl pdm_ctrl;
    vp_i2c_regmap_dac_ctrl1 dac_ctrl1;
    vp_i2c_regmap_dac_ctrl2 dac_ctrl2;
    vp_i2c_regmap_dac_ctrl3 dac_ctrl3;
    vp_i2c_regmap_dac_vol dac_vol;
    vp_i2c_regmap_dac_clip dac_clip;
    vp_i2c_regmap_spt_ctrl1 spt_ctrl1;
    vp_i2c_regmap_spt_ctrl2 spt_ctrl2;
    vp_i2c_regmap_amp_ctrl amp_ctrl;
    vp_i2c_regmap_lim_ctrl lim_ctrl;
    vp_i2c_regmap_lim_ctrl2 lim_ctrl2;
    vp_i2c_regmap_fault_ctrl fault_ctrl;
    vp_i2c_regmap_status_clr status_clr;
    vp_i2c_regmap_status status;
    vp_i2c_regmap_reset_reg reset_reg;
    vp_regmap_i2c_regmap(vp::Block &top, std::string name): vp::regmap(top, name),
        vendor_id(top, "vendor_id"),
        device_id1(top, "device_id1"),
        device_id2(top, "device_id2"),
        revision(top, "revision"),
        pwr_ctrl(top, "pwr_ctrl"),
        clk_ctrl(top, "clk_ctrl"),
        pdm_ctrl(top, "pdm_ctrl"),
        dac_ctrl1(top, "dac_ctrl1"),
        dac_ctrl2(top, "dac_ctrl2"),
        dac_ctrl3(top, "dac_ctrl3"),
        dac_vol(top, "dac_vol"),
        dac_clip(top, "dac_clip"),
        spt_ctrl1(top, "spt_ctrl1"),
        spt_ctrl2(top, "spt_ctrl2"),
        amp_ctrl(top, "amp_ctrl"),
        lim_ctrl(top, "lim_ctrl"),
        lim_ctrl2(top, "lim_ctrl2"),
        fault_ctrl(top, "fault_ctrl"),
        status_clr(top, "status_clr"),
        status(top, "status"),
        reset_reg(top, "reset_reg")
    {
        this->registers_new.push_back(&this->vendor_id);
        this->registers_new.push_back(&this->device_id1);
        this->registers_new.push_back(&this->device_id2);
        this->registers_new.push_back(&this->revision);
        this->registers_new.push_back(&this->pwr_ctrl);
        this->registers_new.push_back(&this->clk_ctrl);
        this->registers_new.push_back(&this->pdm_ctrl);
        this->registers_new.push_back(&this->dac_ctrl1);
        this->registers_new.push_back(&this->dac_ctrl2);
        this->registers_new.push_back(&this->dac_ctrl3);
        this->registers_new.push_back(&this->dac_vol);
        this->registers_new.push_back(&this->dac_clip);
        this->registers_new.push_back(&this->spt_ctrl1);
        this->registers_new.push_back(&this->spt_ctrl2);
        this->registers_new.push_back(&this->amp_ctrl);
        this->registers_new.push_back(&this->lim_ctrl);
        this->registers_new.push_back(&this->lim_ctrl2);
        this->registers_new.push_back(&this->fault_ctrl);
        this->registers_new.push_back(&this->status_clr);
        this->registers_new.push_back(&this->status);
        this->registers_new.push_back(&this->reset_reg);
    }
};

#endif

#endif
