#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

// I2C Address
#define R2221_ADDR 0x32

// Registers
#define REG_SECOND          0x00
#define REG_MINUTE          0x01
#define REG_HOUR            0x02
#define REG_DAY_WEEK        0x03
#define REG_DAY_MONTH       0x04
#define REG_MONTH           0x05
#define REG_YEAR            0x06
#define REG_ADJUSTMENT      0x07
#define REG_ALARM_W_MINUTE  0x08
#define REG_ALARM_W_HOUR    0x09
#define REG_ALARM_W_WEEK    0x0A
#define REG_ALARM_D_MINUTE  0x0B
#define REG_ALARM_D_HOUR    0x0C
#define REG_USER_RAM        0x0D
#define REG_CTRL1           0x0E
#define REG_CTRL2           0x0F

// Control 1 bits
#define BIT_WALE   7  // Weekly alarm enable
#define BIT_DALE   6  // Daily alarm enable
#define BIT_12_24  5  // 12/24 hour mode (1=24h)
#define BIT_CLEN2  4  // Clock enable 2
#define BIT_CT2    2  // Periodic timer control
#define BIT_CT1    1
#define BIT_CT0    0

// Control 2 bits
#define BIT_ECO    7  // Economy mode
#define BIT_VDET   6  // Voltage detect
#define BIT_XSTP   5  // Oscillator stop
#define BIT_PON    4  // Power-on reset flag
#define BIT_CLEN1  3  // Clock enable 1
#define BIT_CTFG   2  // Periodic timer flag
#define BIT_WAFG   1  // Weekly alarm flag
#define BIT_DAFG   0  // Daily alarm flag

typedef struct {
  pin_t pin_int;

  uint8_t registers[16];
  uint8_t address_ptr;

  // I2C State
  uint8_t i2c_write_seq; // 0=Addr Ptr, 1=Data

  timer_t tick_timer;
} chip_state_t;

// ---- Helper Functions ----

static uint8_t to_bcd(uint8_t val) {
  return ((val / 10) << 4) | (val % 10);
}

static uint8_t from_bcd(uint8_t val) {
  return ((val >> 4) * 10) + (val & 0x0F);
}

static uint8_t days_in_month(uint8_t month, uint8_t year) {
  // month: 1-12, year: 0-99 (2000-2099)
  const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month < 1 || month > 12) return 30;
  uint8_t d = days[month - 1];
  if (month == 2) {
    uint16_t y = 2000 + year;
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) d = 29;
  }
  return d;
}

// ---- Alarm Check ----

static void check_alarms(chip_state_t *chip) {
  uint8_t ctrl1 = chip->registers[REG_CTRL1];
  uint8_t min  = chip->registers[REG_MINUTE];
  uint8_t hour = chip->registers[REG_HOUR];
  uint8_t wday = chip->registers[REG_DAY_WEEK];
  bool alarm_triggered = false;

  // Weekly alarm
  if (ctrl1 & (1 << BIT_WALE)) {
    if (min == chip->registers[REG_ALARM_W_MINUTE] &&
        hour == chip->registers[REG_ALARM_W_HOUR] &&
        (chip->registers[REG_ALARM_W_WEEK] & (1 << from_bcd(wday)))) {
      chip->registers[REG_CTRL2] |= (1 << BIT_WAFG);
      alarm_triggered = true;
    }
  }

  // Daily alarm
  if (ctrl1 & (1 << BIT_DALE)) {
    if (min == chip->registers[REG_ALARM_D_MINUTE] &&
        hour == chip->registers[REG_ALARM_D_HOUR]) {
      chip->registers[REG_CTRL2] |= (1 << BIT_DAFG);
      alarm_triggered = true;
    }
  }

  // Assert INT pin (active low, open drain) on alarm
  if (alarm_triggered) {
    pin_write(chip->pin_int, LOW);
  }
}

// ---- Timer Callback (1 second tick) ----

