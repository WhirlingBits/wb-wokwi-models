# MAX17261 Wokwi Model

Simulation model for the Maxim Integrated **MAX17261**, a ModelGauge™ m5 EZ Fuel Gauge.

## Features

*   **Communication**: I2C Interface (Default Address `0x36`).
*   **Simulation**:
    *   Simulates ModelGauge m5 registers (`RepCap`, `RepSOC`, `VCell`, `Current`, `Temp`, etc.).
    *   Configurable battery parameters via Wokwi attributes.

## Usage

Use this model to simulate a Li-ion battery fuel gauge. You can adjust voltage and SOC dynamically to test your driver's response to low-battery conditions or charging states.

### Pins

| Pin | Description |
|-----|-------------|
| VCC | Power Supply |
| GND | Ground |
| SCL | I2C Clock |
| SDA | I2C Data |
