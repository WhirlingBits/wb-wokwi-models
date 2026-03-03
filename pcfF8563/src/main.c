#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

// I2C Address
#define PCF8563_ADDR 0x51

// Registers (0x00 - 0x0F)
#define REG_CTRL_STATUS_1   0x00
#define REG_CTRL_STATUS_2   0x01
#define REG_SECONDS         0x02
#define REG_MINUTES         0x03
#define REG_HOURS           0x04
#define REG_DAYS            0x05
#define REG_WEEKDAYS        0x06
#define REG_MONTHS          0x07
#define REG_YEARS           0x08
#define REG_ALARM_MIN       0x09
#define REG_ALARM_HOUR      0x0A
#define REG_ALARM_DAY       0x0B
#define REG_ALARM_WEEKDAY   0x0C
#define REG_CLKOUT_CTRL     0x0D
#define REG_TIMER_CTRL      0x0E
#define REG_TIMER           0x0F

// Control/Status 1 bits
#define BIT_TEST1   7  // EXT_CLK test mode
#define BIT_STOP    5  // Stop RTC clock
#define BIT_TESTC   3  // Power-on reset override

// Control/Status 2 bits
#define BIT_TI_TP   4  // Timer interrupt type (pulse/level)
#define BIT_AF      3  // Alarm flag
#define BIT_TF      2  // Timer flag
#define BIT_AIE     1  // Alarm interrupt enable
#define BIT_TIE     0  // Timer interrupt enable

// Alarm register disable bit (bit 7 = 1 means alarm disabled for that field)
#define ALARM_DISABLE 0x80

typedef struct {
  pin_t pin_int;
  pin_t pin_clkout;

  uint8_t registers[16];
  uint8_t address_ptr;

  // I2C State
  uint8_t i2c_write_seq; // 0=Addr Ptr, 1=Data

  timer_t tick_timer;
  timer_t countdown_timer;
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
  const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 30;
  uint8_t d = days[month - 1];
  if (month == 2) {
    uint16_t y = 2000 + year;
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) d = 29;
  }
  return d;
}

// ---- Alarm Check ----

static void check_alarm(chip_state_t *chip) {
  uint8_t ctrl2 = chip->registers[REG_CTRL_STATUS_2];
  bool match = true;

  // Each alarm field: bit 7 = 0 means enabled for matching
  // Minute alarm
  if (!(chip->registers[REG_ALARM_MIN] & ALARM_DISABLE)) {
    uint8_t alarm_min = from_bcd(chip->registers[REG_ALARM_MIN] & 0x7F);
    uint8_t curr_min  = from_bcd(chip->registers[REG_MINUTES] & 0x7F);
    if (alarm_min != curr_min) match = false;
  }

  // Hour alarm
  if (!(chip->registers[REG_ALARM_HOUR] & ALARM_DISABLE)) {
    uint8_t alarm_hour = from_bcd(chip->registers[REG_ALARM_HOUR] & 0x3F);
    uint8_t curr_hour  = from_bcd(chip->registers[REG_HOURS] & 0x3F);
    if (alarm_hour != curr_hour) match = false;
  }

  // Day alarm
  if (!(chip->registers[REG_ALARM_DAY] & ALARM_DISABLE)) {
    uint8_t alarm_day = from_bcd(chip->registers[REG_ALARM_DAY] & 0x3F);
    uint8_t curr_day  = from_bcd(chip->registers[REG_DAYS] & 0x3F);
    if (alarm_day != curr_day) match = false;
  }

  // Weekday alarm
  if (!(chip->registers[REG_ALARM_WEEKDAY] & ALARM_DISABLE)) {
    uint8_t alarm_wd = chip->registers[REG_ALARM_WEEKDAY] & 0x07;
    uint8_t curr_wd  = chip->registers[REG_WEEKDAYS] & 0x07;
    if (alarm_wd != curr_wd) match = false;
  }

  // Check if ALL enabled alarm fields are disabled (no alarm configured)
  bool any_enabled = false;
  if (!(chip->registers[REG_ALARM_MIN] & ALARM_DISABLE)) any_enabled = true;
  if (!(chip->registers[REG_ALARM_HOUR] & ALARM_DISABLE)) any_enabled = true;
  if (!(chip->registers[REG_ALARM_DAY] & ALARM_DISABLE)) any_enabled = true;
  if (!(chip->registers[REG_ALARM_WEEKDAY] & ALARM_DISABLE)) any_enabled = true;

  if (match && any_enabled) {
    // Set alarm flag
    chip->registers[REG_CTRL_STATUS_2] |= (1 << BIT_AF);

    // Assert INT if alarm interrupt enabled (AIE)
    if (ctrl2 & (1 << BIT_AIE)) {
      pin_write(chip->pin_int, LOW);
    }
  }
}

// ---- Countdown Timer Callback ----

