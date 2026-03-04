# ADS7142 Example – Wokwi Simulation

This example demonstrates how to use the custom **ADS7142** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the Texas Instruments ADS7142 nanopower dual-channel 12-bit ADC over I2C using its opcode-based protocol, configures manual mode with auto-sequencing on both channels, and reads conversion results in a loop.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Resets the device via the `WKEY` / `DEVICE_RESET` sequence.
3. Triggers offset calibration.
4. Configures the ADC:
   - Two-channel single-ended input (`CH0` + `CH1`).
   - Manual mode with auto-sequencing.
   - High-speed oscillator, nCLK = 21.
   - Output format 2: 12-bit ADC + channel ID + valid bit.
5. Enters a loop (every 2 seconds) that:
   - Starts a conversion sequence.
   - Reads 2 samples from the FIFO via `BLOCK_READ`.
   - Parses and logs the 12-bit ADC value and voltage for each channel.
   - Aborts the sequence before the next round.

## Interactive Controls

The Wokwi chip model provides sliders to set the input voltage on each analog channel during the simulation:

| Slider             | Range      | Step | Description              |
|--------------------|------------|------|--------------------------|
| AIN0 Voltage (V)   | 0 – 3.3    | 0.01 | Voltage applied to CH0   |
| AIN1 Voltage (V)   | 0 – 3.3    | 0.01 | Voltage applied to CH1   |

Initial values are set in `diagram.json` via the chip attributes (`"v0": "1.65"`, `"v1": "0.82"`).

## ADS7142 I2C Protocol

The ADS7142 uses an **opcode-based** protocol rather than plain register addressing:

| Opcode         | Value | Description                              |
|----------------|-------|------------------------------------------|
| SINGLE_WRITE   | 0x08  | Write one register: `[0x08][reg][data]`  |
| SINGLE_READ    | 0x10  | Read one register: TX `[0x10][reg]` → RX `[data]` |
| BLOCK_READ     | 0x30  | Read N samples from FIFO: TX `[0x30][N]` → RX `[N×2 bytes]` |

## Key Registers

| Register            | Address | Description                        |
|---------------------|---------|------------------------------------|
| OPMODE_STATUS       | 0x00    | Operating mode status              |
| DATA_BUFFER_STATUS  | 0x01    | FIFO buffer status                 |
| DEVICE_RESET        | 0x14    | Device reset (requires WKEY)       |
| OFFSET_CAL          | 0x15    | Trigger offset calibration         |
| WKEY                | 0x17    | Write key (unlock: 0x0A, lock: 0x00) |
| OSC_SEL             | 0x18    | Oscillator selection               |
| nCLK_SEL            | 0x19    | Conversion clock divider           |
| OPMODE_SEL          | 0x1C    | Operating mode selection           |
| START_SEQUENCE      | 0x1E    | Start conversion sequence          |
| ABORT_SEQUENCE      | 0x1F    | Abort conversion sequence          |
| AUTO_SEQ_CHEN       | 0x20    | Auto-sequence channel enable       |
| CHANNEL_INPUT_CFG   | 0x24    | Channel input configuration        |
| DOUT_FORMAT_CFG     | 0x28    | Data output format                 |

## Output Data Format (Format 2)

Each 16-bit sample: `[12-bit ADC value][3-bit channel ID][1-bit valid]`

Voltage conversion with $V_{REF} = 3.3\text{ V}$:

$$V = \frac{\text{adc\_value}}{4095} \times 3.3$$

## Wiring (diagram.json)

| ESP32-S3 Pin | ADS7142 Pin | Description       |
|--------------|-------------|-------------------|
| GPIO 4       | SDA         | I2C Data          |
| GPIO 5       | SCL         | I2C Clock         |
| GPIO 6       | ALERT       | Alert output (optional) |
| 3V3          | VDD         | Power Supply      |
| GND          | GND         | Ground            |

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/) (v5.x recommended)
- [Wokwi for VS Code](https://docs.wokwi.com/vscode/getting-started) extension

## How to Build

```bash
idf.py set-target esp32s3
idf.py build
```

## How to Run in Wokwi

1. Open this folder in VS Code with the Wokwi extension installed.
2. Press **F1** and select **Wokwi: Start Simulator**.
3. The custom chip model (`chip.wasm`) is referenced in `wokwi.toml` and loaded automatically.
4. Use the **AIN0 / AIN1 voltage sliders** in the Wokwi UI to adjust input voltages.
5. Observe the serial monitor output showing 12-bit ADC values and voltages for both channels.

## Expected Output

```
I (xxx) ads7142-example: Initializing I2C master...
I (xxx) ads7142-example: Resetting ADS7142...
I (xxx) ads7142-example: OPMODE_SEL = 0x04
I (xxx) ads7142-example: CHANNEL_INPUT_CFG = 0x03
I (xxx) ads7142-example: CH0: 2048 (1.650 V)  CH1: 1017 (0.819 V)
I (xxx) ads7142-example: CH0: 2048 (1.650 V)  CH1: 1017 (0.819 V)
...
```

## Project Structure

```
ads7142/
├── CMakeLists.txt        # Top-level CMake project file
├── diagram.json          # Wokwi wiring diagram
├── wokwi.toml            # Wokwi config (firmware path & custom chip reference)
├── sdkconfig.defaults    # Default: ESP32-S3 target
└── main/
    ├── CMakeLists.txt    # Component registration
    └── main.c            # Application entry point
```

## Configuration

| Define                | Value    | Description                        |
|-----------------------|----------|------------------------------------|
| `I2C_MASTER_SDA_IO`  | 4        | GPIO for I2C SDA                   |
| `I2C_MASTER_SCL_IO`  | 5        | GPIO for I2C SCL                   |
| `I2C_MASTER_FREQ_HZ` | 100000   | I2C clock frequency (100 kHz)      |
| `ADS7142_ADDR`        | 0x1F     | I2C address of the ADS7142         |
| `VREF`                | 3.3      | Reference voltage (V)              |
| `ADC_RESOLUTION`      | 4095     | 12-bit full-scale value            |

## License

See the repository root [LICENSE](../../LICENSE) for details.
