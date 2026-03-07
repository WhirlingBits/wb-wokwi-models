/*
 * ADS7142 Nanopower Dual-Channel 12-Bit SAR ADC – Wokwi Custom Chip
 *
 * Simulates the TI ADS7142 I2C ADC with:
 *   - Opcode-based protocol (SINGLE_READ/WRITE, SET_BIT, CLEAR_BIT,
 *     BLOCK_READ, BLOCK_WRITE)
 *   - Full register map matching the datasheet
 *   - 12-bit conversion results from two slider-controlled channels
 *   - Data FIFO with configurable output format
 *   - DATA_BUFFER_OPMODE: Stop-Burst, Start-Burst, Pre-Alert, Post-Alert
 *   - Digital Window Comparator (DWC) with hysteresis and ALERT pin
 *   - Accumulator with configurable sample count (1/4/8/16)
 *   - Pre-alert event counter
 *   - Device reset via WKEY / DEVICE_RESET sequence
 *   - Manual, Autonomous, and High-Precision operating modes
 *
 * I2C Address: 0x1F (default, R1=0 Ω / R2=DNP)
 */

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── I2C Address ────────────────────────────────────────────────── */
#define ADS7142_ADDR            0x1F

/* ── Opcodes ────────────────────────────────────────────────────── */
#define OP_SINGLE_WRITE         0x08
#define OP_SINGLE_READ          0x10
#define OP_SET_BIT              0x18
#define OP_CLEAR_BIT            0x20
#define OP_BLOCK_WRITE          0x28
#define OP_BLOCK_READ           0x30

/* ── Register Addresses ─────────────────────────────────────────── */

/* Status (read-only) */
#define REG_OPMODE_STATUS       0x00
#define REG_DATA_BUFFER_STATUS  0x01
#define REG_ACCUMULATOR_STATUS  0x02
#define REG_ALERT_TRIG_CHID     0x03
#define REG_SEQUENCE_STATUS     0x04

/* Accumulator data (read-only) */
#define REG_ACC_CH0_LSB         0x08
#define REG_ACC_CH0_MSB         0x09
#define REG_ACC_CH1_LSB         0x0A
#define REG_ACC_CH1_MSB         0x0B

/* Alert flags (write-1-to-clear) */
#define REG_ALERT_LOW_FLAGS     0x0C
#define REG_ALERT_HIGH_FLAGS    0x0E

/* Reset */
#define REG_DEVICE_RESET        0x14
#define REG_OFFSET_CAL          0x15
#define REG_WKEY                0x17

/* Oscillator & timing */
#define REG_OSC_SEL             0x18
#define REG_nCLK_SEL            0x19

/* Mode control */
#define REG_OPMODE_SEL          0x1C
#define REG_START_SEQUENCE      0x1E
#define REG_ABORT_SEQUENCE      0x1F

/* Sequencer */
#define REG_AUTO_SEQ_CHEN       0x20

/* Input configuration */
#define REG_CHANNEL_INPUT_CFG   0x24

/* Data output */
#define REG_DOUT_FORMAT_CFG     0x28

/* Data buffer */
#define REG_DATA_BUFFER_OPMODE  0x2C

/* Accumulator control */
#define REG_ACC_EN              0x30

/* Digital Window Comparator */
#define REG_ALERT_CHEN          0x34
#define REG_PRE_ALT_MAX_EVT_CNT 0x36
#define REG_ALERT_DWC_EN        0x37
#define REG_DWC_HTH_CH0_LSB    0x38
#define REG_DWC_HTH_CH0_MSB    0x39
#define REG_DWC_LTH_CH0_LSB    0x3A
#define REG_DWC_LTH_CH0_MSB    0x3B
#define REG_DWC_HTH_CH1_LSB    0x3C
#define REG_DWC_HTH_CH1_MSB    0x3D
#define REG_DWC_LTH_CH1_LSB    0x3E
#define REG_DWC_LTH_CH1_MSB    0x3F
#define REG_DWC_HYS_CH0        0x40
#define REG_DWC_HYS_CH1        0x41

#define REG_SPACE_SIZE          0x42

