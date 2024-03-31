
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

#ifndef __ARCHI_I2C_REGMAP_REGFIELD__
#define __ARCHI_I2C_REGMAP_REGFIELD__

#if !defined(LANGUAGE_ASSEMBLY) && !defined(__ASSEMBLER__)


#endif




//
// REGISTERS FIELDS
//

// Analog Devices, Inc., Vendor ID (access: R)
#define I2C_REGMAP_VENDOR_ID_VENDOR_BIT                              0
#define I2C_REGMAP_VENDOR_ID_VENDOR_WIDTH                            8
#define I2C_REGMAP_VENDOR_ID_VENDOR_MASK                             0xff
#define I2C_REGMAP_VENDOR_ID_VENDOR_RESET                            0x41

// Device ID 1 (access: R)
#define I2C_REGMAP_DEVICE_ID1_DEVICE1_BIT                            0
#define I2C_REGMAP_DEVICE_ID1_DEVICE1_WIDTH                          8
#define I2C_REGMAP_DEVICE_ID1_DEVICE1_MASK                           0xff
#define I2C_REGMAP_DEVICE_ID1_DEVICE1_RESET                          0x65

// Device ID 2 (access: R)
#define I2C_REGMAP_DEVICE_ID2_DEVICE2_BIT                            0
#define I2C_REGMAP_DEVICE_ID2_DEVICE2_WIDTH                          8
#define I2C_REGMAP_DEVICE_ID2_DEVICE2_MASK                           0xff
#define I2C_REGMAP_DEVICE_ID2_DEVICE2_RESET                          0x15

// Revision ID (access: R)
#define I2C_REGMAP_REVISION_REV_BIT                                  0
#define I2C_REGMAP_REVISION_REV_WIDTH                                8
#define I2C_REGMAP_REVISION_REV_MASK                                 0xff
#define I2C_REGMAP_REVISION_REV_RESET                                0x2

// Send event to clear TX FIFO (access: RW)
#define I2C_REGMAP_PWR_CTRL_SPWDN_BIT                                0
#define I2C_REGMAP_PWR_CTRL_SPWDN_WIDTH                              1
#define I2C_REGMAP_PWR_CTRL_SPWDN_MASK                               0x1
#define I2C_REGMAP_PWR_CTRL_SPWDN_RESET                              0x1

// Send event to clear RX FIFO (access: RW)
#define I2C_REGMAP_PWR_CTRL_APWDN_EN_BIT                             1
#define I2C_REGMAP_PWR_CTRL_APWDN_EN_WIDTH                           1
#define I2C_REGMAP_PWR_CTRL_APWDN_EN_MASK                            0x2
#define I2C_REGMAP_PWR_CTRL_APWDN_EN_RESET                           0x0

// Send event to reset RX FSM (access: RW)
#define I2C_REGMAP_PWR_CTRL_LIM_EN_BIT                               4
#define I2C_REGMAP_PWR_CTRL_LIM_EN_WIDTH                             1
#define I2C_REGMAP_PWR_CTRL_LIM_EN_MASK                              0x10
#define I2C_REGMAP_PWR_CTRL_LIM_EN_RESET                             0x0

// Send event to clear TX FIFO (access: RW)
#define I2C_REGMAP_CLK_CTRL_BCLK_RATE_BIT                            0
#define I2C_REGMAP_CLK_CTRL_BCLK_RATE_WIDTH                          5
#define I2C_REGMAP_CLK_CTRL_BCLK_RATE_MASK                           0x1f
#define I2C_REGMAP_CLK_CTRL_BCLK_RATE_RESET                          0x0

// PDM or PCM input mode (access: RW)
#define I2C_REGMAP_PDM_CTRL_PDM_MODE_BIT                             0
#define I2C_REGMAP_PDM_CTRL_PDM_MODE_WIDTH                           1
#define I2C_REGMAP_PDM_CTRL_PDM_MODE_MASK                            0x1
#define I2C_REGMAP_PDM_CTRL_PDM_MODE_RESET                           0x0

// PDM Sample Rate Selection (access: RW)
#define I2C_REGMAP_PDM_CTRL_PDM_FS_BIT                               1
#define I2C_REGMAP_PDM_CTRL_PDM_FS_WIDTH                             2
#define I2C_REGMAP_PDM_CTRL_PDM_FS_MASK                              0x6
#define I2C_REGMAP_PDM_CTRL_PDM_FS_RESET                             0x0

// PDM Channel Select (access: RW)
#define I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_BIT                         3
#define I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_WIDTH                       1
#define I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_MASK                        0x8
#define I2C_REGMAP_PDM_CTRL_PDM_CHAN_SEL_RESET                       0x0

// PDM Input Filtering Control. (access: RW)
#define I2C_REGMAP_PDM_CTRL_PDM_FILT_BIT                             4
#define I2C_REGMAP_PDM_CTRL_PDM_FILT_WIDTH                           2
#define I2C_REGMAP_PDM_CTRL_PDM_FILT_MASK                            0x30
#define I2C_REGMAP_PDM_CTRL_PDM_FILT_RESET                           0x2

// DAC Path Sample Rate Selection. (access: RW)
#define I2C_REGMAP_DAC_CTRL1_DAC_FS_BIT                              0
#define I2C_REGMAP_DAC_CTRL1_DAC_FS_WIDTH                            4
#define I2C_REGMAP_DAC_CTRL1_DAC_FS_MASK                             0xf
#define I2C_REGMAP_DAC_CTRL1_DAC_FS_RESET                            0x5

