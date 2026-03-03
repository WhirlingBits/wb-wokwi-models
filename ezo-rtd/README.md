# Atlas Scientific EZO-RTD Wokwi Model

Simulation model for the **Atlas Scientific EZO™ RTD Temperature Circuit** in I2C mode.

## Features

*   **Communication**: I2C Interface (Default Address `0x66` / `102`).
*   **Protocol**: ASCII command protocol (EZO standard).
    *   Supports `R` (Read), `Cal`, `I` (Info), `Status`, `Sleep`.
    *   Simulates processing delays (`Pending` status 254).

## Controls

*   **Temperature**: A slider control allowing you to set the temperature.
    *   *Scale*: Centi-degrees Celsius (e.g., `2500` = 25.00°C).

## Usage

Send the command `"R"` to address `102` to trigger a reading. The device will return status code `254` (Pending) until the simulated processing time is complete, after which it returns `1` (Success) followed by the ASCII temperature value.