/* ── FIFO ───────────────────────────────────────────────────────── */
#define FIFO_MAX_SAMPLES        32

/* ── VREF (AVDD) ────────────────────────────────────────────────── */
#define VREF                    3.3f
#define ADC_MAX                 4095

/* ── DATA_BUFFER_OPMODE values ──────────────────────────────────── */
#define DBUF_STOP_BURST         0x00
#define DBUF_START_BURST        0x01
#define DBUF_PRE_ALERT          0x04
#define DBUF_POST_ALERT         0x06

/* ── I2C Write State Machine ────────────────────────────────────── */
enum {
    STATE_IDLE = 0,         /* Expect opcode byte              */
    STATE_EXPECT_ADDR,      /* Expect register / count         */
    STATE_EXPECT_DATA,      /* Expect data byte                */
    STATE_BLOCK_WRITE_DATA, /* Receiving block-write bytes     */
};

/* ── Chip State ─────────────────────────────────────────────────── */
typedef struct {
    pin_t   pin_alert;

    uint32_t attr_v0;
    uint32_t attr_v1;

    uint8_t regs[REG_SPACE_SIZE];

    /* Conversion FIFO (each sample = 2 bytes MSB,LSB) */
    uint8_t fifo[FIFO_MAX_SAMPLES * 2];
    uint8_t fifo_count;         /* samples currently in FIFO  */
    uint8_t fifo_rd_idx;        /* byte-level read index      */

    /* I2C protocol state */
    uint8_t state;
    uint8_t current_opcode;
    uint8_t current_reg;
    uint8_t block_write_remaining; /* bytes left in block write */

    /* Device state */
    bool    sequence_running;
    bool    wkey_unlocked;

    /* Accumulator state (32-bit to support sum of up to 16 × 12-bit) */
    uint32_t acc_ch0;
    uint32_t acc_ch1;
    uint8_t  acc_sample_count_ch0;
    uint8_t  acc_sample_count_ch1;

    /* Pre-alert event counter */
    uint8_t  pre_alert_evt_count;
    bool     alert_triggered;       /* latched alert state */

    timer_t convert_timer;
} chip_state_t;

/* ── Forward declarations ───────────────────────────────────────── */
static void run_conversion(chip_state_t *chip);
static void check_alerts(chip_state_t *chip, uint16_t adc0, uint16_t adc1);
static void reset_device(chip_state_t *chip);
static void timer_callback(void *user_data);

/* ── Helpers ────────────────────────────────────────────────────── */

static uint16_t voltage_to_adc(float v) {
    int32_t raw = (int32_t)((v / VREF) * ADC_MAX + 0.5f);
    if (raw < 0)      raw = 0;
    if (raw > ADC_MAX) raw = ADC_MAX;
    return (uint16_t)raw;
}

/* Return the number of accumulation samples from ACC_EN bits [3:2]. */
static uint8_t acc_num_samples(uint8_t acc_en_val) {
    switch ((acc_en_val >> 2) & 0x03) {
    case 0: return 1;
    case 1: return 4;
    case 2: return 8;
    case 3: return 16;
    }
    return 1;
}

/* ── I2C Callbacks ──────────────────────────────────────────────── */

static bool on_i2c_connect(void *user_data, uint32_t address, bool connect) {
    chip_state_t *chip = (chip_state_t *)user_data;
    (void)address;

    if (connect) {
        /* Reset FIFO read pointer at the start of every read transaction
           so that a raw i2c_master_receive() always starts from sample 0. */
        if (chip->current_opcode != OP_SINGLE_READ) {
            chip->fifo_rd_idx = 0;
        }
    }
    /* Keep current_opcode across STOP → START so that a SINGLE_REG_READ
       write-phase ([opcode][addr] STOP) is still remembered in the
       following read-phase (START [addr+R] [data] STOP).               */
    return true;
}

