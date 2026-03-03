# Usage Examples

The following directories contain Wokwi projects that demonstrate how to use the custom chip models.

Each directory includes:
*   `diagram.json`: The wiring diagram.
*   `wokwi.toml`: The configuration linking the generic chip name (e.g., `chip-ads1115`) to the local source code (`../../ads1115`).
*   `sketch.ino`: A stub firmware file (simulating Arduino environment).

## How to Run

1.  Open one of the example folders (e.g., `ads1115`) in VS Code with the Wokwi Extension installed.
2.  Press F1 and select "Wokwi: Start Simulator".
3.  The chip model will be compiled automatically and loaded into the simulation.

## Available Examples

*   **ads1115**: Shows wiring for the ADS1115 ADC with a potentiometer.
*   **ezo-ph**: Shows connection to the EZO pH Sensor (Address 99).
*   **ezo-rtd**: Shows connection to the EZO RTD Sensor (Address 102).
*   **r2221**: Shows connection to the R2221 RTC.

*(Add more as needed)*
