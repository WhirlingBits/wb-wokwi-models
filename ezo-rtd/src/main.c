/*
 * Atlas Scientific EZO-RTD Temperature Circuit - Wokwi Custom Chip
 *
 * I2C Address: 0x66 (default)
 * Protocol:    ASCII command/response (EZO standard)
 *
 * Supported commands:
 *   R           - Read temperature (600ms processing)
 *   Cal,t       - Single-point calibration at t°C (600ms)
 *   Cal,?       - Query calibration status (300ms)
 *   Cal,clear   - Clear calibration (300ms)
 *   I           - Device info (300ms)
 *   Status      - Device status (300ms)
 *   T,t         - Set temperature compensation value (300ms)
 *   T,?         - Query temperature compensation (300ms)
 *   S,c|k|f     - Set scale Celsius/Kelvin/Fahrenheit (300ms)
 *   S,?         - Query current scale (300ms)
 *   Sleep       - Enter sleep mode (no response)
 *   Factory     - Factory reset (no response)
 *   Find        - Blink LED (300ms)
 *   LED,1|0     - Enable/disable LED (300ms)
 *   LED,?       - Query LED state (300ms)
 *
 * Response format: [status_byte][data...]
 *   Status 1   = Success
 *   Status 2   = Failed
 *   Status 254 = Pending (processing)
 *   Status 255 = No data
 *
 * Slider control: "temperature" in centi-degrees (2500 = 25.00°C)
 */

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Constants ────────────────────────────────────────────────────────── */

#define EZO_RTD_ADDR      0x66

#define STATUS_SUCCESS    1
#define STATUS_FAILED     2
#define STATUS_PENDING    254
#define STATUS_NO_DATA    255

#define CMD_BUF_SIZE      40
#define RESP_BUF_SIZE     40

/* Processing delays (microseconds) matching driver expectations */
#define DELAY_READ_US     (600 * 1000)
#define DELAY_CAL_US      (600 * 1000)
#define DELAY_GENERIC_US  (300 * 1000)

/* Temperature scales */
#define SCALE_CELSIUS     'C'
#define SCALE_KELVIN      'K'
#define SCALE_FAHRENHEIT  'F'

/* ── Chip State ───────────────────────────────────────────────────────── */

typedef struct {
  /* Pins (only functionally used ones) */
  pin_t pin_sda;
  pin_t pin_scl;

  /* Wokwi attribute: temperature in centi-degrees */
  uint32_t attr_temp;

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
  char     scale;           /* C, K, or F */
  float    temp_comp;       /* temperature compensation value */
  int      cal_status;      /* 0=uncal, 1=calibrated */
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

static float celsius_to_scale(float temp_c, char scale) {
  switch (scale) {
    case SCALE_KELVIN:     return temp_c + 273.15f;
    case SCALE_FAHRENHEIT: return temp_c * 1.8f + 32.0f;
    default:               return temp_c;
  }
}

/* ── Command Processing ───────────────────────────────────────────────── */

static void execute_command(chip_state_t *chip) {
  char *cmd = chip->cmd_buf;

  trim_trailing(cmd, &chip->cmd_len);
  str_to_upper(cmd, chip->cmd_len);

  chip->resp_buf[0] = '\0';
  chip->resp_len = 0;

  /* --- R: Read temperature ------------------------------------------ */
  if (strcmp(cmd, "R") == 0) {
    int32_t raw = (int32_t)attr_read(chip->attr_temp);
    float temp_c = (float)raw / 100.0f;
    float val = celsius_to_scale(temp_c, chip->scale);
    sprintf(chip->resp_buf, "%.2f", (double)val);

  /* --- Cal,t: Calibrate at known temperature ------------------------ */
  } else if (strncmp(cmd, "CAL,", 4) == 0) {
    const char *arg = &cmd[4];
    if (strcmp(arg, "?") == 0) {
      sprintf(chip->resp_buf, "?Cal,%d", chip->cal_status);
    } else if (strcmp(arg, "CLEAR") == 0) {
      chip->cal_status = 0;
      /* empty response = success with no data */
    } else {
      /* Cal,t — single-point calibration, just acknowledge */
      chip->cal_status = 1;
    }

  /* --- I: Device info ----------------------------------------------- */
  } else if (strcmp(cmd, "I") == 0) {
    sprintf(chip->resp_buf, "?I,RTD,2.02");

  /* --- Status: Device status ---------------------------------------- */
  } else if (strcmp(cmd, "STATUS") == 0) {
    sprintf(chip->resp_buf, "?STATUS,P,5.038");

  /* --- T,t: Temperature compensation -------------------------------- */
  } else if (strncmp(cmd, "T,", 2) == 0) {
    const char *arg = &cmd[2];
    if (strcmp(arg, "?") == 0) {
      sprintf(chip->resp_buf, "?T,%.2f", (double)chip->temp_comp);
    } else {
      chip->temp_comp = (float)strtod(arg, NULL);
    }

  /* --- S,x: Scale --------------------------------------------------- */
  } else if (strncmp(cmd, "S,", 2) == 0) {
    const char *arg = &cmd[2];
    if (strcmp(arg, "?") == 0) {
      sprintf(chip->resp_buf, "?S,%c", chip->scale);
    } else if (arg[0] == 'C' || arg[0] == 'K' || arg[0] == 'F') {
      chip->scale = arg[0];
    }

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
    /* no-op in simulation, but any subsequent I2C activity wakes */
    chip->status_code = STATUS_NO_DATA;
    chip->processing = false;
    chip->cmd_len = 0;
    return;

  /* --- Factory: Factory reset --------------------------------------- */
  } else if (strcmp(cmd, "FACTORY") == 0) {
    chip->scale = SCALE_CELSIUS;
    chip->temp_comp = 25.0f;
    chip->cal_status = 0;
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

  /* Attribute: temperature in centi-degrees (default 25.00°C) */
  chip->attr_temp = attr_init("temperature", 2500);

  /* Default device state */
  chip->scale      = SCALE_CELSIUS;
  chip->temp_comp  = 25.0f;
  chip->cal_status = 0;
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
    .address     = EZO_RTD_ADDR,
    .scl         = chip->pin_scl,
    .sda         = chip->pin_sda,
    .connect     = on_i2c_connect,
    .read        = on_i2c_read,
    .write       = on_i2c_write,
    .disconnect  = on_i2c_disconnect,
  };
  i2c_init(&i2c_cfg);

  printf("EZO-RTD initialized at 0x%02X\n", EZO_RTD_ADDR);
}
