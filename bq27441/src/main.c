/*
 * Texas Instruments BQ27441-G1 Fuel Gauge - Wokwi Custom Chip
 *
 * I2C Address: 0x55 (fixed)
 * Protocol:    16-bit little-endian register read/write
 *
 * Standard Commands (read word at register address):
 *   0x00  CONTROL       Write subcmd (2 bytes LSB,MSB), read result
 *   0x02  TEMPERATURE   0.1°K units
 *   0x04  VOLTAGE       mV
 *   0x06  FLAGS         Status flags
 *   0x08  NOM_AVAIL_CAP mAh
 *   0x0A  FULL_AVAIL_CAP mAh
 *   0x0C  REM_CAP       mAh
 *   0x0E  FULL_CHG_CAP  mAh
 *   0x10  AVG_CURRENT   signed mA
 *   0x12  STBY_CURRENT  signed mA
 *   0x14  MAX_LOAD_CUR  signed mA
 *   0x18  AVG_POWER     signed mW
 *   0x1C  SOC           %
 *   0x1E  INT_TEMP      0.1°K
 *   0x20  SOH           % (low byte) + status (high byte)
 *   0x28  REM_CAP_UNFIL mAh
 *   0x2A  REM_CAP_FIL   mAh
 *   0x2C  FULL_CAP_UNFIL mAh
 *   0x2E  FULL_CAP_FIL  mAh
 *   0x30  SOC_UNFIL     %
 *
 * Control Subcommands (write to 0x00, read from 0x00):
 *   0x0000  CONTROL_STATUS
 *   0x0001  DEVICE_TYPE   → 0x0421
 *   0x0002  FW_VERSION    → 0x0109
 *   0x0008  CHEM_ID       → 0x0128
 *   0x0013  SET_CFGUPDATE
 *   0x0042  SOFT_RESET
 *   0x0043  EXIT_CFGUPDATE
 *
 * Extended Data (0x3A–0x61):
 *   0x3A  OPCONFIG      Operating configuration (word)
 *   0x3C  DESIGN_CAP    Design capacity (word)
 *   0x3E  DATACLASS
 *   0x3F  DATABLOCK
 *   0x40–0x5F BLOCKDATA (32 bytes)
 *   0x60  CHECKSUM
 *   0x61  CONTROL       BlockDataControl
 *
 * Flags Register (0x06) bits:
 *   Bit 0:  DSG  (Discharging)
 *   Bit 1:  SOCF (SOC final threshold)
 *   Bit 2:  SOC1 (SOC threshold 1)
 *   Bit 3:  BAT_DET (Battery detected)
 *   Bit 4:  CFGUPMODE (Config update mode)
 *   Bit 5:  ITPOR (POR detected)
 *   Bit 7:  OCVTAKEN (OCV measurement complete)
 *   Bit 8:  CHG  (Charging detected)
 *   Bit 9:  FC   (Full charge)
 *   Bit 14: UT   (Under-temperature)
 *   Bit 15: OT   (Over-temperature)
 *
 * Slider controls:
 *   "soc"     0–100 %
 *   "voltage" 2500–4500 mV
 *   "current" -2000..2000 mA (signed)
 *   "temp"    -40..85 °C
 *
 * Pin: GPOUT (active-low battery alert output)
 */

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Constants ────────────────────────────────────────────────────────── */

#define BQ27441_ADDR          0x55

/* Standard command register addresses */
#define REG_CONTROL           0x00
#define REG_TEMPERATURE       0x02
#define REG_VOLTAGE           0x04
#define REG_FLAGS             0x06
#define REG_NOM_AVAIL_CAP     0x08
#define REG_FULL_AVAIL_CAP    0x0A
#define REG_REM_CAP           0x0C
#define REG_FULL_CHG_CAP      0x0E
#define REG_AVG_CURRENT       0x10
#define REG_STBY_CURRENT      0x12
#define REG_MAX_LOAD_CURRENT  0x14
#define REG_AVG_POWER         0x18
#define REG_SOC               0x1C
#define REG_INT_TEMP          0x1E
#define REG_SOH               0x20
#define REG_REM_CAP_UNFIL     0x28
#define REG_REM_CAP_FIL       0x2A
#define REG_FULL_CAP_UNFIL    0x2C
#define REG_FULL_CAP_FIL      0x2E
#define REG_SOC_UNFIL         0x30

