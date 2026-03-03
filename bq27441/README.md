# BQ27441-G1 Wokwi Model

Simulation model for the Texas Instruments **BQ27441-G1**, a System-Side Impedance Track™ Fuel Gauge.

## Features

*   **Communication**: I2C Interface (Default Address `0x55`).
*   **Simulation**:
    *   Simulates battery parameters: Voltage, State of Charge (SOC), Current, Temperature.
    *   Implements Standard Commands and Extended Data blocks.

## Controls

The simulation exposes Wokwi attributes/sliders to modify battery state in real-time:
*   **Voltage** (mV)
*   **State of Charge** (%)
*   **Current** (mA)
*   **Temperature** (0.1°K or °C depending on `chip.json`)

## Usage

Connect to your microcontroller's I2C bus to read battery telemetry. Great for testing power management logic without a real battery.
