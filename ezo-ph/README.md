# Atlas Scientific EZO-pH Wokwi Model

Simulation model for the **Atlas Scientific EZO™ pH Circuit** in I2C mode.

## Features

*   **Communication**: I2C Interface (Default Address `0x63` / `99`).
*   **Protocol**: ASCII command protocol (EZO standard).
    *   Supports `R` (Read), `Cal`, `I` (Info), `Status`, `Sleep`.
    *   Simulates processing delays (`Pending` status 254).

## Controls

*   **pH Value**: A slider control allowing you to set the pH from 0.00 to 14.00.
    *   *Note: Detailed implementation uses centi-pH (e.g., 700 = 7.00) for internal precision.*

## Usage

Send the command `"R"` to address `99` to trigger a reading. The device will return status code `254` (Pending) until the simulated processing time (~900ms) is complete, after which it returns `1` (Success) followed by the ASCII pH value.