static uint8_t on_i2c_read(void *user_data) {
    chip_state_t *chip = (chip_state_t *)user_data;

    /* SINGLE_REG_READ: return addressed register */
    if (chip->current_opcode == OP_SINGLE_READ) {
        if (chip->current_reg < REG_SPACE_SIZE) {
            return chip->regs[chip->current_reg];
        }
        return 0x00;
    }

    /* BLOCK_READ or raw read: return FIFO data */
    if (chip->fifo_rd_idx < (uint16_t)chip->fifo_count * 2) {
        return chip->fifo[chip->fifo_rd_idx++];
    }

    return 0x00;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
    chip_state_t *chip = (chip_state_t *)user_data;

    switch (chip->state) {

    /* ── Byte 0: Opcode ─────────────────────────────────────────── */
    case STATE_IDLE:
        chip->current_opcode = data;
        chip->state = STATE_EXPECT_ADDR;
        break;

    /* ── Byte 1: Register address (or block count) ──────────────── */
    case STATE_EXPECT_ADDR:
        if (chip->current_opcode == OP_BLOCK_READ) {
            /* Byte is the sample count — prepare FIFO read pointer */
            chip->fifo_rd_idx = 0;
            chip->state = STATE_IDLE;
        } else if (chip->current_opcode == OP_BLOCK_WRITE) {
            /* First byte after opcode = starting register address */
            chip->current_reg = data;
            chip->state = STATE_BLOCK_WRITE_DATA;
        } else {
            chip->current_reg = data;
            if (chip->current_opcode == OP_SINGLE_READ) {
                /* Write phase done; read follows after STOP+START */
                chip->state = STATE_IDLE;
            } else {
                /* SINGLE_WRITE / SET_BIT / CLEAR_BIT need a data byte */
                chip->state = STATE_EXPECT_DATA;
            }
        }
        break;

    /* ── Block-write data bytes (auto-increment register) ───────── */
    case STATE_BLOCK_WRITE_DATA:
    {
        uint8_t reg = chip->current_reg;
        if (reg < REG_SPACE_SIZE) {
            chip->regs[reg] = data;
        }
        chip->current_reg = reg + 1;
        /* Stay in this state until STOP (disconnect resets to IDLE) */
        break;
    }

    /* ── Byte 2: Data ───────────────────────────────────────────── */
    case STATE_EXPECT_DATA:
    {
        uint8_t reg = chip->current_reg;
        if (reg < REG_SPACE_SIZE) {

            if (chip->current_opcode == OP_SINGLE_WRITE) {
                /* ── Special register handling ────────────────────── */
                if (reg == REG_WKEY) {
                    chip->wkey_unlocked = (data == 0x0A);
                    chip->regs[reg] = data;
                } else if (reg == REG_DEVICE_RESET) {
                    if (chip->wkey_unlocked && data == 0x01) {
                        reset_device(chip);
                    }
                } else if (reg == REG_OFFSET_CAL) {
                    /* Offset calibration acknowledged (no-op in sim) */
                    chip->regs[reg] = data;
                } else if (reg == REG_START_SEQUENCE) {
                    if (data & 0x01) {
                        chip->sequence_running = true;
                        /* Real device: 0x02 = sequence enabled, no error */
                        chip->regs[REG_SEQUENCE_STATUS] = 0x02;
                        /* Reset pre-alert event counter */
                        chip->pre_alert_evt_count = 0;
                        chip->alert_triggered = false;
                        /* Reset accumulators */
                        chip->acc_ch0 = 0;
                        chip->acc_ch1 = 0;
                        chip->acc_sample_count_ch0 = 0;
                        chip->acc_sample_count_ch1 = 0;
                        /* Run an immediate conversion */
                        run_conversion(chip);
                    }
                } else if (reg == REG_ABORT_SEQUENCE) {
                    if (data & 0x01) {
                        chip->sequence_running = false;
                        chip->regs[REG_SEQUENCE_STATUS] = 0x00;
                    }
                } else if (reg == REG_ALERT_LOW_FLAGS || reg == REG_ALERT_HIGH_FLAGS) {
                    /* Write-1-to-clear */
                    chip->regs[reg] &= ~data;
                    /* Re-evaluate alert pin after clearing flags */
                    if (chip->regs[REG_ALERT_LOW_FLAGS] == 0 &&
                        chip->regs[REG_ALERT_HIGH_FLAGS] == 0) {
                        pin_write(chip->pin_alert, HIGH);
                        chip->alert_triggered = false;
                    }
                } else {
                    chip->regs[reg] = data;
                }

            } else if (chip->current_opcode == OP_SET_BIT) {
                chip->regs[reg] |= data;
            } else if (chip->current_opcode == OP_CLEAR_BIT) {
                chip->regs[reg] &= ~data;
            }
        }
        chip->state = STATE_IDLE;
        break;
    }

    default:
        chip->state = STATE_IDLE;
        break;
    }

    return true;
}

