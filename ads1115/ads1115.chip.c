#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
// #include <stdint.h>

// I2C Address based on ADDR pin
// GND: 0x48 (1001000)
// VDD: 0x49 (1001001)
// SDA: 0x4A (1001010)
// SCL: 0x4B (1001011)

typedef struct {
  pin_t pin_vdd;
  pin_t pin_gnd;
  pin_t pin_scl;
  pin_t pin_sda;
  pin_t pin_addr;
  pin_t pin_alrt;
  pin_t pin_a0;
  pin_t pin_a1;
  pin_t pin_a2;
  pin_t pin_a3;

  uint16_t conversion_reg;
  uint16_t config_reg;
  uint16_t lo_thresh_reg;
  uint16_t hi_thresh_reg;
  
  uint8_t address_ptr; // Current register pointer

  uint32_t i2c_addr;
  
  // I2C State
  uint8_t i2c_write_state;
  uint16_t i2c_temp_val;
  uint8_t i2c_read_byte_idx;

  // Timer for conversion
  timer_t conversion_timer;
  
  // Conversion state
  bool conversion_in_progress;
  uint32_t timer_interval_us; // Timer interval based on data rate

} chip_state_t;

static chip_state_t chip_instance;

// Register Pointers
#define REG_CONVERSION 0x00
#define REG_CONFIG     0x01
#define REG_LO_THRESH  0x02
#define REG_HI_THRESH  0x03

static void chip_timer_callback(void *user_data);

static bool on_i2c_connect(void *user_data, uint32_t address, bool connect);
static uint8_t on_i2c_read(void *user_data);
static bool on_i2c_write(void *user_data, uint8_t data);
static void on_i2c_disconnect(void *user_data);

void chip_init(void) {
  chip_state_t *chip = &chip_instance;
  
  chip->pin_vdd = pin_init("VDD", INPUT);
  chip->pin_gnd = pin_init("GND", INPUT);
  chip->pin_scl = pin_init("SCL", INPUT);
  chip->pin_sda = pin_init("SDA", INPUT);
  chip->pin_addr = pin_init("ADDR", INPUT);
  chip->pin_alrt = pin_init("ALRT", OUTPUT);
  chip->pin_a0 = pin_init("A0", INPUT);
  chip->pin_a1 = pin_init("A1", INPUT);
  chip->pin_a2 = pin_init("A2", INPUT);
  chip->pin_a3 = pin_init("A3", INPUT);

  // Initialize registers to default values
  chip->config_reg = 0x8583; // Default reset value
  chip->lo_thresh_reg = 0x8000;
  chip->hi_thresh_reg = 0x7FFF;
  chip->address_ptr = 0;
  
  chip->i2c_write_state = 0;
  chip->i2c_temp_val = 0;
  chip->i2c_read_byte_idx = 0;
  
  chip->conversion_in_progress = false;
  chip->timer_interval_us = 10000; // Default 10ms

  // Determine I2C address
  // Simplified logic: assume ADDR connected to GND (0x48) if floating/low
  // In a real simulation, we should check pin_read(chip->pin_addr) voltage.
  // For now default to 0x48.
  chip->i2c_addr = 0x48; 
  // TODO: Check ADDR pin state to set 0x49, 0x4A, 0x4B

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = chip->i2c_addr,
    .scl = chip->pin_scl,
    .sda = chip->pin_sda,
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
  };
  i2c_init(&i2c_config);

  const timer_config_t timer_config = {
    .callback = chip_timer_callback,
    .user_data = chip,
  };
  chip->conversion_timer = timer_init(&timer_config);
  
  // Calculate initial timer interval from data rate (bits 7:5)
  uint8_t dr = (chip->config_reg >> 5) & 0x07;
  uint32_t dr_table[] = {125000, 62500, 31250, 15625, 7812, 4000, 2105, 1163}; // microseconds per sample
  chip->timer_interval_us = dr_table[dr];
  
  // Start timer
  timer_start(chip->conversion_timer, chip->timer_interval_us, true);

  // printf("ADS1115 Chip Initialized at 0x%02X\n", chip->i2c_addr);
}

