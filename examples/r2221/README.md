# R2221 Example – Wokwi Simulation

This example demonstrates how to use the custom **R2221** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the Ricoh/Nisshinbo R2221 Real-Time Clock over I2C, sets an initial date/time, and reads it back every second.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Scans the I2C bus and logs all detected devices.
3. Configures the R2221 RTC at address `0x32`:
   - Clears power-on flags (Control 2 register).
   - Enables 24-hour mode (Control 1 register, bit 5).
4. Sets the time to **12:30:00, Monday, March 3rd, 2026**.
5. Enters a loop that reads all 7 time registers (seconds through year) every second and logs the current date/time.

## R2221 Register Access

The R2221 uses a shifted register addressing scheme — register addresses are sent as `(reg << 4)` on the I2C bus. The time registers store values in **BCD** format.

| Register | Address | Description      |
|----------|---------|------------------|
| Second   | 0x00    | Seconds (BCD)    |
| Minute   | 0x01    | Minutes (BCD)    |
| Hour     | 0x02    | Hours (BCD)      |
| Day/Week | 0x03    | Day of week      |
| Day/Month| 0x04    | Day of month     |
| Month    | 0x05    | Month (BCD)      |
| Year     | 0x06    | Year (BCD, 0–99) |
| Control 1| 0x0E    | Mode settings    |
| Control 2| 0x0F    | Status flags     |

## Wiring (diagram.json)

| ESP32-S3 Pin | R2221 Pin | Description  |
|--------------|-----------|--------------|
| GPIO 4       | SDA       | I2C Data     |
| GPIO 5       | SCL       | I2C Clock    |
| 3V3          | VDD       | Power Supply |
| GND          | GND       | Ground       |

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
4. Observe the serial monitor output showing the R2221 time incrementing every second.

## Expected Output

```
I (xxx) r2221-example: I2C initialized successfully
I (xxx) r2221-example: Found device at: 0x32
I (xxx) r2221-example: R2221 Example Started - reading time every second
I (xxx) r2221-example: Time: 2026-03-03 (W1) 12:30:01
I (xxx) r2221-example: Time: 2026-03-03 (W1) 12:30:02
...
```

## Project Structure

```
r2221/
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
| `R2221_ADDR`          | 0x32     | I2C address of the R2221 RTC       |

## License

See the repository root [LICENSE](../../LICENSE) for details.
