/*
 * ADS7142 Nanopower SAR-ADC – Wokwi Simulation Example (ESP-IDF)
 *
 * Demonstrates the opcode-based I2C protocol of the TI ADS7142:
 *   1. Device reset via WKEY + DEVICE_RESET
 *   2. Manual-mode configuration (auto-seq on both channels)
 *   3. Periodic conversion reads
 *
 * Wiring (diagram.json):
 *   GPIO 4  → SDA
 *   GPIO 5  → SCL
 *   GPIO 6  → ALERT (optional)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ads7142-example";

/* ── I2C Configuration ────────────────────────────────────────────── */

#define I2C_MASTER_SCL_IO           5
#define I2C_MASTER_SDA_IO           4
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

/* ── ADS7142 Constants ────────────────────────────────────────────── */

#define ADS7142_ADDR                0x1F

/* Opcodes */
#define OP_SINGLE_WRITE             0x08
#define OP_SINGLE_READ              0x10
#define OP_BLOCK_READ               0x30

/* Registers */
#define REG_OPMODE_STATUS           0x00
#define REG_DATA_BUFFER_STATUS      0x01
#define REG_SEQUENCE_STATUS         0x04
#define REG_DEVICE_RESET            0x14
#define REG_OFFSET_CAL              0x15
#define REG_WKEY                    0x17
#define REG_OSC_SEL                 0x18
#define REG_nCLK_SEL                0x19
#define REG_OPMODE_SEL              0x1C
#define REG_START_SEQUENCE          0x1E
#define REG_ABORT_SEQUENCE          0x1F
#define REG_AUTO_SEQ_CHEN           0x20
#define REG_CHANNEL_INPUT_CFG       0x24
#define REG_DOUT_FORMAT_CFG         0x28
#define REG_DATA_BUFFER_OPMODE      0x2C
#define REG_ACC_EN                  0x30

/* Values */
#define VAL_WKEY_UNLOCK             0x0A
#define VAL_WKEY_LOCK               0x00
#define VAL_DEVICE_RESET            0x01
#define VAL_TRIG_OFFCAL             0x01
#define VAL_CH_CFG_2CH_SE           0x03    /* Two-channel single-ended */
#define VAL_OPMODE_MANUAL_AUTOSEQ   0x04    /* Manual mode + auto seq   */
#define VAL_AUTOSEQ_CH0_CH1         0x03    /* Both channels enabled    */
#define VAL_START_SEQ               0x01
#define VAL_ABORT_SEQ               0x01
#define VAL_DOUT_FMT2               0x02    /* 12-bit + chID + valid    */

#define ADC_RESOLUTION              4095
#define VREF                        3.3f

/* ── I2C Init ─────────────────────────────────────────────────────── */

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

/* ── ADS7142 Helpers ──────────────────────────────────────────────── */

/**
 * @brief Write a single register: [OP_SINGLE_WRITE][reg][data]
 */
static esp_err_t ads7142_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[3] = { OP_SINGLE_WRITE, reg, data };
    return i2c_master_write_to_device(
        I2C_MASTER_NUM, ADS7142_ADDR,
        buf, 3,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Read a single register:
 *        TX: [OP_SINGLE_READ][reg]  →  RX: [data]
 */
static esp_err_t ads7142_read_reg(uint8_t reg, uint8_t *data)
{
    uint8_t cmd[2] = { OP_SINGLE_READ, reg };
    esp_err_t err = i2c_master_write_to_device(
        I2C_MASTER_NUM, ADS7142_ADDR,
        cmd, 2,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) return err;

    uint8_t buf[1] = {0};
    err = i2c_master_read_from_device(
        I2C_MASTER_NUM, ADS7142_ADDR,
        buf, 1,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
        *data = buf[0];
    }
    return err;
}

/**
 * @brief Read N bytes from the data FIFO via BLOCK_READ opcode
 */
static esp_err_t ads7142_read_fifo(uint8_t *buf, uint8_t count)
{
    uint8_t cmd[2] = { OP_BLOCK_READ, count };
    esp_err_t err = i2c_master_write_to_device(
        I2C_MASTER_NUM, ADS7142_ADDR,
        cmd, 2,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) return err;

    return i2c_master_read_from_device(
        I2C_MASTER_NUM, ADS7142_ADDR,
        buf, count * 2,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/* ── Main ─────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C master...");
    ESP_ERROR_CHECK(i2c_master_init());

    /* ── Device Reset ─────────────────────────────────────────────── */
    ESP_LOGI(TAG, "Resetting ADS7142...");
    ads7142_write_reg(REG_WKEY, VAL_WKEY_UNLOCK);
    ads7142_write_reg(REG_DEVICE_RESET, VAL_DEVICE_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* ── Offset Calibration ───────────────────────────────────────── */
    ads7142_write_reg(REG_OFFSET_CAL, VAL_TRIG_OFFCAL);
    ads7142_write_reg(REG_WKEY, VAL_WKEY_LOCK);

    /* ── Configure Manual Mode with Auto-Sequencing ───────────────── */
    ads7142_write_reg(REG_CHANNEL_INPUT_CFG, VAL_CH_CFG_2CH_SE);
    ads7142_write_reg(REG_OPMODE_SEL, VAL_OPMODE_MANUAL_AUTOSEQ);
    ads7142_write_reg(REG_AUTO_SEQ_CHEN, VAL_AUTOSEQ_CH0_CH1);
    ads7142_write_reg(REG_OSC_SEL, 0x00);          /* High-speed oscillator */
    ads7142_write_reg(REG_nCLK_SEL, 0x15);         /* nCLK = 21             */
    ads7142_write_reg(REG_DOUT_FORMAT_CFG, VAL_DOUT_FMT2);

    uint8_t opmode = 0;
    ads7142_read_reg(REG_OPMODE_SEL, &opmode);
    ESP_LOGI(TAG, "OPMODE_SEL = 0x%02X", opmode);

    uint8_t ch_cfg = 0;
    ads7142_read_reg(REG_CHANNEL_INPUT_CFG, &ch_cfg);
    ESP_LOGI(TAG, "CHANNEL_INPUT_CFG = 0x%02X", ch_cfg);

    /* ── Continuous Read Loop ─────────────────────────────────────── */
    while (1) {
        /* Start conversion sequence */
        ads7142_write_reg(REG_START_SEQUENCE, VAL_START_SEQ);
        vTaskDelay(pdMS_TO_TICKS(50));

        /* Read conversion data (2 channels × 2 bytes) */
        uint8_t data[4] = {0};
        esp_err_t err = ads7142_read_fifo(data, 2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "FIFO read failed: %s", esp_err_to_name(err));
        } else {
            /* Parse Format 2: [12-bit ADC][3-bit chID][1-bit valid] */
            uint16_t raw0 = ((uint16_t)data[0] << 8) | data[1];
            uint16_t raw1 = ((uint16_t)data[2] << 8) | data[3];

            uint16_t adc0 = (raw0 >> 4) & 0x0FFF;
            uint16_t adc1 = (raw1 >> 4) & 0x0FFF;

            float v0 = ((float)adc0 / ADC_RESOLUTION) * VREF;
            float v1 = ((float)adc1 / ADC_RESOLUTION) * VREF;

            ESP_LOGI(TAG, "CH0: %d (%.3f V)  CH1: %d (%.3f V)",
                     adc0, v0, adc1, v1);
        }

        /* Abort sequence before next round */
        ads7142_write_reg(REG_ABORT_SEQUENCE, VAL_ABORT_SEQ);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
