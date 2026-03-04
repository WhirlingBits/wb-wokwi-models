# EZO-pH Example – Wokwi Simulation

This example demonstrates how to use the custom **Atlas Scientific EZO-pH** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the EZO™ pH circuit over I2C using the standard ASCII command protocol, performs device identification, calibration, and continuously reads pH values.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Queries device info (`"I"` command).
3. Queries calibration status (`"Cal,?"` command).
4. Performs a mid-point calibration at pH 7.00 (`"Cal,mid,7.00"` command).
5. Verifies calibration status and reads probe slope (`"Slope,?"` command).
6. Enters a loop that sends a read command (`"R"`) every 2 seconds and logs the returned pH value.

## EZO I2C ASCII Protocol

The EZO-pH uses an ASCII-based I2C protocol:

1. **Send**: Write the command string (e.g., `"R"`) to the device address.
2. **Wait**: Allow processing time (300 ms for info commands, ~900 ms for readings).
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
| `R`               | 900 ms  | Take a pH reading                    |
| `I`               | 300 ms  | Device information                   |
| `Cal,?`           | 300 ms  | Query calibration point count        |
| `Cal,mid,<value>` | 900 ms  | Mid-point calibration                |
| `Cal,low,<value>` | 900 ms  | Low-point calibration                |
| `Cal,high,<value>`| 900 ms  | High-point calibration               |
| `Slope,?`         | 300 ms  | Query probe slope                    |
| `Status`          | 300 ms  | Device status                        |
| `Sleep`           | —       | Enter low-power sleep mode           |

## Interactive Controls

The Wokwi chip model provides a slider to set the simulated pH value:

| Slider                             | Range      | Step | Description                          |
|------------------------------------|------------|------|--------------------------------------|
| pH Keep-Alive (centi-pH)           | 0 – 1400   | 1    | pH × 100 (e.g., 700 = pH 7.00)      |

The initial value is set in `diagram.json` via the chip attribute (`"ph": "700"` → pH 7.00).

## Wiring (diagram.json)

| ESP32-S3 Pin | EZO-pH Pin | Description  |
|--------------|------------|--------------|
| GPIO 4       | TX-SDA     | I2C Data     |
| GPIO 5       | RX-SCL     | I2C Clock    |
| 3V3          | VCC        | Power Supply |
| GND          | GND        | Ground       |

> **Note**: The EZO-pH uses dual-purpose pins (`TX-SDA` / `RX-SCL`) that serve as UART TX/RX or I2C SDA/SCL. This example uses I2C mode.

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
4. Use the **pH slider** in the Wokwi UI to adjust the simulated pH value.
5. Observe the serial monitor output showing device info, calibration, and pH readings.

## Expected Output

```
I (xxx) ezo-ph-example: Initializing I2C master...
I (xxx) ezo-ph-example: Device Info: ?I,pH,2.0
I (xxx) ezo-ph-example: Calibration: ?Cal,0
I (xxx) ezo-ph-example: Performing mid-point calibration...
I (xxx) ezo-ph-example: Mid-point calibration done
I (xxx) ezo-ph-example: Calibration after mid: ?Cal,1
I (xxx) ezo-ph-example: Slope: ?Slope,99.9,100.0
I (xxx) ezo-ph-example: pH: 7.00
I (xxx) ezo-ph-example: pH: 7.00
...
```

## Project Structure

```
ezo-ph/
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
| `EZO_PH_ADDR`        | 0x63     | I2C address of the EZO-pH (99 dec) |
| `EZO_DELAY_PH_MS`    | 900      | Processing delay for pH readings   |
| `EZO_DELAY_GENERIC_MS`| 300     | Processing delay for info commands  |

## License

See the repository root [LICENSE](../../LICENSE) for details.
