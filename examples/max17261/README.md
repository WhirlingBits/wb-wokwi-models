# MAX17261 Example – Wokwi Simulation

This example demonstrates how to use the custom **MAX17261** Wokwi chip model with an ESP32-S3 using ESP-IDF. It communicates with the Maxim Integrated MAX17261 ModelGauge™ m5 fuel gauge over I2C, performs the initial POR (Power-On Reset) setup sequence, and continuously reads battery state data.

## Overview

The firmware performs the following:

1. Initializes the I2C master on **GPIO 4** (SDA) and **GPIO 5** (SCL) at 400 kHz.
2. Scans the I2C bus and logs all detected devices.
3. Reads and verifies the device ID register (`0x21`, expected `0x4033`).
4. If POR is detected (Status register bit 1), runs the full initialization:
   - Waits for the DNR (Data Not Ready) bit in FSTAT to clear.
   - Exits hibernate mode.
   - Writes design capacity (5000 mAh) and triggers model refresh.
   - Restores hibernate config and clears the POR flag.
5. Enters a loop that reads key battery parameters every 2 seconds:
   - **State of Charge (SOC)**
   - **Cell Voltage**
   - **Current**
   - **Temperature**
   - **Remaining Capacity**
   - **Time to Empty (TTE)**

## Interactive Controls

The Wokwi chip model provides sliders to dynamically adjust simulated battery parameters during the simulation:

| Slider              | Range          | Step | Description                     |
|---------------------|----------------|------|---------------------------------|
| State of Charge (%) | 0 – 100        | 1    | Simulated battery SOC           |
| Current (mA)        | -2000 – 2000   | 10   | Simulated charge/discharge current |
| Temperature (°C)    | -40 – 85       | 1    | Simulated battery temperature   |

## Key Registers

| Register   | Address | Description                       |
|------------|---------|-----------------------------------|
| Status     | 0x00    | Status flags (POR, alerts, etc.)  |
| RepCap     | 0x05    | Reported remaining capacity       |
| RepSOC     | 0x06    | Reported state of charge          |
| Temp       | 0x08    | Temperature                       |
| VCell      | 0x09    | Cell voltage                      |
| Current    | 0x0A    | Instantaneous current             |
| AvgCurrent | 0x0B    | Average current                   |
| FullCap    | 0x10    | Full capacity                     |
| TTE        | 0x11    | Time to empty                     |
| DesignCap  | 0x18    | Design capacity                   |
| DevName    | 0x21    | Device ID                         |
| FSTAT      | 0x3D    | Fuel gauge status                 |
| HibCFG     | 0xBA    | Hibernate configuration           |
| ModelCFG   | 0xDB    | Model configuration / refresh     |

## Unit Conversion (Rsense = 10 mΩ)

| Parameter  | LSB Resolution         |
|------------|------------------------|
| Capacity   | 0.5 mAh / LSB         |
| Current    | 15.625 µA / LSB       |
| Voltage    | 0.078125 mV / LSB     |
| Time       | 5.625 s / LSB         |
| Temperature| 1/256 °C / LSB        |

## Wiring (diagram.json)

| ESP32-S3 Pin | MAX17261 Pin | Description  |
|--------------|--------------|--------------|
| GPIO 4       | SDA          | I2C Data     |
| GPIO 5       | SCL          | I2C Clock    |
| 3V3          | VDD          | Power Supply |
| GND          | GND          | Ground       |

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
4. Use the **interactive sliders** in the Wokwi UI to adjust SOC, current, and temperature.
5. Observe the serial monitor output showing live battery readings.

## Expected Output

```
I (xxx) max17261-example: I2C initialized successfully
I (xxx) max17261-example: Found device at: 0x36
I (xxx) max17261-example: MAX17261 Device ID: 0x4033 (expected 0x4033)
I (xxx) max17261-example: Setup complete
I (xxx) max17261-example: MAX17261 Example Started - reading battery data every 2 seconds
I (xxx) max17261-example: SOC: 75% | Voltage: 3800 mV | Current: -250.0 mA | Temp: 25.0 C | Cap: 3750 mAh | TTE: 900.0 min
...
```

## Project Structure

```
max17261/
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
| `I2C_MASTER_FREQ_HZ` | 400000   | I2C clock frequency (400 kHz)      |
| `MAX17261_ADDR`       | 0x36     | I2C address of the MAX17261        |
| `R_SENSE`             | 0.01     | Sense resistor value (10 mΩ)      |

## License

See the repository root [LICENSE](../../LICENSE) for details.