static void on_i2c_disconnect(void *user_data) {
    chip_state_t *chip = (chip_state_t *)user_data;
    /* Reset write state machine but keep current_opcode for read phase */
    chip->state = STATE_IDLE;
}

/* ── Conversion Logic ───────────────────────────────────────────── */

static void push_fifo_sample(chip_state_t *chip, uint16_t sample) {
    if (chip->fifo_count < FIFO_MAX_SAMPLES) {
        chip->fifo[chip->fifo_count * 2]     = (sample >> 8) & 0xFF;
        chip->fifo[chip->fifo_count * 2 + 1] =  sample       & 0xFF;
        chip->fifo_count++;
    }
}

static void run_conversion(chip_state_t *chip) {
    float v0 = attr_read_float(chip->attr_v0);
    float v1 = attr_read_float(chip->attr_v1);

    uint16_t adc0 = voltage_to_adc(v0);
    uint16_t adc1 = voltage_to_adc(v1);

    uint8_t ch_en    = chip->regs[REG_AUTO_SEQ_CHEN];
    uint8_t dout_fmt = chip->regs[REG_DOUT_FORMAT_CFG];
    uint8_t dbuf_mode = chip->regs[REG_DATA_BUFFER_OPMODE];

    /* In pre-alert mode, stop filling FIFO after alert triggers */
    if (dbuf_mode == DBUF_PRE_ALERT && chip->alert_triggered) {
        return;
    }

    /* In stop-burst mode, don't push samples to FIFO */
    bool push_to_fifo = (dbuf_mode != DBUF_STOP_BURST);

    /* In post-alert mode, only start filling FIFO after an alert */
    if (dbuf_mode == DBUF_POST_ALERT && !chip->alert_triggered) {
        push_to_fifo = false;
    }

    if (push_to_fifo) {
        /* CH0 */
        if (ch_en & 0x01) {
            uint16_t sample;
            if (dout_fmt == 0x02) {
                /* Format 2: [12-bit ADC][3-bit chID=000][DATA_VALID=1] */
                sample = (adc0 << 4) | (0 << 1) | 1;
            } else {
                /* Default: 12-bit left-aligned */
                sample = adc0 << 4;
            }
            push_fifo_sample(chip, sample);
        }

        /* CH1 */
        if (ch_en & 0x02) {
            uint16_t sample;
            if (dout_fmt == 0x02) {
                /* Format 2: [12-bit ADC][3-bit chID=001][DATA_VALID=1] */
                sample = (adc1 << 4) | (1 << 1) | 1;
            } else {
                sample = adc1 << 4;
            }
            push_fifo_sample(chip, sample);
        }
    }

    /* Update data buffer status */
    chip->regs[REG_DATA_BUFFER_STATUS] = chip->fifo_count;

    /* ── Accumulator ──────────────────────────────────────────────── */
    uint8_t acc_en = chip->regs[REG_ACC_EN];
    uint8_t acc_target = acc_num_samples(acc_en);

    if (acc_en & 0x01) {
        chip->acc_ch0 += adc0;
        chip->acc_sample_count_ch0++;
        if (chip->acc_sample_count_ch0 >= acc_target) {
            chip->regs[REG_ACC_CH0_LSB] =  chip->acc_ch0       & 0xFF;
            chip->regs[REG_ACC_CH0_MSB] = (chip->acc_ch0 >> 8) & 0xFF;
            chip->regs[REG_ACCUMULATOR_STATUS] |= 0x01; /* CH0 data ready */
            chip->acc_ch0 = 0;
            chip->acc_sample_count_ch0 = 0;
        }
    }
    if (acc_en & 0x02) {
        chip->acc_ch1 += adc1;
        chip->acc_sample_count_ch1++;
        if (chip->acc_sample_count_ch1 >= acc_target) {
            chip->regs[REG_ACC_CH1_LSB] =  chip->acc_ch1       & 0xFF;
            chip->regs[REG_ACC_CH1_MSB] = (chip->acc_ch1 >> 8) & 0xFF;
            chip->regs[REG_ACCUMULATOR_STATUS] |= 0x02; /* CH1 data ready */
            chip->acc_ch1 = 0;
            chip->acc_sample_count_ch1 = 0;
        }
    }

    /* OPMODE_I2CMODE_STATUS: bit 5..4 = opmode echoed, bit 0 = I2C mode active */
    chip->regs[REG_OPMODE_STATUS] =
        ((chip->regs[REG_OPMODE_SEL] & 0x07) << 4) | (chip->sequence_running ? 0x01 : 0x00);

    check_alerts(chip, adc0, adc1);
}

