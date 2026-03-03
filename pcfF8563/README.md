# PCF8563 Wokwi Model

Simulation model for the NXP **PCF8563** Real-Time Clock (RTC) and Calendar.

## Features

*   **Communication**: I2C Interface (Default Address `0x51`).
*   **Simulation**:
    *   Timekeeping (Seconds, Minutes, Hours, Days, Weekdays, Months, Years).
    *   VL (Voltage Low) bit simulation.
*   **Synchronization**: By default, it may sync to the Wokwi simulation time or start from a fixed epoch depending on configuration.

## Usage

Standard I2C RTC. Connect to your microcontroller to test time-keeping drivers.

### Pins

| Pin | Description |
|-----|-------------|
| VCC | Power Supply |
| GND | Ground |
| SCL | I2C Clock |
| SDA | I2C Data |
| INT | Interrupt Output (Simulation of Alarm/Timer interrupts) |
