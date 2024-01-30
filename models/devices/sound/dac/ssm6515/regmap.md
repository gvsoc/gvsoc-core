
# SSM6515



## description
Dac

## Registers

| Register Name | Offset | Size | Register Type | Host Access Type | Block Access Type | Default | Description                                           |
| ------------- | ------ | ---- | ------------- | ---------------- | ----------------- | ------- | ----------------------------------------------------- |
| VENDOR_ID     | 0x00   | 8    | Config        | R                | RW                | 0x41    | Analog Devices, Inc., Vendor ID                       |
| DEVICE_ID1    | 0x01   | 8    | Config        | R                | RW                | 0x65    | Device ID 1                                           |
| DEVICE_ID2    | 0x02   | 8    | Config        | R                | RW                | 0x15    | Device ID 2                                           |
| REVISION      | 0x03   | 8    | Config        | R                | RW                | 0x02    | Revision ID                                           |
| PWR_CTRL      | 0x04   | 8    | Config        | RW               | RW                | 0x01    | Master and block power control                        |
| CLK_CTRL      | 0x05   | 8    | Config        | RW               | RW                | 0x00    | Bclk rate control                                     |
| PDM_CTRL      | 0x06   | 8    | Config        | RW               | RW                | 0x20    | PDM control                                           |
| DAC_CTRL1     | 0x07   | 8    | Config        | RW               | RW                | 0x15    | Dac sample rate, power modes, filtering control       |
| DAC_CTRL2     | 0x08   | 8    | Config        | RW               | RW                | 0x13    | DAC mute and volume options                           |
| DAC_CTRL3     | 0x09   | 8    | Config        | RW               | RW                | 0xD0    | DAC high-pass filter controls                         |
| DAC_VOL       | 0x0A   | 8    | Config        | RW               | RW                | 0x40    | DAC volume control                                    |
| DAC_CLIP      | 0x0B   | 8    | Config        | RW               | RW                | 0xFF    | DAC clipper control                                   |
| SPT_CTRL1     | 0x0C   | 8    | Config        | RW               | RW                | 0x00    | Serial audio port settings                            |
| SPT_CTRL2     | 0x0D   | 8    | Config        | RW               | RW                | 0x00    | Serial audio port slot selection                      |
| AMP_CTRL      | 0x0E   | 8    | Config        | RW               | RW                | 0x14    | Amplifier controls                                    |
| LIM_CTRL      | 0x0F   | 8    | Config        | RW               | RW                | 0x22    | Audio output limiter attack and release rate controls |
| LIM_CTRL2     | 0x10   | 8    | Config        | RW               | RW                | 0x0A    | Audio output limiter threshold control                |
| FAULT_CTRL    | 0x11   | 8    | Config        | RW               | RW                | 0x00    | Fault control                                         |
| STATUS_CLR    | 0x12   | 8    | Status        | RW               | RW                | 0x00    | Status clear                                          |
| STATUS        | 0x13   | 8    | Status        | R                | RW                | 0x00    | Amplifier status                                      |
| RESET_REG     | 0x14   | 8    | Reset         | W                | RW                | 0x00    | Soft reset                                            |

### VENDOR_ID

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description                     |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | ------------------------------- |
| VENDOR     | 0      | 8    | R                | RW                | 0x41    | Analog Devices, Inc., Vendor ID |



### DEVICE_ID1

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | ----------- |
| DEVICE1    | 0      | 8    | R                | RW                | 0x65    | Device ID 1 |


### DEVICE_ID2

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | ----------- |
| DEVICE2    | 0      | 8    | R                | RW                | 0x15    | Device ID 2 |

### REVISION

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | ----------- |
| Rev        | 0      | 8    | R                | RW                | 0x2     | Revision ID |


### PWR_CTRL

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description                 |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | --------------------------- |
| SPWDN      | 0      | 1    | RW               | RW                | 0x1     | Send event to clear TX FIFO |
| APWDN_EN   | 1      | 1    | RW               | RW                | 0x0     | Send event to clear RX FIFO |
| LIM_EN     | 4      | 1    | RW               | RW                | 0x0     | Send event to reset RX FSM  |


### CLK_CTRL

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description                 |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | --------------------------- |
| BCLK_RATE  | 0      | 5    | RW               | RW                | 0x0     | Send event to clear TX FIFO |


### PDM_CTRL

#### Fields

| Field Name   | Offset | Size | Host Access Type | Block Access Type | Default | Description                  |
| ------------ | ------ | ---- | ---------------- | ----------------- | ------- | ---------------------------- |
| PDM_MODE     | 0      | 1    | RW               | RW                | 0x0     | PDM or PCM input mode        |
| PDM_FS       | 1      | 2    | RW               | RW                | 0x0     | PDM Sample Rate Selection    |
| PDM_CHAN_SEL | 3      | 1    | RW               | RW                | 0x0     | PDM Channel Select           |
| PDM_FILT     | 4      | 2    | RW               | RW                | 0x2     | PDM Input Filtering Control. |