static void chip_timer_callback(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  
  // Check device mode (bit 8)
  uint8_t mode = (chip->config_reg >> 8) & 0x01;
  
  // In single-shot mode, only convert if OS bit (bit 15) is set or conversion in progress
  if (mode == 1) { // Single-shot mode
    if (!chip->conversion_in_progress && !((chip->config_reg >> 15) & 0x01)) {
      // No conversion requested
      return;
    }
  }
  // In continuous mode, always convert
  
  // 1. Read input based on CONFIG register MUX bits [14:12]
  //    000 : AIN0 - AIN1
  //    001 : AIN0 - AIN3
  //    010 : AIN1 - AIN3
  //    011 : AIN2 - AIN3
  //    100 : AIN0 - GND
  //    101 : AIN1 - GND
  //    110 : AIN2 - GND
  //    111 : AIN3 - GND
  
  uint16_t mux = (chip->config_reg >> 12) & 0x07;
  uint16_t pga = (chip->config_reg >> 9) & 0x07;
  
  double v0 = pin_adc_read(chip->pin_a0);
  double v1 = pin_adc_read(chip->pin_a1);
  double v2 = pin_adc_read(chip->pin_a2);
  double v3 = pin_adc_read(chip->pin_a3);
  
  double diff_volts = 0.0;
  
  switch(mux) {
    case 0: diff_volts = v0 - v1; break;
    case 1: diff_volts = v0 - v3; break;
    case 2: diff_volts = v1 - v3; break;
    case 3: diff_volts = v2 - v3; break;
    case 4: diff_volts = v0; break;
    case 5: diff_volts = v1; break;
    case 6: diff_volts = v2; break;
    case 7: diff_volts = v3; break;
  }
  
  // 2. Apply PGA Gain
  //    000 : FS = +/-6.144V
  //    ...
  //    For simulation, we map voltage to 16-bit signed integer.
  
  double fs_range = 6.144;
  if      (pga == 0) fs_range = 6.144;
  else if (pga == 1) fs_range = 4.096; // Default
  else if (pga == 2) fs_range = 2.048;
  else if (pga == 3) fs_range = 1.024;
  else if (pga == 4) fs_range = 0.512;
  else               fs_range = 0.256;
  
  // Clamp
  if (diff_volts > fs_range) diff_volts = fs_range;
  if (diff_volts < -fs_range) diff_volts = -fs_range;
  
  int16_t raw_val = (int16_t)((diff_volts / fs_range) * 32767.0);
  
  chip->conversion_reg = (uint16_t)raw_val;
  
  // In single-shot mode, clear OS bit after conversion and set conversion_in_progress to false
  if (mode == 1) { // Single-shot mode
    chip->config_reg &= ~(1 << 15); // Clear OS bit (bit 15)
    chip->conversion_in_progress = false;
  }
  // In continuous mode, just keep updating.
}


static bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  chip_state_t *chip = (chip_state_t *)user_data;
  // Reset state on new connection (START condition)
  // Note: connect=false means STOP or Re-START? 
  // Wokwi API: disconnect callback is for STOP. connect callback is for START.
  chip->i2c_write_state = 0;
  chip->i2c_read_byte_idx = 0;
  return true; // Ack
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  
  // 16-bit registers: reading high byte then low byte?
  // Usually the pointer register isn't incremented automatically on ADS1115
  // But reading multiple bytes returns MSB then LSB.
  
  // We need to keep track of a byte index or just return based on what was requested?
  // Wokwi I2C API handles bytes one by one.
  
  // Let's implement a simple state for read:
  // We assume the master reads 2 bytes for the register.
  
  uint16_t value = 0;
  
  switch(chip->address_ptr) {
    case REG_CONVERSION: value = chip->conversion_reg; break;
    case REG_CONFIG:     value = chip->config_reg; break;
    case REG_LO_THRESH:  value = chip->lo_thresh_reg; break;
    case REG_HI_THRESH:  value = chip->hi_thresh_reg; break;
  }
  
  uint8_t ret = 0;
  if (chip->i2c_read_byte_idx == 0) {
    ret = (value >> 8) & 0xFF;
    chip->i2c_read_byte_idx = 1;
  } else {
    ret = value & 0xFF; // LSB
    chip->i2c_read_byte_idx = 0; // Reset for next read sequence?
    // Usually standard I2C wraps or stays at last byte? ADS1115 datasheet says read wraps.
  }
  return ret;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  
  if (chip->i2c_write_state == 0) {
    // First byte is always the Address Pointer Register
    chip->address_ptr = data & 0x03; 
    chip->i2c_write_state = 1; // Expecting data next, if any
  } else if (chip->i2c_write_state == 1) {
    // MSB of the register data
    chip->i2c_temp_val = (uint16_t)data << 8;
    chip->i2c_write_state = 2;
  } else if (chip->i2c_write_state == 2) {
    // LSB of the register data
    chip->i2c_temp_val |= data;
    
    // Write complete, update register
    switch(chip->address_ptr) {
      case REG_CONFIG:     chip->config_reg = chip->i2c_temp_val; break;
      case REG_LO_THRESH:  chip->lo_thresh_reg = chip->i2c_temp_val; break;
      case REG_HI_THRESH:  chip->hi_thresh_reg = chip->i2c_temp_val; break;
      // Conversion reg is read-only
    }
    
    // Check if OS bit (bit 15) is 1 in config to start a single shot conversion
    if (chip->address_ptr == REG_CONFIG) {
      uint8_t new_mode = (chip->i2c_temp_val >> 8) & 0x01;
      uint8_t new_dr = (chip->i2c_temp_val >> 5) & 0x07;
      
      // Update data rate timer if changed
      uint32_t dr_table[] = {125000, 62500, 31250, 15625, 7812, 4000, 2105, 1163};
      chip->timer_interval_us = dr_table[new_dr];
      
      // If OS bit is set (bit 15), start single-shot conversion
      if ((chip->i2c_temp_val & 0x8000)) {
        chip->conversion_in_progress = true;
      }
    }

    // After writing 2 bytes, if master continues, what happens?
    // Simple implementation: wait for new START/STOP.
    // We can reset state or stay here. Let's just stay here or reset to 1?
    // Ideally the master sends a STOP condition.
    chip->i2c_write_state = 1; 
  }
  
  return true; // Ack
}

static void on_i2c_disconnect(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  // Reset state for next transaction
  chip->i2c_write_state = 0;
  chip->i2c_read_byte_idx = 0;
}