/* Extended data register addresses */
#define REG_OPCONFIG          0x3A
#define REG_DESIGN_CAP        0x3C
#define REG_DATACLASS         0x3E
#define REG_DATABLOCK         0x3F
#define REG_BLOCKDATA         0x40  /* 0x40–0x5F = 32 bytes */
#define REG_CHECKSUM          0x60
#define REG_BLOCK_CONTROL     0x61

/* Control subcommand IDs */
#define CTL_CONTROL_STATUS    0x0000
#define CTL_DEVICE_TYPE       0x0001
#define CTL_FW_VERSION        0x0002
#define CTL_DM_CODE           0x0004
#define CTL_PREV_MACWRITE     0x0007
#define CTL_CHEM_ID           0x0008
#define CTL_BAT_INSERT        0x000C
#define CTL_BAT_REMOVE        0x000D
#define CTL_SET_HIBERNATE     0x0011
#define CTL_CLEAR_HIBERNATE   0x0012
#define CTL_SET_CFGUPDATE     0x0013
#define CTL_SHUTDOWN_ENABLE   0x001B
#define CTL_SHUTDOWN          0x001C
#define CTL_SEALED            0x0020
#define CTL_TOGGLE_GPOUT      0x0023
#define CTL_RESET             0x0041
#define CTL_SOFT_RESET        0x0042
#define CTL_EXIT_CFGUPDATE    0x0043
#define CTL_EXIT_RESIM        0x0044
#define CTL_UNSEAL_KEY        0x8000

/* Flags register bits (matches TI datasheet Table 4-4) */
#define FLAG_DSG              (1 << 0)
#define FLAG_SOCF             (1 << 1)
#define FLAG_SOC1             (1 << 2)
#define FLAG_BAT_DET          (1 << 3)
#define FLAG_CFGUPMODE        (1 << 4)
#define FLAG_ITPOR            (1 << 5)
#define FLAG_OCVTAKEN         (1 << 7)
#define FLAG_CHG              (1 << 8)
#define FLAG_FC               (1 << 9)
#define FLAG_UT               (1 << 14)
#define FLAG_OT               (1 << 15)

/* Control status bits */
#define STATUS_SS             (1 << 13)  /* Sealed status */
#define STATUS_CALMODE        (1 << 12)
#define STATUS_CCA            (1 << 11)
#define STATUS_BCA            (1 << 10)
#define STATUS_QMAX_UP        (1 << 9)
#define STATUS_RES_UP         (1 << 8)
#define STATUS_INITCOMP       (1 << 7)
#define STATUS_HIBERNATE      (1 << 6)
#define STATUS_SLEEP          (1 << 4)
#define STATUS_LDMD           (1 << 3)
#define STATUS_RUP_DIS        (1 << 2)
#define STATUS_VOK            (1 << 1)

/* Register space size */
#define REG_SPACE_SIZE        0x62

/* Design capacity default (mAh) */
#define DESIGN_CAPACITY       3000

/* Extended data class IDs */
#define CLASS_STATE           82
#define CLASS_DISCHARGE       49
#define CLASS_REGISTERS       64

/* ── Chip State ───────────────────────────────────────────────────────── */

typedef struct {
  /* Pins */
  pin_t pin_gpout;

  /* I2C state */
  uint8_t  addr_ptr;       /* current register address pointer */
  int      write_count;    /* bytes written in current transaction */

  /* Register space */
  uint8_t  regs[REG_SPACE_SIZE];

  /* Control subcommand handling */
  uint16_t ctl_subcmd;     /* last written subcommand */
  uint16_t ctl_data;       /* result of last subcommand */

  /* Device state */
  uint16_t control_status;
  bool     cfg_update_mode;
  bool     sealed;
  uint8_t  unseal_step;    /* 0 = locked, 1 = first key received */
  bool     itpor;          /* POR (Power-On Reset) flag */
  uint16_t opconfig;       /* OpConfig register (0x3A) */

  /* Extended data block */
  uint8_t  data_class;
  uint8_t  data_block;
  uint8_t  block_data[32];

  /* Attributes (sliders) */
  uint32_t attr_soc;
  uint32_t attr_voltage;
  uint32_t attr_current;
  uint32_t attr_temp;

  /* Periodic update timer */
  timer_t  update_timer;

} chip_state_t;

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void write_word(chip_state_t *chip, uint8_t reg, uint16_t value) {
  if (reg + 1 < REG_SPACE_SIZE) {
    chip->regs[reg]     = (uint8_t)(value & 0xFF);
    chip->regs[reg + 1] = (uint8_t)((value >> 8) & 0xFF);
  }
}

