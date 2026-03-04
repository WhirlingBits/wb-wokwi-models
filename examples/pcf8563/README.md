# PCF8563 Example – Wokwi Simulation

This example demonstrates how to use the custom **PCF8563** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the NXP PCF8563 Real-Time Clock over I2C, sets an initial date/time, and reads it back every second.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Scans the I2C bus and logs all detected devices.
3. Checks the **VL (Voltage Low) bit** in the seconds register to verify clock integrity.
4. Clears both control/status registers.
5. Sets the time to **12:30:00, Tuesday, March 3rd, 2026**.
6. Enters a loop that reads all 7 time registers (seconds through year) every second and logs the current date/time.

## PCF8563 Register Map

The PCF8563 uses standard sequential register addressing. Time values are stored in **BCD** format.

| Register        | Address | Description                          |
|-----------------|---------|--------------------------------------|
| Control/Status 1| 0x00    | Control register 1                   |
| Control/Status 2| 0x01    | Control register 2                   |
| Seconds         | 0x02    | Seconds (BCD), bit 7 = VL flag      |
| Minutes         | 0x03    | Minutes (BCD)                        |
| Hours           | 0x04    | Hours (BCD, 24h format)              |
| Days            | 0x05    | Day of month (BCD)                   |
| Weekdays        | 0x06    | Day of week (0–6)                    |
| Months          | 0x07    | Month (BCD), bit 7 = century flag    |
| Years           | 0x08    | Year (BCD, 0–99)                     |

## Wiring (diagram.json)

| ESP32-S3 Pin | PCF8563 Pin | Description  |
|--------------|-------------|--------------|
| GPIO 4       | SDA         | I2C Data     |
| GPIO 5       | SCL         | I2C Clock    |
| 3V3          | VDD         | Power Supply |
| GND          | VSS         | Ground       |

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
4. Observe the serial monitor output showing the PCF8563 time incrementing every second.

## Expected Output

```
I (xxx) pcf8563-example: I2C initialized successfully
I (xxx) pcf8563-example: Found device at: 0x51
I (xxx) pcf8563-example: PCF8563 Example Started - reading time every second
I (xxx) pcf8563-example: Time: 2026-03-03 (W2) 12:30:01
I (xxx) pcf8563-example: Time: 2026-03-03 (W2) 12:30:02
...
```

## Project Structure

```
pcf8563/
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
| `PCF8563_ADDR`        | 0x51     | I2C address of the PCF8563 RTC     |

## License

See the repository root [LICENSE](../../LICENSE) for details.