static void countdown_timer_tick(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  // Decrement timer value
  if (chip->registers[REG_TIMER] > 0) {
    chip->registers[REG_TIMER]--;
  }

  if (chip->registers[REG_TIMER] == 0) {
    // Set timer flag
    chip->registers[REG_CTRL_STATUS_2] |= (1 << BIT_TF);

    // Assert INT if timer interrupt enabled (TIE)
    if (chip->registers[REG_CTRL_STATUS_2] & (1 << BIT_TIE)) {
      pin_write(chip->pin_int, LOW);
    }
  }
}

// ---- RTC Tick (1 second) ----

static void update_time(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  // Check STOP bit (Bit 5 of Control/Status 1)
  if (chip->registers[REG_CTRL_STATUS_1] & (1 << BIT_STOP)) {
    return; // Clock is stopped
  }

  uint8_t s    = from_bcd(chip->registers[REG_SECONDS] & 0x7F);
  uint8_t m    = from_bcd(chip->registers[REG_MINUTES] & 0x7F);
  uint8_t h    = from_bcd(chip->registers[REG_HOURS] & 0x3F);
  uint8_t wday = chip->registers[REG_WEEKDAYS] & 0x07;
  uint8_t d    = from_bcd(chip->registers[REG_DAYS] & 0x3F);
  uint8_t mo   = from_bcd(chip->registers[REG_MONTHS] & 0x1F);
  uint8_t yr   = from_bcd(chip->registers[REG_YEARS]);

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
        d++;
        if (d > days_in_month(mo, yr)) {
          d = 1;
          mo++;
          if (mo > 12) {
            mo = 1;
            yr++;
            if (yr > 99) yr = 0;
          }
        }
      }
    }
  }

  // Preserve VL bit (bit 7 of seconds register)
  uint8_t vl_bit = chip->registers[REG_SECONDS] & 0x80;
  chip->registers[REG_SECONDS]  = to_bcd(s) | vl_bit;
  chip->registers[REG_MINUTES]  = to_bcd(m);
  chip->registers[REG_HOURS]    = to_bcd(h);
  chip->registers[REG_WEEKDAYS] = wday;
  chip->registers[REG_DAYS]     = to_bcd(d);
  // Preserve century bit (bit 7 of months register)
  uint8_t century_bit = chip->registers[REG_MONTHS] & 0x80;
  chip->registers[REG_MONTHS]   = to_bcd(mo) | century_bit;
  chip->registers[REG_YEARS]    = to_bcd(yr);

  check_alarm(chip);
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
    chip->address_ptr = data & 0x0F;
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

  chip->pin_clkout = pin_init("CLKOUT", OUTPUT);
  pin_write(chip->pin_clkout, HIGH); // CLKOUT enabled by default (32.768 kHz)

  printf("PCF8563 Initialized at I2C Address 0x%02X\n", PCF8563_ADDR);

  // Power-on defaults: 2000-01-01 00:00:00 Saturday
  chip->registers[REG_CTRL_STATUS_1] = 0x08; // TESTC=1 (normal power-on)
  chip->registers[REG_CTRL_STATUS_2] = 0x00;
  chip->registers[REG_SECONDS]       = 0x80; // VL=1 (voltage low detected on power-on)
  chip->registers[REG_MINUTES]       = 0x00;
  chip->registers[REG_HOURS]         = 0x00;
  chip->registers[REG_DAYS]          = 0x01;
  chip->registers[REG_WEEKDAYS]      = 0x06; // Saturday
  chip->registers[REG_MONTHS]        = 0x01;
  chip->registers[REG_YEARS]         = 0x00; // Year 2000

  // Alarm registers: all disabled by default (bit 7 = 1)
  chip->registers[REG_ALARM_MIN]     = ALARM_DISABLE;
  chip->registers[REG_ALARM_HOUR]    = ALARM_DISABLE;
  chip->registers[REG_ALARM_DAY]     = ALARM_DISABLE;
  chip->registers[REG_ALARM_WEEKDAY] = ALARM_DISABLE;

  // CLKOUT enabled at 32.768 kHz by default
  chip->registers[REG_CLKOUT_CTRL]   = 0x80; // FE=1, FD=00 (32.768 kHz)

  // Timer disabled by default
  chip->registers[REG_TIMER_CTRL]    = 0x00;
  chip->registers[REG_TIMER]         = 0x00;

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = PCF8563_ADDR,
    .scl = pin_init("SCL", INPUT),
    .sda = pin_init("SDA", INPUT),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
  };
  i2c_init(&i2c_config);

  // 1-second RTC tick
  const timer_config_t tick_config = {
    .callback = update_time,
    .user_data = chip,
  };
  chip->tick_timer = timer_init(&tick_config);
  timer_start(chip->tick_timer, 1000000, true);

  // Countdown timer (for timer register)
  const timer_config_t countdown_config = {
    .callback = countdown_timer_tick,
    .user_data = chip,
  };
  chip->countdown_timer = timer_init(&countdown_config);
  // Timer starts when TE bit is set in REG_TIMER_CTRL via I2C write
  // For simplicity, start a 1-second countdown tick (effective only when timer value > 0)
  // In real hardware, the clock source for the timer is configurable (4096/64/1/1/60 Hz)
  timer_start(chip->countdown_timer, 1000000, true);
}