static uint16_t read_word(chip_state_t *chip, uint8_t reg) {
  if (reg + 1 < REG_SPACE_SIZE) {
    return (uint16_t)chip->regs[reg] | ((uint16_t)chip->regs[reg + 1] << 8);
  }
  return 0;
}

/* ── Control Subcommand Processing ────────────────────────────────────── */

static void process_control_subcmd(chip_state_t *chip) {
  switch (chip->ctl_subcmd) {

    case CTL_DEVICE_TYPE:
      chip->ctl_data = 0x0421;
      break;

    case CTL_FW_VERSION:
      chip->ctl_data = 0x0109;
      break;

    case CTL_CONTROL_STATUS:
      chip->ctl_data = chip->control_status;
      break;

    case CTL_CHEM_ID:
      chip->ctl_data = 0x0128;
      break;

    case CTL_DM_CODE:
      chip->ctl_data = 0x000E;
      break;

    case CTL_SET_CFGUPDATE:
      chip->cfg_update_mode = true;
      chip->ctl_data = chip->control_status;
      break;

    case CTL_EXIT_CFGUPDATE:
      chip->cfg_update_mode = false;
      chip->ctl_data = chip->control_status;
      break;

    case CTL_SOFT_RESET:
      chip->cfg_update_mode = false;
      chip->itpor = false;  /* SOFT_RESET clears ITPOR */
      chip->ctl_data = chip->control_status;
      break;

    case CTL_EXIT_RESIM:
      chip->cfg_update_mode = false;
      chip->ctl_data = chip->control_status;
      break;

    case CTL_RESET:
      chip->cfg_update_mode = false;
      chip->itpor = true;   /* Hardware reset sets ITPOR */
      chip->sealed = true;
      chip->unseal_step = 0;
      chip->control_status = STATUS_SS | STATUS_INITCOMP | STATUS_VOK;
      chip->ctl_data = chip->control_status;
      break;

    case CTL_SEALED:
      chip->sealed = true;
      chip->unseal_step = 0;
      chip->control_status |= STATUS_SS;
      chip->ctl_data = chip->control_status;
      break;

    case CTL_UNSEAL_KEY:
      /* Real BQ27441 requires the unseal key (0x8000) sent twice */
      if (chip->unseal_step == 0) {
        chip->unseal_step = 1;
      } else {
        chip->sealed = false;
        chip->unseal_step = 0;
        chip->control_status &= ~STATUS_SS;
      }
      chip->ctl_data = chip->control_status;
      break;

    case CTL_TOGGLE_GPOUT:
      pin_write(chip->pin_gpout, !pin_read(chip->pin_gpout));
      chip->ctl_data = chip->control_status;
      break;

    case CTL_BAT_INSERT:
    case CTL_BAT_REMOVE:
    case CTL_SHUTDOWN_ENABLE:
    case CTL_SHUTDOWN:
      chip->ctl_data = chip->control_status;
      break;

    case CTL_SET_HIBERNATE:
      chip->control_status |= STATUS_HIBERNATE;
      chip->ctl_data = chip->control_status;
      break;

    case CTL_CLEAR_HIBERNATE:
      chip->control_status &= ~STATUS_HIBERNATE;
      chip->ctl_data = chip->control_status;
      break;

    default:
      chip->ctl_data = chip->control_status;
      break;
  }
}

/* ── Extended Data Block Handling ─────────────────────────────────────── */