/* ── Digital Window Comparator (with hysteresis) ────────────────── */

static void check_alerts(chip_state_t *chip, uint16_t adc0, uint16_t adc1) {
    if (!(chip->regs[REG_ALERT_DWC_EN] & 0x01)) {
        pin_write(chip->pin_alert, HIGH);
        return;
    }

    bool alert_now = false;

    /* ── CH0 ──────────────────────────────────────────────────────── */
    if (chip->regs[REG_ALERT_CHEN] & 0x01) {
        uint16_t hth = ((uint16_t)chip->regs[REG_DWC_HTH_CH0_MSB] << 8)
                     | chip->regs[REG_DWC_HTH_CH0_LSB];
        uint16_t lth = ((uint16_t)chip->regs[REG_DWC_LTH_CH0_MSB] << 8)
                     | chip->regs[REG_DWC_LTH_CH0_LSB];
        uint16_t hys = chip->regs[REG_DWC_HYS_CH0];

        /* High threshold with hysteresis:
           Alert sets when adc > hth, clears when adc < (hth - hys) */
        bool was_high = (chip->regs[REG_ALERT_HIGH_FLAGS] & 0x01) != 0;
        if (adc0 > hth || (was_high && hys > 0 && adc0 >= (hth > hys ? hth - hys : 0))) {
            chip->regs[REG_ALERT_HIGH_FLAGS] |= 0x01;
            chip->regs[REG_ALERT_TRIG_CHID]  |= 0x01;
            alert_now = true;
        }

        /* Low threshold with hysteresis:
           Alert sets when adc < lth, clears when adc > (lth + hys) */
        bool was_low = (chip->regs[REG_ALERT_LOW_FLAGS] & 0x01) != 0;
        if (adc0 < lth || (was_low && hys > 0 && adc0 <= (uint16_t)(lth + hys))) {
            chip->regs[REG_ALERT_LOW_FLAGS]  |= 0x01;
            chip->regs[REG_ALERT_TRIG_CHID]  |= 0x01;
            alert_now = true;
        }
    }

    /* ── CH1 ──────────────────────────────────────────────────────── */
    if (chip->regs[REG_ALERT_CHEN] & 0x02) {
        uint16_t hth = ((uint16_t)chip->regs[REG_DWC_HTH_CH1_MSB] << 8)
                     | chip->regs[REG_DWC_HTH_CH1_LSB];
        uint16_t lth = ((uint16_t)chip->regs[REG_DWC_LTH_CH1_MSB] << 8)
                     | chip->regs[REG_DWC_LTH_CH1_LSB];
        uint16_t hys = chip->regs[REG_DWC_HYS_CH1];

        bool was_high = (chip->regs[REG_ALERT_HIGH_FLAGS] & 0x02) != 0;
        if (adc1 > hth || (was_high && hys > 0 && adc1 >= (hth > hys ? hth - hys : 0))) {
            chip->regs[REG_ALERT_HIGH_FLAGS] |= 0x02;
            chip->regs[REG_ALERT_TRIG_CHID]  |= 0x02;
            alert_now = true;
        }

        bool was_low = (chip->regs[REG_ALERT_LOW_FLAGS] & 0x02) != 0;
        if (adc1 < lth || (was_low && hys > 0 && adc1 <= (uint16_t)(lth + hys))) {
            chip->regs[REG_ALERT_LOW_FLAGS]  |= 0x02;
            chip->regs[REG_ALERT_TRIG_CHID]  |= 0x02;
            alert_now = true;
        }
    }

    /* Pre-alert event counting: in Pre-Alert mode, track events before
       actually triggering the ALERT pin. */
    if (alert_now && !chip->alert_triggered) {
        uint8_t dbuf_mode = chip->regs[REG_DATA_BUFFER_OPMODE];
        if (dbuf_mode == DBUF_PRE_ALERT) {
            uint8_t max_evt = chip->regs[REG_PRE_ALT_MAX_EVT_CNT];
            /* max_evt encodes the count in the upper nibble: n = (max_evt >> 4) */
            uint8_t threshold = (max_evt >> 4) & 0x0F;
            if (threshold == 0) threshold = 1;
            chip->pre_alert_evt_count++;
            if (chip->pre_alert_evt_count < threshold) {
                /* Not enough events yet — don't fire ALERT pin */
                alert_now = false;
            }
        }
    }

    if (alert_now) {
        chip->alert_triggered = true;
    }

    /* ALERT is active-low */
    pin_write(chip->pin_alert, chip->alert_triggered ? LOW : HIGH);
}

