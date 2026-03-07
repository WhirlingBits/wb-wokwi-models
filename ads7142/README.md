# ADS7142 Wokwi Model

Simulation model for the Texas Instruments **ADS7142**, a nanopower, dual-channel, programmable sensor monitor and 12-bit ADC.

## Features

*   **Communication**: I2C Interface (Default Address `0x1F`, configurable via resistor selection `0x18`–`0x1F`).
*   **Resolution**: 12-bit ADC simulation.
*   **Opcode Protocol**: SINGLE_READ (`0x10`), SINGLE_WRITE (`0x08`), SET_BIT (`0x18`), CLEAR_BIT (`0x20`), BLOCK_READ (`0x30`), BLOCK_WRITE (`0x28`).
*   **Operating Modes**: Manual, Manual + Auto-Sequencing, Autonomous Monitoring, High-Precision.
*   **Data FIFO**: Up to 32 samples with configurable output format (raw 12-bit or Format 2 with channel ID + DATA_VALID).
*   **DATA_BUFFER_OPMODE**: Stop-Burst, Start-Burst, Pre-Alert, Post-Alert modes.
*   **Digital Window Comparator (DWC)**: Per-channel high/low thresholds with hysteresis and active-low ALERT pin.
*   **Pre-Alert Event Counter**: Configurable event count before ALERT fires.
*   **Accumulator**: Sums 1/4/8/16 samples per channel (configurable via ACC_EN bits [3:2]).
*   **Device Reset**: Via WKEY (`0x0A`) + DEVICE_RESET (`0x01`) sequence.
*   **Slider Controls**: Two voltage sliders (0–3.3 V) for AIN0 and AIN1.

## Usage

### Pins

| Pin   | Description          |
|-------|----------------------|
| VDD   | Power Supply         |
| GND   | Ground               |
| SCL   | I2C Clock            |
| SDA   | I2C Data             |
| ALERT | Alert Output (active-low) |
| AIN0  | Analog Input 0       |
| AIN1  | Analog Input 1       |

### Controls

| Control           | Range     | Description               |
|-------------------|-----------|---------------------------|
| AIN0 Voltage (V)  | 0 – 3.3  | Sets the voltage on CH0   |
| AIN1 Voltage (V)  | 0 – 3.3  | Sets the voltage on CH1   |

### Register Map

All registers from the ADS7142 datasheet are implemented. Key registers:

| Register               | Addr   | Description                        |
|------------------------|--------|------------------------------------|
| OPMODE_I2CMODE_STATUS  | `0x00` | Operating mode + I2C status        |
| DATA_BUFFER_STATUS     | `0x01` | Number of samples in FIFO          |
| ACCUMULATOR_STATUS     | `0x02` | Accumulator data-ready flags       |
| SEQUENCE_STATUS        | `0x04` | Sequence enabled/disabled          |
| OPMODE_SEL             | `0x1C` | Operating mode selection           |
| AUTO_SEQ_CHEN          | `0x20` | Channel enable for auto-sequencing |
| DOUT_FORMAT_CFG        | `0x28` | Output data format                 |
| DATA_BUFFER_OPMODE     | `0x2C` | FIFO buffering mode                |
| ACC_EN                 | `0x30` | Accumulator enable + sample count  |
| ALERT_DWC_EN           | `0x37` | DWC enable                         |
| DWC_HTH/LTH_CHx       | `0x38`–`0x3F` | DWC thresholds             |
| DWC_HYS_CHx            | `0x40`–`0x41` | DWC hysteresis             |
