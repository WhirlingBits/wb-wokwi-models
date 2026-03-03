#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

// Registers
#define REG_INPUT_PORT    0x00
#define REG_OUTPUT_PORT   0x01
#define REG_POLARITY_INV  0x02
#define REG_CONFIG        0x03

typedef struct {
  pin_t pin_int;
  pin_t pin_gpio[8];

  // Registers
  uint8_t input_port_reg;   // Read-Only (reflects pin state)
  uint8_t output_port_reg;  // R/W (user sets output level)
  uint8_t polarity_inv_reg; // R/W (inverts input read)
  uint8_t config_reg;       // R/W (1=Input, 0=Output)

  uint8_t address_ptr;
  
  // I2C State
  uint32_t i2c_addr;
  uint8_t i2c_write_seq; // 0=Addr Ptr, 1=Data

} chip_state_t;

static void update_api_pins(chip_state_t *chip);

static bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (connect) {
      chip->i2c_write_seq = 0; // Reset to expect Command Byte
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  uint8_t val = 0;
  
  // Read register based on address pointer
  switch (chip->address_ptr & 0x03) {
      case REG_INPUT_PORT:
          {
              uint8_t inputs = 0;
              for(int i=0; i<8; i++) {
                  // For inputs, we read the pin. 
                  // If pin is HIGH, bit is 1.
                  if (pin_read(chip->pin_gpio[i]) == HIGH) {
                      inputs |= (1 << i);
                  }
              }
              val = inputs ^ chip->polarity_inv_reg;
          }
          break;
      case REG_OUTPUT_PORT:
          val = chip->output_port_reg;
          break;
      case REG_POLARITY_INV:
          val = chip->polarity_inv_reg;
          break;
      case REG_CONFIG:
          val = chip->config_reg;
          break;
  }
  
  return val;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  
  if (chip->i2c_write_seq == 0) {
      // Command Byte: Set Address Pointer
      chip->address_ptr = data & 0x03;
      chip->i2c_write_seq = 1;
  } else {
      // Data Byte: Write to Register
      switch (chip->address_ptr & 0x03) {
          case REG_OUTPUT_PORT:
              chip->output_port_reg = data;
              update_api_pins(chip);
              break;
          case REG_POLARITY_INV:
              chip->polarity_inv_reg = data;
              break;
          case REG_CONFIG:
              chip->config_reg = data;
              update_api_pins(chip);
              break;
          // Input Port is Read-Only
      }
      // Usually, address pointer doesn't increment in TCA9554 if we write multiple bytes to same register?
      // Datasheet logic implies register is written directly.
  }
  
  return true;
}

static void on_i2c_disconnect(void *user_data) {
    // Nothing
}

static void update_api_pins(chip_state_t *chip) {
    // Update direction and output state
    for(int i=0; i<8; i++) {
        // Config: 1=Input, 0=Output
        bool is_input = (chip->config_reg >> i) & 0x01;
        
        if (is_input) {
            pin_mode(chip->pin_gpio[i], INPUT);
        } else {
            pin_mode(chip->pin_gpio[i], OUTPUT);
            // Output Port: 1=High, 0=Low
            bool state = (chip->output_port_reg >> i) & 0x01;
            pin_write(chip->pin_gpio[i], state ? HIGH : LOW);
        }
    }
}

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));
  
  chip->pin_int = pin_init("INT", OUTPUT);
  pin_write(chip->pin_int, HIGH); // Inactive High

  char name[4];
  for(int i=0; i<8; i++) {
      sprintf(name, "P%d", i);
      chip->pin_gpio[i] = pin_init(name, INPUT);
  }

  // Address Pins A0-A2
  pin_t a0 = pin_init("A0", INPUT);
  pin_t a1 = pin_init("A1", INPUT);
  pin_t a2 = pin_init("A2", INPUT);

  // Determine I2C Address (Base 0x20 + A2 A1 A0)
  // We assume default LOW (0) if not connected, but pullups might exist?
  // Wokwi pins default to LOW/Floating. INPUT usually reads LOW unless driven.
  uint32_t addr = 0x20;
  if(pin_read(a0) == HIGH) addr |= 0x01;
  if(pin_read(a1) == HIGH) addr |= 0x02;
  if(pin_read(a2) == HIGH) addr |= 0x04;
  
  chip->i2c_addr = addr;
  printf("TCA9554 Initialized at I2C Address 0x%02X\n", chip->i2c_addr);

  // Default Register Values
  chip->input_port_reg = 0; 
  chip->output_port_reg = 0xFF; // Default High
  chip->polarity_inv_reg = 0x00;
  chip->config_reg = 0xFF; // All Inputs (Default Power-on)
  
  update_api_pins(chip);

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = chip->i2c_addr,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
  };
  
  i2c_init(&i2c_config);
}