static void load_block_data(chip_state_t *chip) {
  /* Fill block_data with defaults based on class/block */
  memset(chip->block_data, 0, 32);

  if (chip->data_class == CLASS_STATE && chip->data_block == 0) {
    /* Offset 10-11: Design Capacity (MSB first in block) */
    chip->block_data[10] = (DESIGN_CAPACITY >> 8) & 0xFF;
    chip->block_data[11] = DESIGN_CAPACITY & 0xFF;
    /* Offset 12-13: Design Energy (mWh ≈ capacity * 3.7V) */
    uint16_t energy = (uint16_t)((uint32_t)DESIGN_CAPACITY * 37 / 10);
    chip->block_data[12] = (energy >> 8) & 0xFF;
    chip->block_data[13] = energy & 0xFF;
    /* Offset 16-17: Terminate Voltage (mV) */
    chip->block_data[16] = 0x0B;  /* 2800 mV = 0x0AF0 */
    chip->block_data[17] = 0x30;
    /* Offset 26: SOCI Delta (1%) */
    chip->block_data[26] = 1;
    /* Offset 27-28: Taper Rate */
    chip->block_data[27] = 0x00;
    chip->block_data[28] = 0xC8; /* 200 = 0x00C8 */
  } else if (chip->data_class == CLASS_DISCHARGE && chip->data_block == 0) {
    /* Offset 0: SOC1 Set Threshold */
    chip->block_data[0] = 15;
    /* Offset 1: SOC1 Clear Threshold */
    chip->block_data[1] = 20;
    /* Offset 2: SOCF Set Threshold */
    chip->block_data[2] = 5;
    /* Offset 3: SOCF Clear Threshold */
    chip->block_data[3] = 10;
  } else if (chip->data_class == CLASS_REGISTERS && chip->data_block == 0) {
    /* Offset 0-1: OpConfig (use current value) */
    chip->block_data[0] = (chip->opconfig >> 8) & 0xFF;
    chip->block_data[1] = chip->opconfig & 0xFF;
  }

  /* Copy to register space at 0x40-0x5F */
  memcpy(&chip->regs[REG_BLOCKDATA], chip->block_data, 32);
}

static uint8_t compute_block_checksum(chip_state_t *chip) {
  uint8_t sum = 0;
  for (int i = 0; i < 32; i++) {
    sum += chip->regs[REG_BLOCKDATA + i];
  }
  return 255 - sum;
}

/* ── Register Update (periodic timer) ────────────────────────────────── */

