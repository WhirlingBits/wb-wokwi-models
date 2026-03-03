# R2221 Wokwi Model

Simulation model for the Ricoh/Nisshinbo **R2221** Real-Time Clock (RTC).

## Features

*   **Communication**: I2C Interface (Address `0x32`).
*   **Simulation**:
    *   Full calendar/clock registers.
    *   Control registers.
*   **Note**: This chip is distinct from other common RTCs and uses its own register map.

## Usage

Use for simulating R2221-based timing circuits.

### Pins

| Pin | Description |
|-----|-------------|
| VCC | Power Supply |
| GND | Ground |
| SCL | I2C Clock |
| SDA | I2C Data |
