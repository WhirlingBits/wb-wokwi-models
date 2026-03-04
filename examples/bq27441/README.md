# BQ27441-G1 Example – Wokwi Simulation

This example demonstrates how to use the custom **BQ27441-G1** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the Texas Instruments BQ27441-G1 Impedance Track™ fuel gauge over I2C, verifies the device type, and continuously reads a comprehensive set of battery parameters.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 100 kHz.
2. Reads and verifies the device type via the Control subcommand `0x0001` (expected `0x0421`).
3. Enters a loop (every 2 seconds) that reads:
   - **Voltage** (mV)
   - **Average Current** (mA, signed)
   - **State of Charge** (%)
   - **Temperature** (converted from 0.1 K to °C)
   - **Remaining / Full Charge Capacity** (mAh)
   - **Average Power** (mW)
   - **State of Health** (%)
   - **Flags** (DSG, CHG, FC)

## Interactive Controls

The Wokwi chip model provides sliders to dynamically adjust simulated battery parameters during the simulation:

| Slider              | Range          | Step | Description                        |
|---------------------|----------------|------|------------------------------------|
| State of Charge (%) | 0 – 100        | 1    | Simulated battery SOC              |
| Voltage (mV)        | 2500 – 4500    | 10   | Simulated cell voltage             |
| Current (mA)        | -2000 – 2000   | 10   | Simulated charge/discharge current |
| Temperature (°C)    | -40 – 85       | 1    | Simulated battery temperature      |

Initial values are set in `diagram.json` via chip attributes (`"soc": "75"`, `"voltage": "3800"`, `"current": "-250"`, `"temp": "25"`).

## Key Registers (Standard Commands)

| Register         | Address | Description                       |
|------------------|---------|-----------------------------------|
| Control          | 0x00    | Control subcommand interface      |
| Temperature      | 0x02    | Battery temperature (0.1 K)       |
| Voltage          | 0x04    | Measured voltage (mV)             |
| Flags            | 0x06    | Status flags                      |
| NomAvailCap      | 0x08    | Nominal available capacity (mAh)  |
| FullAvailCap     | 0x0A    | Full available capacity (mAh)     |
| RemainingCap     | 0x0C    | Remaining capacity (mAh)          |
| FullChgCap       | 0x0E    | Full charge capacity (mAh)        |
| AvgCurrent       | 0x10    | Average current (mA, signed)      |
| StbyCurrent      | 0x12    | Standby current (mA)              |
| MaxLoadCurrent   | 0x14    | Maximum load current (mA)         |
| AvgPower         | 0x18    | Average power (mW, signed)        |
| SOC              | 0x1C    | State of charge (%)               |
| IntTemp          | 0x1E    | Internal temperature (0.1 K)      |
| SOH              | 0x20    | State of health (%)               |

## Flag Bits

| Flag | Bit  | Description                    |
|------|------|--------------------------------|
| DSG  | 0    | Discharging detected           |
| CHG  | 8    | Fast charging allowed          |
| FC   | 9    | Full charge detected           |

## Control Subcommands

Subcommands are written as 16-bit little-endian values to register `0x00`, and the result is read back from the same register.

| Subcommand   | Value  | Description          |
|--------------|--------|----------------------|
| DEVICE_TYPE  | 0x0001 | Returns device ID (expected `0x0421`) |

## Wiring (diagram.json)

| ESP32-S3 Pin | BQ27441 Pin | Description               |
|--------------|-------------|---------------------------|
| GPIO 4       | SDA         | I2C Data                  |
| GPIO 5       | SCL         | I2C Clock                 |
| GPIO 6       | GPOUT       | Battery low alert output  |
| 3V3          | VDD         | Power Supply              |
| GND          | GND         | Ground                    |

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
4. Use the **interactive sliders** in the Wokwi UI to adjust SOC, voltage, current, and temperature.
5. Observe the serial monitor output showing live battery readings and flag states.

## Expected Output

```
I (xxx) bq27441-example: Initializing I2C master...
I (xxx) bq27441-example: Device Type: 0x0421 (BQ27441)
I (xxx) bq27441-example: V=3800mV  I=-250mA  SOC=75%  T=25.0°C  Cap=3750/5000mAh  P=-950mW  SOH=100%
I (xxx) bq27441-example: Flags: 0x0001 [DSG ]
...
```

## Project Structure

```
bq27441/
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
| `BQ27441_ADDR`        | 0x55     | I2C address of the BQ27441-G1      |

## License

See the repository root [LICENSE](../../LICENSE) for details.
