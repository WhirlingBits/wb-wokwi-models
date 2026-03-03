# WhirlingBits Wokwi Models

This repository contains custom Wokwi simulation models for various electronic components, sensors, and ICs. These models are designed to be used with the [Wokwi Simulator](https://wokwi.com/) to simulate hardware behavior in a virtual environment.

## Available Models

| Model | Description | Protocol | Default I2C Address |
|-------|-------------|----------|---------------------|
| **[ADS1115](ads1115)** | 16-Bit ADC with PGA | I2C | `0x48` |
| **[ADS7142](ads7142)** | Low-Power 12-Bit ADC | I2C | `0x18` |
| **[BQ27441](bq27441)** | System-Side Fuel Gauge | I2C | `0x55` |
| **[EZO-pH](ezo-ph)** | Atlas Scientific pH Sensor | I2C | `0x63` (99) |
| **[EZO-RTD](ezo-rtd)** | Atlas Scientific RTD Temp Sensor | I2C | `0x66` (102) |
| **[MAX17261](max17261)** | ModelGauge m5 Fuel Gauge | I2C | `0x36` |
| **[PCF8563](PCF8563)** | Real-Time Clock / Calendar | I2C | `0x51` |
| **[R2221](r2221)** | Real-Time Clock (Ricoh/Nisshinbo) | I2C | `0x32` |
| **[TCA9554](tca9554)** | 8-Bit I/O Expander | I2C | `0x38` |

## Usage

To use these models in your Wokwi project:

1.  **Clone the repository** (or copy the specific model folder).
2.  **Add the model** to your `diagram.json` as a custom chip.
3.  **Compile the model** (usually to a `.wasm` file using your build system or Wokwi extension).

### Directory Structure

Each model directory typically contains:
*   `chip.json`: Defines the chip's interface, pins, and controls.
*   `src/main.c`: The simulation logic written in C.
*   `CMakeLists.txt`: Build configuration for the simulation model.

## License

See the [LICENSE](LICENSE) file for details.
