# ADS1115 Wokwi Model

Simulation model for the Texas Instruments **ADS1115**, a 16-bit Analog-to-Digital Converter (ADC) with Internal Reference and Programmable Gain Amplifier (PGA).

## Features

*   **Communication**: I2C Interface (Default Address `0x48`).
*   **Resolution**: 16-bit simulation.
*   **Inputs**:
    *   `AIN0`, `AIN1`, `AIN2`, `AIN3`: Analog input pins.
*   **Features**:
    *   Simulates conversion logic.
    *   Configurable input voltage sources via Wokwi diagram.

## Usage

Connect analog signals (potentiometers, sensors) to `AIN0-3`. The model simulates the conversion process and returns values based on the PGA amplification settings in the config register.

### Pins

| Pin | Description |
|-----|-------------|
| VCC | Power Supply |
| GND | Ground |
| SCL | I2C Clock |
| SDA | I2C Data |
| AIN0 | Analog Input 0 |
| AIN1 | Analog Input 1 |
| AIN2 | Analog Input 2 |
| AIN3 | Analog Input 3 |
| ADDR | Address Select (Currently fixed at `0x48` in simulation) |
| ALRT | Alert/Ready Pin (Simulation state varies) |
