#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

// I2C Address
#define MAX17261_ADDR 0x36

// Key Registers
#define REG_STATUS          0x00
#define REG_VALRTTH         0x01
#define REG_TALRTTH         0x02
#define REG_SALRTTH         0x03
#define REG_REP_CAP         0x05
#define REG_REP_SOC         0x06
#define REG_TEMP            0x08
#define REG_VCELL           0x09
#define REG_CURRENT         0x0A
#define REG_AVG_CURRENT     0x0B
#define REG_REM_CAP         0x0F
#define REG_FULL_CAP_REP    0x10
#define REG_TTE             0x11
#define REG_QRTABLE00       0x12
#define REG_FULLSOCTHR      0x13
#define REG_AVG_TEMP        0x16
#define REG_CYCLES          0x17
#define REG_DESIGN_CAP      0x18
#define REG_AVG_VCELL       0x19
#define REG_MAXMIN_TEMP     0x1A
#define REG_MAXMIN_VOLT     0x1B
#define REG_MAXMIN_CURR     0x1C
#define REG_CONFIG          0x1D
#define REG_ICHGTERM        0x1E
#define REG_TTF             0x20
#define REG_DEV_NAME        0x21
#define REG_QRTABLE10       0x22
#define REG_FULL_CAP_NOM    0x23
#define REG_LEARNCFG        0x28
#define REG_RELAXCFG        0x2A
#define REG_TGAIN           0x2C
#define REG_TOFF            0x2D
#define REG_QRTABLE20       0x32
#define REG_DIETEMP         0x34
#define REG_RCOMP0          0x38
#define REG_TEMPCO          0x39
#define REG_VEMPTY          0x3A
#define REG_FSTAT           0x3D
#define REG_QRTABLE30       0x42
#define REG_DQACC           0x45
#define REG_DPACC           0x46
#define REG_VFSOC0          0x48
#define REG_QH0             0x4C
#define REG_QH              0x4D
#define REG_VFSOC0_QH0_LOCK 0x60
#define REG_LOCK1           0x62
#define REG_LOCK2           0x63
#define REG_IALRTTH         0xB4
#define REG_HIBCFG          0xBA
#define REG_CONFIG2         0xBB
#define REG_MODELCFG        0xDB
#define REG_OCV             0xFB
#define REG_VFSOC           0xFF

// Status register bits
#define BIT_POR    1   // Power-on Reset

// Sense resistor for unit conversion (10 mOhm)
#define R_SENSE              0.01
#define CAPACITY_MULTIPLIER  (5e-3 / R_SENSE)       // 0.5 mAh/LSB
#define CURRENT_MULTIPLIER   (1.5625e-4 / R_SENSE)  // 15.625 mA/LSB
#define VOLTAGE_MULTIPLIER   7.8125e-5               // 78.125 uV/LSB
#define TEMP_MULTIPLIER      256                     // 1/256 °C / LSB
#define TIME_MULTIPLIER      5.625                   // 5.625 sec/LSB

typedef struct {
  pin_t pin_alrt;

  uint16_t registers[256];
  uint8_t address_ptr;

  // I2C State
  uint8_t i2c_write_seq;  // 0=RegAddr, 1=LSB, 2=MSB
  uint16_t i2c_temp_val;
  bool read_msb_next;     // Toggle for 16-bit read (LSB first, then MSB)

  timer_t update_timer;

  // Attribute handles
  uint32_t attr_soc;
  uint32_t attr_current;
  uint32_t attr_temp;
} chip_state_t;

// ---- Simulation Update (driven by Wokwi attributes/sliders) ----

