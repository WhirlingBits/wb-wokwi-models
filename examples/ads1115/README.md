# ADS1115 Example – Wokwi Simulation

This example demonstrates how to use the custom **ADS1115** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the Texas Instruments ADS1115 16-bit ADC over I2C, configures it for continuous single-ended conversion on channel AIN0, and reads the analog value in a loop.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Scans the I2C bus and logs all detected devices.
3. Writes the config register (`0x01`) with `0xC283`:
   - **MUX**: AIN0 vs GND (single-ended)
   - **PGA**: ±4.096 V full-scale range
   - **MODE**: Continuous conversion
   - **DR**: 128 SPS
   - **COMP_QUE**: Comparator disabled
4. Enters a loop that reads the conversion register (`0x00`) every 500 ms, converts the raw 16-bit signed value to voltage, and logs both.

## Interactive Controls

The Wokwi chip model provides sliders to set the voltage on each analog input during the simulation:

| Slider           | Range    | Step | Description              |
|------------------|----------|------|--------------------------|
| A0 Voltage (V)   | 0 – 5    | 0.1  | Voltage applied to AIN0  |
| A1 Voltage (V)   | 0 – 5    | 0.1  | Voltage applied to AIN1  |
| A2 Voltage (V)   | 0 – 5    | 0.1  | Voltage applied to AIN2  |
| A3 Voltage (V)   | 0 – 5    | 0.1  | Voltage applied to AIN3  |

In this example only AIN0 is read, but you can modify the config register MUX bits to read other channels.

## Key Registers

| Register    | Address | Description                              |
|-------------|---------|------------------------------------------|
| Conversion  | 0x00    | 16-bit conversion result (read-only)     |
| Config      | 0x01    | Operating mode, MUX, PGA, data rate, etc.|
| Lo Thresh   | 0x02    | Low threshold for comparator             |
| Hi Thresh   | 0x03    | High threshold for comparator            |

### Config Register Breakdown (0xC283)

| Field    | Bits    | Value | Meaning                      |
|----------|---------|-------|------------------------------|
| OS       | 15      | 1     | Start single conversion      |
| MUX      | 14–12   | 100   | AIN0 vs GND                  |
| PGA      | 11–9    | 001   | ±4.096 V                     |
| MODE     | 8       | 0     | Continuous conversion         |
| DR       | 7–5     | 100   | 128 SPS                      |
| COMP_MODE| 4       | 0     | Traditional comparator        |
| COMP_POL | 3       | 0     | Active low                    |
| COMP_LAT | 2       | 0     | Non-latching                  |
| COMP_QUE | 1–0     | 11    | Disable comparator            |

## Voltage Conversion

With PGA = ±4.096 V:

$$V = \frac{\text{raw}}{32768} \times 4.096$$

## Wiring (diagram.json)

| ESP32-S3 Pin | ADS1115 Pin | Description         |
|--------------|-------------|---------------------|
| GPIO 4       | SDA         | I2C Data            |
| GPIO 5       | SCL         | I2C Clock           |
| 3V3          | VDD         | Power Supply        |
| GND          | GND         | Ground              |

A potentiometer (0–3.3 V) is connected to **AIN0** as an analog input source.

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
4. Adjust the **potentiometer** or use the **A0 voltage slider** in the Wokwi UI.
5. Observe the serial monitor output showing raw ADC values and corresponding voltages.

## Expected Output

```
I (xxx) ads1115-example: I2C initialized successfully
I (xxx) ads1115-example: Found device at: 0x48
I (xxx) ads1115-example: Configuring ADS1115 with 0xC283
I (xxx) ads1115-example: ADS1115 ESP-IDF Example Started
I (xxx) ads1115-example: Reading continuous values from AIN0...
I (xxx) ads1115-example: Raw: 16384    Voltage: 2.048 V
I (xxx) ads1115-example: Raw: 16384    Voltage: 2.048 V
...
```

## Project Structure

```
ads1115/
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
| `ADS1115_ADDR`        | 0x48     | I2C address of the ADS1115         |

## License

See the repository root [LICENSE](../../LICENSE) for details.