/* ── Timer Callback (periodic conversions in autonomous mode) ──── */

static void timer_callback(void *user_data) {
    chip_state_t *chip = (chip_state_t *)user_data;
    if (chip->sequence_running) {
        run_conversion(chip);
    }
}

/* ── Device Reset ───────────────────────────────────────────────── */

static void reset_device(chip_state_t *chip) {
    memset(chip->regs, 0, REG_SPACE_SIZE);
    chip->sequence_running = false;
    chip->wkey_unlocked    = false;
    chip->fifo_count       = 0;
    chip->fifo_rd_idx      = 0;
    chip->state            = STATE_IDLE;
    chip->current_opcode   = 0;
    chip->acc_ch0          = 0;
    chip->acc_ch1          = 0;
    chip->acc_sample_count_ch0 = 0;
    chip->acc_sample_count_ch1 = 0;
    chip->pre_alert_evt_count  = 0;
    chip->alert_triggered      = false;
    pin_write(chip->pin_alert, HIGH);
    printf("ADS7142 Device Reset\n");
}

/* ── Chip Init ──────────────────────────────────────────────────── */

void chip_init(void) {
    chip_state_t *chip = calloc(1, sizeof(chip_state_t));

    /* Pins — only ALERT needs to be stored (output) */
    pin_t pin_vdd  = pin_init("VDD",   INPUT);
    pin_t pin_gnd  = pin_init("GND",   INPUT);
    pin_t pin_ain0 = pin_init("AIN0",  ANALOG);
    pin_t pin_ain1 = pin_init("AIN1",  ANALOG);
    chip->pin_alert = pin_init("ALERT", OUTPUT);
    pin_write(chip->pin_alert, HIGH);   /* inactive (active-low) */

    (void)pin_vdd; (void)pin_gnd; (void)pin_ain0; (void)pin_ain1;

    /* Slider attributes (float, 0 – 3.3 V) */
    chip->attr_v0 = attr_init_float("v0", 0.0f);
    chip->attr_v1 = attr_init_float("v1", 0.0f);

    /* Periodic conversion timer (100 ms) */
    const timer_config_t timer_cfg = {
        .callback  = timer_callback,
        .user_data = chip,
    };
    chip->convert_timer = timer_init(&timer_cfg);
    timer_start(chip->convert_timer, 100000, true);

    /* I2C */
    const i2c_config_t i2c_cfg = {
        .user_data  = chip,
        .address    = ADS7142_ADDR,
        .scl        = pin_init("SCL", INPUT),
        .sda        = pin_init("SDA", INPUT),
        .connect    = on_i2c_connect,
        .read       = on_i2c_read,
        .write      = on_i2c_write,
        .disconnect = on_i2c_disconnect,
    };
    i2c_init(&i2c_cfg);

    printf("ADS7142 Initialized at 0x%02X\n", ADS7142_ADDR);
}