static void update_registers(chip_state_t *chip) {
  int32_t soc        = (int32_t)attr_read(chip->attr_soc);        // 0-100 %
  int32_t current_mA = (int32_t)attr_read(chip->attr_current);    // mA (signed)
  int32_t temp_C     = (int32_t)attr_read(chip->attr_temp);       // °C (signed)

  // Clamp SOC
  if (soc < 0)   soc = 0;
  if (soc > 100) soc = 100;

  // 1. RepSOC (0x06): LSB = 1/256 %
  chip->registers[REG_REP_SOC] = (uint16_t)(soc * 256);

  // 2. VCell (0x09): derive from SOC — linear 3.0V (0%) to 4.2V (100%)
  //    LSB = 78.125 uV
  double voltage_v = 3.0 + (1.2 * (double)soc / 100.0);
  uint16_t vcell = (uint16_t)(voltage_v / VOLTAGE_MULTIPLIER);
  chip->registers[REG_VCELL]    = vcell;
  chip->registers[REG_AVG_VCELL] = vcell;

  // 3. Current (0x0A): LSB = 1.5625 uV / R_SENSE = 15.625 mA/LSB
  int16_t current_reg = (int16_t)((double)current_mA / (CURRENT_MULTIPLIER * 1000.0));
  chip->registers[REG_CURRENT]     = (uint16_t)current_reg;
  chip->registers[REG_AVG_CURRENT] = (uint16_t)current_reg;

  // 4. Temperature (0x08): LSB = 1/256 °C
  int16_t temp_reg = (int16_t)(temp_C * TEMP_MULTIPLIER);
  chip->registers[REG_TEMP]     = (uint16_t)temp_reg;
  chip->registers[REG_AVG_TEMP] = (uint16_t)temp_reg;
  chip->registers[REG_DIETEMP]  = (uint16_t)temp_reg;

  // 5. RepCap (0x05): FullCap * SOC / 100
  uint16_t full_cap = chip->registers[REG_FULL_CAP_REP];
  uint32_t rep_cap  = ((uint32_t)full_cap * (uint32_t)soc) / 100;
  chip->registers[REG_REP_CAP] = (uint16_t)rep_cap;
  chip->registers[REG_REM_CAP] = (uint16_t)rep_cap;

  // 6. TTE (0x11): Time to Empty in 5.625 sec units
  //    Simple estimation: if discharging (current < 0), TTE = remaining_cap / |current|
  if (current_mA < 0 && rep_cap > 0) {
    double cap_mAh   = (double)rep_cap * CAPACITY_MULTIPLIER;
    double abs_curr   = (double)(-current_mA);
    double tte_hours  = cap_mAh / abs_curr;
    double tte_secs   = tte_hours * 3600.0;
    uint16_t tte_reg  = (uint16_t)(tte_secs / TIME_MULTIPLIER);
    chip->registers[REG_TTE] = tte_reg;
  } else {
    chip->registers[REG_TTE] = 0xFFFF; // Not discharging
  }

  // 7. TTF (0x20): Time to Full
  if (current_mA > 0 && soc < 100) {
    double remaining_mAh = (double)(full_cap - rep_cap) * CAPACITY_MULTIPLIER;
    double ttf_hours     = remaining_mAh / (double)current_mA;
    double ttf_secs      = ttf_hours * 3600.0;
    uint16_t ttf_reg     = (uint16_t)(ttf_secs / TIME_MULTIPLIER);
    chip->registers[REG_TTF] = ttf_reg;
  } else {
    chip->registers[REG_TTF] = 0xFFFF; // Not charging
  }

  // 8. MaxMin registers (simplified: current values as both min and max)
  chip->registers[REG_MAXMIN_TEMP] = ((uint8_t)temp_C << 8) | ((uint8_t)temp_C & 0xFF);
  chip->registers[REG_MAXMIN_VOLT] = ((vcell >> 8) << 8) | (vcell >> 8);

  // 9. VFSOC
  chip->registers[REG_VFSOC] = chip->registers[REG_REP_SOC];
}

static void update_timer_callback(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  update_registers(chip);
}

// ---- I2C Callbacks ----

static bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (connect) {
    chip->i2c_write_seq = 0;
    chip->read_msb_next = false;
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  uint16_t val = chip->registers[chip->address_ptr];
  uint8_t ret;

  // MAX17261: 16-bit word read, LSB first then MSB
  if (!chip->read_msb_next) {
    ret = val & 0xFF;          // LSB
    chip->read_msb_next = true;
  } else {
    ret = (val >> 8) & 0xFF;   // MSB
    chip->read_msb_next = false;
    // No auto-increment per MAX17261 datasheet
  }

  return ret;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->i2c_write_seq == 0) {
    // Register address byte
    chip->address_ptr = data;
    chip->i2c_write_seq = 1;
  } else if (chip->i2c_write_seq == 1) {
    // LSB of 16-bit word
    chip->i2c_temp_val = data;
    chip->i2c_write_seq = 2;
  } else if (chip->i2c_write_seq == 2) {
    // MSB of 16-bit word
    chip->i2c_temp_val |= ((uint16_t)data << 8);
    chip->registers[chip->address_ptr] = chip->i2c_temp_val;

    // Handle ModelCFG refresh: clear bit 15 after write
    if (chip->address_ptr == REG_MODELCFG) {
      chip->registers[REG_MODELCFG] &= 0x7FFF;
    }

    // Handle FSTAT: clear DNR bit on any access
    chip->registers[REG_FSTAT] &= ~0x0001;

    // No auto-increment per MAX17261 datasheet
    chip->i2c_write_seq = 1;
  }

  return true;
}

static void on_i2c_disconnect(void *user_data) {
  // Nothing
}

// ---- Chip Init ----

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));

  chip->pin_alrt = pin_init("ALRT", OUTPUT);
  pin_write(chip->pin_alrt, HIGH); // ALRT is active-low, idle high

  printf("MAX17261 Fuel Gauge Initialized at I2C Address 0x%02X\n", MAX17261_ADDR);

  // Initialize attribute handles (mapped to chip.json controls/sliders)
  chip->attr_soc     = attr_init("soc", 50);
  chip->attr_current = attr_init("current", 0);
  chip->attr_temp    = attr_init("temp", 25);

  // Power-on defaults
  chip->registers[REG_STATUS]       = 0x0002;  // POR bit set
  chip->registers[REG_DEV_NAME]     = 0x4033;  // MAX17261 device ID
  chip->registers[REG_DESIGN_CAP]   = 0x2710;  // 10000 LSBs = 5000 mAh (@ 0.5 mAh/LSB)
  chip->registers[REG_FULL_CAP_REP] = 0x2710;
  chip->registers[REG_FULL_CAP_NOM] = 0x2710;
  chip->registers[REG_CONFIG]       = 0x0210;
  chip->registers[REG_CONFIG2]      = 0x0000;
  chip->registers[REG_HIBCFG]       = 0x0000;
  chip->registers[REG_MODELCFG]     = 0x0000;  // Refresh bit cleared
  chip->registers[REG_FSTAT]        = 0x0000;  // DNR=0, ready
  chip->registers[REG_VEMPTY]       = 0x9661;  // ~3.0V empty typical
  chip->registers[REG_ICHGTERM]     = 0x00A0;  // Charge termination ~25mA
  chip->registers[REG_FULLSOCTHR]   = 0x5F05;
  chip->registers[REG_LEARNCFG]     = 0x2602;
  chip->registers[REG_RELAXCFG]     = 0x023B;
  chip->registers[REG_RCOMP0]       = 0x0070;
  chip->registers[REG_TEMPCO]       = 0x223E;
  chip->registers[REG_CYCLES]       = 0x0000;
  chip->registers[REG_DQACC]        = chip->registers[REG_DESIGN_CAP] / 32;
  chip->registers[REG_DPACC]        = 0x0C80;

  // Initial simulation update
  update_registers(chip);

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = MAX17261_ADDR,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
  };
  i2c_init(&i2c_config);

  const timer_config_t timer_config = {
    .callback = update_timer_callback,
    .user_data = chip,
  };
  chip->update_timer = timer_init(&timer_config);
  timer_start(chip->update_timer, 250000, true); // Update 4x/sec
}
