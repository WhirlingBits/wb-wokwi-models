# TCA9554 Example – Wokwi Simulation

This example demonstrates how to use the custom **TCA9554** Wokwi chip model with an ESP32-S3 using ESP-IDF. It configures the TCA9554 8-bit I/O expander over I2C and toggles an LED connected to pin `P0`.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Scans the I2C bus and logs all detected devices.
3. Configures the TCA9554 at address `0x20` (A0=A1=A2=GND):
   - Pins `P0`–`P3` as **outputs**
   - Pins `P4`–`P7` as **inputs**
4. Enters a loop that:
   - Toggles `P0` every second (blinks the LED).
   - Reads back the full Input Port register and logs the value.

## Wiring (diagram.json)

| ESP32-S3 Pin | TCA9554 Pin | Description           |
|--------------|-------------|-----------------------|
| GPIO 4       | SDA         | I2C Data              |
| GPIO 5       | SCL         | I2C Clock             |
| 3V3          | VDD         | Power Supply          |
| GND          | GND         | Ground                |
| GND          | A0          | Address bit 0 → LOW   |
| GND          | A1          | Address bit 1 → LOW   |
| GND          | A2          | Address bit 2 → LOW   |

A red LED is connected between `P0` (anode) and GND (cathode).

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
4. Observe the LED blinking on `P0` and the I2C read-back values in the serial monitor.

## Project Structure

```
tca9554/
├── CMakeLists.txt        # Top-level CMake project file
├── diagram.json          # Wokwi wiring diagram
├── wokwi.toml            # Wokwi config (firmware path & custom chip reference)
├── sdkconfig.defaults    # Default: ESP32-S3 target
└── main/
    ├── CMakeLists.txt    # Component registration
    └── main.c            # Application entry point
```

## Configuration

| Define                | Value    | Description                            |
|-----------------------|----------|----------------------------------------|
| `I2C_MASTER_SDA_IO`  | 4        | GPIO for I2C SDA                       |
| `I2C_MASTER_SCL_IO`  | 5        | GPIO for I2C SCL                       |
| `I2C_MASTER_FREQ_HZ` | 100000   | I2C clock frequency (100 kHz)          |
| `TCA9554_ADDR`        | 0x20     | I2C slave address (A0=A1=A2=GND)      |

## License

See the repository root [LICENSE](../../LICENSE) for details.