### DAC_CTRL1

#### Fields

| Field Name   | Offset | Size | Host Access Type | Block Access Type | Default | Description                     |
| ------------ | ------ | ---- | ---------------- | ----------------- | ------- | ------------------------------- |
| DAC_FS       | 0      | 4    | RW               | RW                | 0x5     | DAC Path Sample Rate Selection. |
| DAC_PWR_MODE | 4      | 2    | RW               | RW                | 0x1     | DAC Power Mode                  |
| DAC_IBIAS    | 6      | 2    | RW               | RW                | 0x0     | DAC Bias Control                |


###  DAC_CTRL2

#### Fields

| Field Name    | Offset | Size | Host Access Type | Block Access Type | Default | Description                           |
| ------------- | ------ | ---- | ---------------- | ----------------- | ------- | ------------------------------------- |
| DAC_MUTE      | 0      | 1    | RW               | RW                | 0x1     | DAC Mute Control                      |
| DAC_VOL_MODE  | 1      | 2    | RW               | RW                | 0x1     | DAC Volume Control Bypass, Fixed Gain |
| DAC_MORE_FILT | 3      | 1    | RW               | RW                | 0x0     | DAC Additional Filtering              |
| DAC_VOL_ZC    | 4      | 1    | RW               | RW                | 0x1     | DAC Volume Zero Crossing Control      |
| DAC_HARD_VOL  | 5      | 1    | RW               | RW                | 0x0     | DAC Hard Volume                       |
| DAC_PERF_MODE | 6      | 1    | RW               | RW                | 0x0     | DAC High Performance Mode Enable      |
| DAC_INVERT    | 7      | 1    | RW               | RW                | 0x0     | DAC Signal Phase Inversion Enable     |


###  DAC_CTRL3

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description                           |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | ------------------------------------- |
| DAC_HPF_EN | 0      | 1    | RW               | RW                | 0x0     | DAC Channel 0 Enable High-Pass Filter |
| DAC_HPF_FC | 4      | 4    | RW               | RW                | 0x0     | DAC High-Pass Filter Cutoff Frequency |


###  SPT_CTRL1

#### Fields

| Field Name      | Offset | Size | Host Access Type | Block Access Type | Default | Description                            |
| --------------- | ------ | ---- | ---------------- | ----------------- | ------- | -------------------------------------- |
| SPT_SAI_MODE    | 0      | 1    | RW               | RW                | 0x0     | Serial Port–Selects Stereo or TDM Mode |
| SPT_DATA_FORMAT | 1      | 3    | RW               | RW                | 0x0     | Serial Port–Selects Data Format        |
| SPT_SLOT_WIDTH  | 4      | 2    | RW               | RW                | 0x0     | Serial Port–Selects TDM Slot Width     |
| SPT_BCLK_POL    | 6      | 1    | RW               | RW                | 0x0     | Serial Port–Selects BCLK Polarity      |
| SPT_LRCLK_POL   | 7      | 1    | RW               | RW                | 0x0     | Serial Port–Selects LRCLK Polarity     |


###  SPT_CTRL2

#### Fields

| Field Name   | Offset | Size | Host Access Type | Block Access Type | Default | Description                                   |
| ------------ | ------ | ---- | ---------------- | ----------------- | ------- | --------------------------------------------- |
| SPT_SLOT_SEL | 0      | 5    | RW               | RW                | 0x0     | Serial Port–Selects Slot/Channel Used For DAC |


###  AMPT_CTRL

#### Fields

| Field Name | Offset | Size | Host Access Type | Block Access Type | Default | Description                                  |
| ---------- | ------ | ---- | ---------------- | ----------------- | ------- | -------------------------------------------- |
| AMP_LPM    | 0      | 1    | RW               | RW                | 0x0     | Headphone Amplifier Low Power Mode Enable    |
| EMI_MODE   | 1      | 1    | RW               | RW                | 0x0     | EMI Mode                                     |
| AMP_RLOAD  | 2      | 2    | RW               | RW                | 0x1     | Headphone Amplifier Resistive Load Selection |
| OCP_EN     | 4      | 1    | RW               | RW                | 0x1     | Amplifier Overcurrent Protection Enable      |


###  RESET_REG

#### Fields

| Field Name      | Offset | Size | Host Access Type | Block Access Type | Default | Description                                    |
| --------------- | ------ | ---- | ---------------- | ----------------- | ------- | ---------------------------------------------- |
| SOFT_RESET      | 0      | 1    | RW               | RW                | 0x0     | Software Reset Not Including Register Settings |
| SOFT_FULL_RESET | 4      | 1    | RW               | RW                | 0x0     | Software Full Reset of Entire IC               |