static void update_time(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  uint8_t s     = from_bcd(chip->registers[REG_SECOND] & 0x7F);
  uint8_t m     = from_bcd(chip->registers[REG_MINUTE] & 0x7F);
  uint8_t h     = from_bcd(chip->registers[REG_HOUR] & 0x3F);
  uint8_t wday  = from_bcd(chip->registers[REG_DAY_WEEK] & 0x07);
  uint8_t day   = from_bcd(chip->registers[REG_DAY_MONTH] & 0x3F);
  uint8_t month = from_bcd(chip->registers[REG_MONTH] & 0x1F);
  uint8_t year  = from_bcd(chip->registers[REG_YEAR]);

  s++;
  if (s >= 60) {
    s = 0;
    m++;
    if (m >= 60) {
      m = 0;
      h++;
      if (h >= 24) {
        h = 0;
        wday = (wday + 1) % 7;
        day++;
        if (day > days_in_month(month, year)) {
          day = 1;
          month++;
          if (month > 12) {
            month = 1;
            year++;
            if (year > 99) year = 0;
          }
        }
      }
    }
  }

  chip->registers[REG_SECOND]    = to_bcd(s);
  chip->registers[REG_MINUTE]    = to_bcd(m);
  chip->registers[REG_HOUR]      = to_bcd(h);
  chip->registers[REG_DAY_WEEK]  = to_bcd(wday);
  chip->registers[REG_DAY_MONTH] = to_bcd(day);
  chip->registers[REG_MONTH]     = to_bcd(month);
  chip->registers[REG_YEAR]      = to_bcd(year);

  // Periodic timer flag (CT2:CT0 in CTRL1)
  uint8_t ct = chip->registers[REG_CTRL1] & 0x07;
  if (ct != 0) {
    // Simplified: set CTFG every tick when periodic timer is active
    chip->registers[REG_CTRL2] |= (1 << BIT_CTFG);
  }

  check_alarms(chip);
}

// ---- I2C Callbacks ----

static bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (connect) {
    chip->i2c_write_seq = 0;
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  uint8_t val = chip->registers[chip->address_ptr & 0x0F];

  // Auto-increment address pointer
  chip->address_ptr = (chip->address_ptr + 1) & 0x0F;

  return val;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->i2c_write_seq == 0) {
    // R2221 uses upper nibble as register address: addr = (reg << 4)
    chip->address_ptr = (data >> 4) & 0x0F;
    chip->i2c_write_seq = 1;
  } else {
    chip->registers[chip->address_ptr & 0x0F] = data;
    // Auto-increment address pointer
    chip->address_ptr = (chip->address_ptr + 1) & 0x0F;
  }

  return true;
}

static void on_i2c_disconnect(void *user_data) {
  // Nothing
}

// ---- Chip Init ----

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));

  chip->pin_int = pin_init("INT", OUTPUT);
  pin_write(chip->pin_int, HIGH); // INT is active-low, idle high

  printf("R2221 RTC Initialized at I2C Address 0x%02X\n", R2221_ADDR);

  // Power-on defaults
  chip->registers[REG_SECOND]    = 0x00;
  chip->registers[REG_MINUTE]    = 0x00;
  chip->registers[REG_HOUR]      = 0x00;
  chip->registers[REG_DAY_WEEK]  = 0x00;
  chip->registers[REG_DAY_MONTH] = to_bcd(1);
  chip->registers[REG_MONTH]     = to_bcd(1);
  chip->registers[REG_YEAR]      = 0x00; // 2000

  // Control 2: PON=1, XSTP=1 (power-on reset state)
  chip->registers[REG_CTRL1] = 0x00;
  chip->registers[REG_CTRL2] = (1 << BIT_PON) | (1 << BIT_XSTP);

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = R2221_ADDR,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
  };
  i2c_init(&i2c_config);

  const timer_config_t timer_config = {
    .callback = update_time,
    .user_data = chip,
  };
  chip->tick_timer = timer_init(&timer_config);
  timer_start(chip->tick_timer, 1000000, true); // 1 second interval
}