static void update_registers(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  int32_t soc        = (int32_t)attr_read(chip->attr_soc);
  int32_t voltage_mv = (int32_t)attr_read(chip->attr_voltage);
  int32_t current_ma = (int32_t)attr_read(chip->attr_current);
  int32_t temp_c     = (int32_t)attr_read(chip->attr_temp);

  /* Clamp */
  if (soc < 0) soc = 0;
  if (soc > 100) soc = 100;
  if (voltage_mv < 0) voltage_mv = 0;

  /* Temperature: BQ27441 uses 0.1°K units → (°C + 273.15) × 10 */
  uint16_t temp_01k = (uint16_t)((temp_c + 273) * 10 + 2); /* +2 for ~0.15 */

  /* Capacity derivations */
  uint16_t full_cap = DESIGN_CAPACITY;
  uint16_t rem_cap  = (uint16_t)((uint32_t)full_cap * (uint32_t)soc / 100);

  /* Average power: mA × mV / 1000 = mW */
  int16_t avg_power = (int16_t)((int32_t)current_ma * voltage_mv / 1000);

  /* State of Health: 100% in simulation */
  uint16_t soh = 0x0064; /* Low byte = 100%, high byte = 0 (status OK) */

  /* Flags */
  uint16_t flags = FLAG_BAT_DET;  /* battery always detected in sim */
  if (current_ma < 0)  flags |= FLAG_DSG;
  if (current_ma > 0)  flags |= FLAG_CHG;
  if (soc >= 100)      flags |= FLAG_FC;
  if (soc <= 15)       flags |= FLAG_SOC1;
  if (soc <= 5)        flags |= FLAG_SOCF;
  if (temp_c < -20)    flags |= FLAG_UT;
  if (temp_c > 60)     flags |= FLAG_OT;
  if (chip->cfg_update_mode) flags |= FLAG_CFGUPMODE;
  if (chip->itpor) flags |= FLAG_ITPOR;
  flags |= FLAG_OCVTAKEN;  /* OCV measurement always complete in sim */

  /* Write standard registers */
  write_word(chip, REG_TEMPERATURE,      temp_01k);
  write_word(chip, REG_VOLTAGE,          (uint16_t)voltage_mv);
  write_word(chip, REG_FLAGS,            flags);
  write_word(chip, REG_NOM_AVAIL_CAP,    rem_cap);
  write_word(chip, REG_FULL_AVAIL_CAP,   full_cap);
  write_word(chip, REG_REM_CAP,          rem_cap);
  write_word(chip, REG_FULL_CHG_CAP,     full_cap);
  write_word(chip, REG_AVG_CURRENT,      (uint16_t)(int16_t)current_ma);
  write_word(chip, REG_STBY_CURRENT,     (uint16_t)(int16_t)(current_ma < 0 ? current_ma / 4 : 0));
  write_word(chip, REG_MAX_LOAD_CURRENT, (uint16_t)(int16_t)(current_ma < 0 ? current_ma : 0));
  write_word(chip, REG_AVG_POWER,        (uint16_t)avg_power);
  write_word(chip, REG_SOC,              (uint16_t)soc);
  write_word(chip, REG_INT_TEMP,         temp_01k);
  write_word(chip, REG_SOH,              soh);

  /* Unfiltered/filtered copies */
  write_word(chip, REG_REM_CAP_UNFIL,    rem_cap);
  write_word(chip, REG_REM_CAP_FIL,      rem_cap);
  write_word(chip, REG_FULL_CAP_UNFIL,   full_cap);
  write_word(chip, REG_FULL_CAP_FIL,     full_cap);
  write_word(chip, REG_SOC_UNFIL,        (uint16_t)soc);

  /* Extended data registers (directly readable) */
  write_word(chip, REG_OPCONFIG,         chip->opconfig);
  write_word(chip, REG_DESIGN_CAP,       DESIGN_CAPACITY);

  /* Update control status */
  chip->control_status &= ~(STATUS_SLEEP);
  chip->control_status |= STATUS_INITCOMP | STATUS_VOK;
  if (chip->sealed) {
    chip->control_status |= STATUS_SS;
  } else {
    chip->control_status &= ~STATUS_SS;
  }

  /* GPOUT: assert low when SOC below SOCF threshold or battery low */
  if (soc <= 5) {
    pin_write(chip->pin_gpout, LOW);
  } else {
    pin_write(chip->pin_gpout, HIGH);
  }
}

/* ── I2C Callbacks ────────────────────────────────────────────────────── */

static bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (connect) {
    chip->write_count = 0;
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  uint8_t val;

  /* Control register reads return subcommand result */
  if (chip->addr_ptr == 0x00) {
    val = (uint8_t)(chip->ctl_data & 0xFF);
    chip->addr_ptr = 0x01;
    return val;
  }
  if (chip->addr_ptr == 0x01) {
    val = (uint8_t)((chip->ctl_data >> 8) & 0xFF);
    chip->addr_ptr = 0x02;
    return val;
  }

  /* Standard / extended registers */
  if (chip->addr_ptr < REG_SPACE_SIZE) {
    val = chip->regs[chip->addr_ptr];
    chip->addr_ptr++;
    return val;
  }

  return 0xFF;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->write_count == 0) {
    /* First byte = register address */
    chip->addr_ptr = data;
    chip->write_count = 1;
    return true;
  }

  /* Subsequent bytes = data writes */
  if (chip->addr_ptr == REG_CONTROL && chip->write_count == 1) {
    /* Control subcommand LSB */
    chip->ctl_subcmd = (chip->ctl_subcmd & 0xFF00) | data;
    chip->write_count = 2;
  } else if (chip->addr_ptr == REG_CONTROL && chip->write_count == 2) {
    /* Control subcommand MSB — process immediately */
    chip->ctl_subcmd = (chip->ctl_subcmd & 0x00FF) | ((uint16_t)data << 8);
    process_control_subcmd(chip);
    chip->write_count = 3;
  } else if (chip->addr_ptr == 0x01 && chip->write_count == 1) {
    /* Writing to 0x01 (MSB of control) */
    chip->ctl_subcmd = (chip->ctl_subcmd & 0x00FF) | ((uint16_t)data << 8);
    process_control_subcmd(chip);
    chip->write_count = 2;
  } else if (chip->addr_ptr == REG_DATACLASS) {
    /* Data class selection */
    chip->data_class = data;
    chip->write_count++;
  } else if (chip->addr_ptr == REG_DATABLOCK) {
    /* Data block selection — triggers block load */
    chip->data_block = data;
    load_block_data(chip);
    chip->regs[REG_CHECKSUM] = compute_block_checksum(chip);
    chip->write_count++;
  } else if (chip->addr_ptr == REG_BLOCK_CONTROL) {
    /* BlockDataControl enable — just acknowledge */
    chip->write_count++;
  } else if (chip->addr_ptr == REG_CHECKSUM) {
    /* Checksum write — apply block data changes */
    chip->regs[REG_CHECKSUM] = data;
    /* If class 64 was written, apply OpConfig changes */
    if (chip->data_class == CLASS_REGISTERS && chip->data_block == 0) {
      chip->opconfig = ((uint16_t)chip->block_data[0] << 8) | chip->block_data[1];
    }
    chip->write_count++;
  } else if (chip->addr_ptr >= REG_BLOCKDATA && chip->addr_ptr < REG_BLOCKDATA + 32) {
    /* Write to block data area */
    chip->regs[chip->addr_ptr] = data;
    chip->block_data[chip->addr_ptr - REG_BLOCKDATA] = data;
    chip->addr_ptr++;
    chip->write_count++;
  } else {
    /* Generic register write with auto-increment */
    if (chip->addr_ptr < REG_SPACE_SIZE) {
      chip->regs[chip->addr_ptr] = data;
    }
    chip->addr_ptr++;
    chip->write_count++;
  }

  return true;
}

static void on_i2c_disconnect(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  chip->write_count = 0;
}

/* ── Chip Init ────────────────────────────────────────────────────────── */

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));

  /* Pin: only GPOUT is functionally relevant */
  pin_init("VDD", INPUT);
  pin_init("GND", INPUT);
  pin_init("SCL", INPUT);
  pin_t pin_sda  = pin_init("SDA", INPUT);
  pin_t pin_scl  = pin_init("SCL", INPUT);
  chip->pin_gpout = pin_init("GPOUT", OUTPUT);
  pin_init("BIN", INPUT);

  pin_write(chip->pin_gpout, HIGH);  /* GPOUT idle high */

  /* Attributes (sliders) */
  chip->attr_soc     = attr_init("soc", 50);
  chip->attr_voltage = attr_init("voltage", 3700);
  chip->attr_current = attr_init("current", 0);
  chip->attr_temp    = attr_init("temp", 25);

  /* Initial device state */
  chip->sealed = true;
  chip->unseal_step = 0;
  chip->itpor = true;  /* POR flag set on power-on */
  chip->cfg_update_mode = false;
  chip->control_status = STATUS_SS | STATUS_INITCOMP | STATUS_VOK;
  chip->opconfig = 0x2F80;  /* default OpConfig */

  /* Initial register update */
  memset(chip->regs, 0, REG_SPACE_SIZE);
  update_registers(chip);

  /* Periodic update timer (250 ms) */
  const timer_config_t timer_cfg = {
    .callback  = update_registers,
    .user_data = chip,
  };
  chip->update_timer = timer_init(&timer_cfg);
  timer_start(chip->update_timer, 250000, true);

  /* I2C bus */
  const i2c_config_t i2c_cfg = {
    .user_data   = chip,
    .address     = BQ27441_ADDR,
    .scl         = pin_scl,
    .sda         = pin_sda,
    .connect     = on_i2c_connect,
    .read        = on_i2c_read,
    .write       = on_i2c_write,
    .disconnect  = on_i2c_disconnect,
  };
  i2c_init(&i2c_cfg);

  printf("BQ27441 initialized at 0x%02X\n", BQ27441_ADDR);
}
