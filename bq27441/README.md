# BQ27441-G1 Wokwi Model

Simulation model for the Texas Instruments **BQ27441-G1**, a System-Side Impedance Track™ Fuel Gauge.

## Features

*   **Communication**: I2C Interface (Fixed Address `0x55`).
*   **Standard Commands**: Voltage, Current, SOC, Temperature, Flags, Capacity, Power, SOH — all as 16-bit little-endian word reads.
*   **Control Subcommands**: DEVICE_TYPE (`0x0421`), FW_VERSION (`0x0109`), CHEM_ID (`0x0128`), CONTROL_STATUS, SET_CFGUPDATE, EXIT_CFGUPDATE, SOFT_RESET, RESET, SEALED, UNSEAL (two-step), TOGGLE_GPOUT, SET/CLEAR_HIBERNATE.
*   **Extended Data**: Full block read/write support for data classes 82 (State), 49 (Discharge), 64 (Registers/OpConfig) with checksum validation.
*   **Flags Register**: DSG, SOCF, SOC1, BAT_DET, CFGUPMODE, ITPOR, OCVTAKEN, CHG, FC, UT, OT — correct bit positions per TI datasheet.
*   **OpConfig**: Directly readable at `0x3A`, writable via extended data class 64.
*   **Design Capacity**: Directly readable at `0x3C`.
*   **GPOUT Pin**: Active-low battery alert output, toggleable via subcommand.
*   **Two-Step Unseal**: Requires writing `0x8000` twice (matches real hardware).
*   **ITPOR**: Power-On Reset flag set on init, cleared by SOFT_RESET.

## Controls

| Slider              | Range         | Unit | Description                    |
|---------------------|---------------|------|--------------------------------|
| State of Charge (%) | 0–100         | %    | Battery SOC                    |
| Voltage (mV)        | 2500–4500     | mV   | Battery voltage                |
| Current (mA)        | -2000 – 2000  | mA   | Charge (+) / Discharge (-)     |
| Temperature (C)     | -40 – 85      | °C   | Battery temperature            |

## Pins

| Pin   | Description                        |
|-------|------------------------------------|
| VDD   | Power Supply                       |
| GND   | Ground                             |
| SCL   | I2C Clock                          |
| SDA   | I2C Data                           |
| GPOUT | Battery alert output (active-low)  |
| BIN   | Battery insert detect              |
