# TCA9554 Wokwi Model

Simulation model for the **TCA9554** (and compatible PCA9554) 8-bit I/O Expander.

## Features

*   **Communication**: I2C Interface (Address configurable, default usually `0x38`).
*   **Channels**: 8 General Purpose I/O pins (`P0` - `P7`).
*   **Registers**:
    *   Input Port
    *   Output Port
    *   Polarity Inversion
    *   Configuration

## Usage

Expand your microcontroller's GPIO capabilities. In Wokwi simulation, you can connect LEDs, buttons, or other logic to pins `P0`-`P7` and control them via I2C commands.

### Pins

| Pin | Description |
|-----|-------------|
| VCC | Power Supply |
| GND | Ground |
| SCL | I2C Clock |
| SDA | I2C Data |
| P0-P7 | I/O Pins |
| A0-A2 | Address Select Pins (Determine offset from base address) |