// DAC Power Mode (access: RW)
#define I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_BIT                        4
#define I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_WIDTH                      2
#define I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_MASK                       0x30
#define I2C_REGMAP_DAC_CTRL1_DAC_PWR_MODE_RESET                      0x1

// DAC Bias Control (access: RW)
#define I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_BIT                           6
#define I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_WIDTH                         2
#define I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_MASK                          0xc0
#define I2C_REGMAP_DAC_CTRL1_DAC_IBIAS_RESET                         0x0

// DAC Mute Control (access: RW)
#define I2C_REGMAP_DAC_CTRL2_DAC_MUTE_BIT                            0
#define I2C_REGMAP_DAC_CTRL2_DAC_MUTE_WIDTH                          1
#define I2C_REGMAP_DAC_CTRL2_DAC_MUTE_MASK                           0x1
#define I2C_REGMAP_DAC_CTRL2_DAC_MUTE_RESET                          0x1

// DAC Volume Control Bypass, Fixed Gain (access: RW)
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_BIT                        1
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_WIDTH                      2
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_MASK                       0x6
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_MODE_RESET                      0x1

// DAC Additional Filtering (access: RW)
#define I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_BIT                       3
#define I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_WIDTH                     1
#define I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_MASK                      0x8
#define I2C_REGMAP_DAC_CTRL2_DAC_MORE_FILT_RESET                     0x0

// DAC Volume Zero Crossing Control (access: RW)
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_BIT                          4
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_WIDTH                        1
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_MASK                         0x10
#define I2C_REGMAP_DAC_CTRL2_DAC_VOL_ZC_RESET                        0x1

// DAC Hard Volume (access: RW)
#define I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_BIT                        5
#define I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_WIDTH                      1
#define I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_MASK                       0x20
#define I2C_REGMAP_DAC_CTRL2_DAC_HARD_VOL_RESET                      0x0

// DAC High Performance Mode Enable (access: RW)
#define I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_BIT                       6
#define I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_WIDTH                     1
#define I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_MASK                      0x40
#define I2C_REGMAP_DAC_CTRL2_DAC_PERF_MODE_RESET                     0x0

// DAC Signal Phase Inversion Enable (access: RW)
#define I2C_REGMAP_DAC_CTRL2_DAC_INVERT_BIT                          7
#define I2C_REGMAP_DAC_CTRL2_DAC_INVERT_WIDTH                        1
#define I2C_REGMAP_DAC_CTRL2_DAC_INVERT_MASK                         0x80
#define I2C_REGMAP_DAC_CTRL2_DAC_INVERT_RESET                        0x0

// DAC Channel 0 Enable High-Pass Filter (access: RW)
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_BIT                          0
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_WIDTH                        1
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_MASK                         0x1
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_EN_RESET                        0x0

// DAC High-Pass Filter Cutoff Frequency (access: RW)
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_BIT                          4
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_WIDTH                        4
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_MASK                         0xf0
#define I2C_REGMAP_DAC_CTRL3_DAC_HPF_FC_RESET                        0x0

// Serial PortSelects Stereo or TDM Mode (access: RW)
#define I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_BIT                        0
#define I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_WIDTH                      1
#define I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_MASK                       0x1
#define I2C_REGMAP_SPT_CTRL1_SPT_SAI_MODE_RESET                      0x0

// Serial PortSelects Data Format (access: RW)
#define I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_BIT                     1
#define I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_WIDTH                   3
#define I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_MASK                    0xe
#define I2C_REGMAP_SPT_CTRL1_SPT_DATA_FORMAT_RESET                   0x0

// Serial PortSelects TDM Slot Width (access: RW)
#define I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_BIT                      4
#define I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_WIDTH                    2
#define I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_MASK                     0x30
#define I2C_REGMAP_SPT_CTRL1_SPT_SLOT_WIDTH_RESET                    0x0

// Serial PortSelects BCLK Polarity (access: RW)
#define I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_BIT                        6
#define I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_WIDTH                      1
#define I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_MASK                       0x40
#define I2C_REGMAP_SPT_CTRL1_SPT_BCLK_POL_RESET                      0x0

// Serial PortSelects LRCLK Polarity (access: RW)
#define I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_BIT                       7
#define I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_WIDTH                     1
#define I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_MASK                      0x80
#define I2C_REGMAP_SPT_CTRL1_SPT_LRCLK_POL_RESET                     0x0

// Serial PortSelects Slot/Channel Used For DAC (access: RW)
#define I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_BIT                        0
#define I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_WIDTH                      5
#define I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_MASK                       0x1f
#define I2C_REGMAP_SPT_CTRL2_SPT_SLOT_SEL_RESET                      0x0

// Software Reset Not Including Register Settings (access: RW)
#define I2C_REGMAP_RESET_REG_SOFT_RESET_BIT                          0
#define I2C_REGMAP_RESET_REG_SOFT_RESET_WIDTH                        1
#define I2C_REGMAP_RESET_REG_SOFT_RESET_MASK                         0x1
#define I2C_REGMAP_RESET_REG_SOFT_RESET_RESET                        0x0

// Software Full Reset of Entire IC (access: RW)
#define I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_BIT                     4
#define I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_WIDTH                   1
#define I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_MASK                    0x10
#define I2C_REGMAP_RESET_REG_SOFT_FULL_RESET_RESET                   0x0

#endif
