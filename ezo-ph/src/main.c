/*
 * Atlas Scientific EZO-pH Circuit - Wokwi Custom Chip
 *
 * I2C Address: 0x63 (default)
 * Protocol:    ASCII command/response (EZO standard)
 *
 * Supported commands:
 *   R              - Read pH value (900ms processing)
 *   Cal,mid,n      - Mid-point calibration at n pH (900ms)
 *   Cal,low,n      - Low-point calibration at n pH (900ms)
 *   Cal,high,n     - High-point calibration at n pH (900ms)
 *   Cal,clear       - Clear calibration (300ms)
 *   Cal,?           - Query calibration status (300ms)
 *   I               - Device info (300ms)
 *   Status          - Device status (300ms)
 *   T,n             - Set temperature compensation in °C (300ms)
 *   T,?             - Query temperature compensation (300ms)
 *   Slope,?         - Query probe slope (300ms)
 *   Sleep           - Enter sleep mode (no response)
 *   Factory         - Factory reset (no response)
 *   Find            - Blink LED (300ms)
 *   LED,1|0         - Enable/disable LED (300ms)
 *   LED,?           - Query LED state (300ms)
 *
 * Response format: [status_byte][data...]
 *   Status 1   = Success
 *   Status 2   = Failed
 *   Status 254 = Pending (processing)
 *   Status 255 = No data
 *
 * Slider control: "ph" in centi-pH (700 = 7.00)
 */

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Constants ────────────────────────────────────────────────────────── */

#define EZO_PH_ADDR      0x63

#define STATUS_SUCCESS    1
#define STATUS_FAILED     2
#define STATUS_PENDING    254
#define STATUS_NO_DATA    255

#define CMD_BUF_SIZE      40
#define RESP_BUF_SIZE     40

/* Processing delays (microseconds) matching driver expectations */
#define DELAY_READ_US     (900 * 1000)
#define DELAY_CAL_US      (900 * 1000)
#define DELAY_GENERIC_US  (300 * 1000)

/* ── Chip State ───────────────────────────────────────────────────────── */

typedef struct {
  /* Pins (only functionally used ones) */
  pin_t pin_sda;
  pin_t pin_scl;

  /* Wokwi attribute: pH in centi-pH */
  uint32_t attr_ph;

  /* Command buffer (filled during I2C writes) */
  char     cmd_buf[CMD_BUF_SIZE];
  int      cmd_len;

  /* Response buffer (read back during I2C reads) */
  uint8_t  status_code;
  char     resp_buf[RESP_BUF_SIZE];
  int      resp_len;
  int      resp_ptr;

  /* Processing state */
  bool     processing;
  timer_t  process_timer;

  /* Device state */
  float    temp_comp;       /* temperature compensation value (°C) */
  int      cal_status;      /* 0=uncal, 1=1pt, 2=2pt, 3=3pt */
  float    slope_acid;      /* acid slope % */
  float    slope_base;      /* base slope % */
  bool     led_on;

} chip_state_t;

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void str_to_upper(char *s, int len) {
  for (int i = 0; i < len; i++) {
    if (s[i] >= 'a' && s[i] <= 'z') s[i] -= 32;
  }
}

static void trim_trailing(char *s, int *len) {
  while (*len > 0 && (s[*len - 1] == '\r' || s[*len - 1] == '\n' || s[*len - 1] == ' ')) {
    s[--(*len)] = '\0';
  }
}

/* ── Command Processing ───────────────────────────────────────────────── */

