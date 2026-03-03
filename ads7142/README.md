# ADS7142 Wokwi Model

Simulation model for the Texas Instruments **ADS7142**, a nanopower, dual-channel, programmable sensor monitor and 12-bit ADC.

## Features

*   **Communication**: I2C Interface (Default Address `0x18` usually, check `src/main.c`).
*   **Resolution**: 12-bit simulation.
*   **Attributes**:
    *   `channel0`, `channel1`: Simulation attributes to set voltage/value.

## Usage

This model simulates the register map and conversion behavior of the ADS7142.

### Pins

| Pin | Description |
|-----|-------------|
| VCC | Power Supply |
| GND | Ground |
| SCL | I2C Clock |
| SDA | I2C Data |
