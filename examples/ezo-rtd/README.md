# EZO-RTD Example – Wokwi Simulation

This example demonstrates how to use the custom **Atlas Scientific EZO-RTD** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the EZO™ RTD temperature circuit over I2C using the standard ASCII command protocol, queries device info and calibration status, and continuously reads temperature values.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Queries device info (`"I"` command).
3. Queries calibration status (`"Cal,?"` command).
4. Enters a loop that sends a read command (`"R"`) every 2 seconds and logs the returned temperature value.

## EZO I2C ASCII Protocol

The EZO-RTD uses the same ASCII-based I2C protocol as all Atlas Scientific EZO circuits:

1. **Send**: Write the command string (e.g., `"R"`) to the device address.
2. **Wait**: Allow processing time (300 ms for info commands, ~600 ms for temperature readings).
3. **Read**: Read the response — the first byte is a status code, followed by the ASCII payload.

### Response Status Codes

| Code | Meaning                        |
|------|--------------------------------|
| 1    | Success — data follows         |
| 2    | Command failed                 |
| 254  | Still processing (pending)     |
| 255  | No data available              |

### Supported Commands

| Command           | Delay   | Description                          |
|-------------------|---------|--------------------------------------|
| `R`               | 600 ms  | Take a temperature reading           |
| `I`               | 300 ms  | Device information                   |
| `Cal,?`           | 300 ms  | Query calibration status             |
| `Cal,<value>`     | 600 ms  | Single-point calibration             |
| `Status`          | 300 ms  | Device status                        |
| `Sleep`           | —       | Enter low-power sleep mode           |

## Interactive Controls

The Wokwi chip model provides a slider to set the simulated temperature:

| Slider                                    | Range             | Step | Description                                |
|-------------------------------------------|-------------------|------|--------------------------------------------|
| Temperature (centi-degrees)               | -20000 – 120000   | 1    | Temperature × 100 (e.g., 2500 = 25.00 °C) |

The initial value is set in `diagram.json` via the chip attribute (`"temperature": "2500"` → 25.00 °C).

## Wiring (diagram.json)

| ESP32-S3 Pin | EZO-RTD Pin | Description  |
|--------------|-------------|--------------|
| GPIO 4       | TX-SDA      | I2C Data     |
| GPIO 5       | RX-SCL      | I2C Clock    |
| 3V3          | VCC         | Power Supply |
| GND          | GND         | Ground       |

> **Note**: The EZO-RTD uses dual-purpose pins (`TX-SDA` / `RX-SCL`) that serve as UART TX/RX or I2C SDA/SCL. This example uses I2C mode.

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
4. Use the **temperature slider** in the Wokwi UI to adjust the simulated temperature.
5. Observe the serial monitor output showing device info and temperature readings.

## Expected Output

```
I (xxx) ezo-rtd-example: Initializing I2C master...
I (xxx) ezo-rtd-example: Device Info: ?I,RTD,2.0
I (xxx) ezo-rtd-example: Calibration: ?Cal,0
I (xxx) ezo-rtd-example: Temperature: 25.00
I (xxx) ezo-rtd-example: Temperature: 25.00
...
```

## Project Structure

```
ezo-rtd/
├── CMakeLists.txt        # Top-level CMake project file
├── diagram.json          # Wokwi wiring diagram
├── wokwi.toml            # Wokwi config (firmware path & custom chip reference)
├── sdkconfig.defaults    # Default: ESP32-S3 target
└── main/
    ├── CMakeLists.txt    # Component registration
    └── main.c            # Application entry point
```

## Configuration

| Define                 | Value    | Description                           |
|------------------------|----------|---------------------------------------|
| `I2C_MASTER_SDA_IO`   | 4        | GPIO for I2C SDA                      |
| `I2C_MASTER_SCL_IO`   | 5        | GPIO for I2C SCL                      |
| `I2C_MASTER_FREQ_HZ`  | 100000   | I2C clock frequency (100 kHz)         |
| `EZO_RTD_ADDR`        | 0x66     | I2C address of the EZO-RTD (102 dec)  |
| `EZO_DELAY_RTD_MS`    | 600      | Processing delay for temperature reads|
| `EZO_DELAY_GENERIC_MS`| 300      | Processing delay for info commands    |

## License

See the repository root [LICENSE](../../LICENSE) for details.