static void execute_command(chip_state_t *chip) {
  char *cmd = chip->cmd_buf;

  trim_trailing(cmd, &chip->cmd_len);
  str_to_upper(cmd, chip->cmd_len);

  chip->resp_buf[0] = '\0';
  chip->resp_len = 0;

  /* --- R: Read pH --------------------------------------------------- */
  if (strcmp(cmd, "R") == 0) {
    uint32_t raw = attr_read(chip->attr_ph);
    float ph_val = (float)raw / 100.0f;
    sprintf(chip->resp_buf, "%.2f", (double)ph_val);

  /* --- Cal: Calibration --------------------------------------------- */
  } else if (strncmp(cmd, "CAL,", 4) == 0) {
    const char *arg = &cmd[4];
    if (strcmp(arg, "?") == 0) {
      sprintf(chip->resp_buf, "?Cal,%d", chip->cal_status);
    } else if (strcmp(arg, "CLEAR") == 0) {
      chip->cal_status = 0;
      /* empty response = success */
    } else if (strncmp(arg, "MID,", 4) == 0) {
      /* Cal,mid,n — mid-point calibration */
      if (chip->cal_status < 1) chip->cal_status = 1;
    } else if (strncmp(arg, "LOW,", 4) == 0) {
      /* Cal,low,n — low-point calibration */
      if (chip->cal_status < 2) chip->cal_status = 2;
    } else if (strncmp(arg, "HIGH,", 5) == 0) {
      /* Cal,high,n — high-point calibration */
      if (chip->cal_status < 3) chip->cal_status = 3;
    }

  /* --- I: Device info ----------------------------------------------- */
  } else if (strcmp(cmd, "I") == 0) {
    sprintf(chip->resp_buf, "?I,pH,2.12");

  /* --- Status: Device status ---------------------------------------- */
  } else if (strcmp(cmd, "STATUS") == 0) {
    sprintf(chip->resp_buf, "?STATUS,P,5.038");

  /* --- T,n: Temperature compensation -------------------------------- */
  } else if (strncmp(cmd, "T,", 2) == 0) {
    const char *arg = &cmd[2];
    if (strcmp(arg, "?") == 0) {
      sprintf(chip->resp_buf, "?T,%.2f", (double)chip->temp_comp);
    } else {
      chip->temp_comp = (float)strtod(arg, NULL);
    }

  /* --- Slope,?: Probe slope ----------------------------------------- */
  } else if (strcmp(cmd, "SLOPE,?") == 0) {
    sprintf(chip->resp_buf, "?Slope,%.1f,%.1f,0.0",
            (double)chip->slope_acid, (double)chip->slope_base);

  /* --- LED,x: LED control ------------------------------------------- */
  } else if (strncmp(cmd, "LED,", 4) == 0) {
    const char *arg = &cmd[4];
    if (strcmp(arg, "?") == 0) {
      sprintf(chip->resp_buf, "?L,%d", chip->led_on ? 1 : 0);
    } else {
      chip->led_on = (arg[0] == '1');
    }

  /* --- Find: Blink LED ---------------------------------------------- */
  } else if (strcmp(cmd, "FIND") == 0) {
    /* no-op in simulation */

  /* --- Sleep: Enter low-power mode ---------------------------------- */
  } else if (strcmp(cmd, "SLEEP") == 0) {
    chip->status_code = STATUS_NO_DATA;
    chip->processing = false;
    chip->cmd_len = 0;
    return;

  /* --- Factory: Factory reset --------------------------------------- */
  } else if (strcmp(cmd, "FACTORY") == 0) {
    chip->temp_comp = 25.0f;
    chip->cal_status = 0;
    chip->slope_acid = 99.7f;
    chip->slope_base = 100.3f;
    chip->led_on = true;
    chip->status_code = STATUS_NO_DATA;
    chip->processing = false;
    chip->cmd_len = 0;
    return;

  /* --- Unknown command ---------------------------------------------- */
  } else {
    chip->status_code = STATUS_FAILED;
    chip->processing = false;
    chip->cmd_len = 0;
    return;
  }

  chip->status_code = STATUS_SUCCESS;
  chip->resp_len = (int)strlen(chip->resp_buf);
  chip->resp_ptr = 0;
  chip->processing = false;
  chip->cmd_len = 0;
}

/* ── Timer Callback (simulates processing delay) ─────────────────────── */

static void on_process_timer(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  execute_command(chip);
}

/* ── I2C Callbacks ────────────────────────────────────────────────────── */

static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (read) {
    /* Master wants to read the response */
    chip->resp_ptr = 0;
  } else {
    /* Master starts writing a new command */
    chip->cmd_len = 0;
    memset(chip->cmd_buf, 0, sizeof(chip->cmd_buf));
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  /* Still processing → return pending */
  if (chip->processing) {
    return STATUS_PENDING;
  }

  /* First byte = status code */
  if (chip->resp_ptr == 0) {
    if (chip->status_code == 0) return STATUS_NO_DATA;
    chip->resp_ptr++;
    return chip->status_code;
  }

  /* Subsequent bytes = response data */
  int idx = chip->resp_ptr - 1;
  if (idx < chip->resp_len) {
    chip->resp_ptr++;
    return (uint8_t)chip->resp_buf[idx];
  }

  /* Past end of response */
  return 0;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->cmd_len < CMD_BUF_SIZE - 1) {
    chip->cmd_buf[chip->cmd_len++] = (char)data;
    chip->cmd_buf[chip->cmd_len] = '\0';
  }
  return true;
}

static void on_i2c_disconnect(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  /* If we received a command, start processing with appropriate delay */
  if (chip->cmd_len > 0 && !chip->processing) {
    chip->processing = true;
    chip->status_code = STATUS_PENDING;

    /* Determine processing delay based on command */
    char first = chip->cmd_buf[0];
    if (first >= 'a' && first <= 'z') first -= 32;

    uint32_t delay_us = DELAY_GENERIC_US;
    if (first == 'R') {
      delay_us = DELAY_READ_US;
    } else if (first == 'C') {
      delay_us = DELAY_CAL_US;
    }

    timer_start(chip->process_timer, delay_us, false);
  }
}

/* ── Chip Init ────────────────────────────────────────────────────────── */

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));

  /* Pins */
  chip->pin_sda = pin_init("TX-SDA", INPUT);
  chip->pin_scl = pin_init("RX-SCL", INPUT);

  /* Attribute: pH in centi-pH (default 7.00) */
  chip->attr_ph = attr_init("ph", 700);

  /* Default device state */
  chip->temp_comp  = 25.0f;
  chip->cal_status = 0;
  chip->slope_acid = 99.7f;
  chip->slope_base = 100.3f;
  chip->led_on     = true;
  chip->status_code = 0;

  /* Processing timer */
  const timer_config_t timer_cfg = {
    .callback  = on_process_timer,
    .user_data = chip,
  };
  chip->process_timer = timer_init(&timer_cfg);

  /* I2C */
  const i2c_config_t i2c_cfg = {
    .user_data   = chip,
    .address     = EZO_PH_ADDR,
    .scl         = chip->pin_scl,
    .sda         = chip->pin_sda,
    .connect     = on_i2c_connect,
    .read        = on_i2c_read,
    .write       = on_i2c_write,
    .disconnect  = on_i2c_disconnect,
  };
  i2c_init(&i2c_cfg);

  printf("EZO-pH initialized at 0x%02X\n", EZO_PH_ADDR);
}
