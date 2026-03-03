/*
 * BQ27441-G1 Fuel Gauge Wokwi Simulation Example (ESP-IDF)
 *
 * Demonstrates communication with the TI BQ27441-G1 fuel gauge
 * using standard I2C register reads and control subcommands.
 *
 * Wiring (diagram.json):
 *   GPIO 4  → SDA
 *   GPIO 5  → SCL
 *   GPIO 6  → GPOUT (battery low alert)
 *
 * The example:
 *   1. Verifies device type (should be 0x0421)
 *   2. Reads battery parameters in a loop:
 *      - Voltage, Average Current, SOC, Temperature, Flags
 *      - Remaining/Full capacity, Power, SOH
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "bq27441-example";

/* ── I2C Configuration ────────────────────────────────────────────────── */

#define I2C_MASTER_SCL_IO           5
#define I2C_MASTER_SDA_IO           4
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

/* ── BQ27441 Constants ────────────────────────────────────────────────── */

#define BQ27441_ADDR                0x55

/* Standard command registers */
#define BQ27441_CONTROL             0x00
#define BQ27441_TEMPERATURE         0x02
#define BQ27441_VOLTAGE             0x04
#define BQ27441_FLAGS               0x06
#define BQ27441_NOM_AVAIL_CAP       0x08
#define BQ27441_FULL_AVAIL_CAP      0x0A
#define BQ27441_REM_CAP             0x0C
#define BQ27441_FULL_CHG_CAP        0x0E
#define BQ27441_AVG_CURRENT         0x10
#define BQ27441_STBY_CURRENT        0x12
#define BQ27441_MAX_LOAD_CURRENT    0x14
#define BQ27441_AVG_POWER           0x18
#define BQ27441_SOC                 0x1C
#define BQ27441_INT_TEMP            0x1E
#define BQ27441_SOH                 0x20

/* Control subcommands */
#define BQ27441_CTL_DEVICE_TYPE     0x0001

/* Flag bits */
#define FLAG_DSG                    (1 << 0)
#define FLAG_CHG                    (1 << 8)
#define FLAG_FC                     (1 << 9)

/* ── I2C Init ─────────────────────────────────────────────────────────── */

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

/* ── BQ27441 Helpers ──────────────────────────────────────────────────── */

/**
 * @brief Read a 16-bit word from a standard command register (little-endian)
 */
static esp_err_t bq27441_read_word(uint8_t reg, uint16_t *value)
{
    uint8_t data[2] = {0};
    esp_err_t err = i2c_master_write_read_device(
        I2C_MASTER_NUM, BQ27441_ADDR,
        &reg, 1,
        data, 2,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
        *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    }
    return err;
}

/**
 * @brief Write a control subcommand (2 bytes, little-endian, to register 0x00)
 */
static esp_err_t bq27441_write_control(uint16_t subcmd)
{
    uint8_t buf[3] = {
        BQ27441_CONTROL,
        (uint8_t)(subcmd & 0xFF),
        (uint8_t)((subcmd >> 8) & 0xFF)
    };
    return i2c_master_write_to_device(
        I2C_MASTER_NUM, BQ27441_ADDR,
        buf, 3,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Execute control subcommand and read result
 */
static esp_err_t bq27441_read_control(uint16_t subcmd, uint16_t *result)
{
    esp_err_t err = bq27441_write_control(subcmd);
    if (err != ESP_OK) return err;
    return bq27441_read_word(BQ27441_CONTROL, result);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C master...");
    ESP_ERROR_CHECK(i2c_master_init());

    /* Verify device type */
    uint16_t device_id = 0;
    if (bq27441_read_control(BQ27441_CTL_DEVICE_TYPE, &device_id) == ESP_OK) {
        ESP_LOGI(TAG, "Device Type: 0x%04X %s",
                 device_id, device_id == 0x0421 ? "(BQ27441)" : "(UNKNOWN)");
    } else {
        ESP_LOGE(TAG, "Failed to read device type");
    }

    /* Continuous reading loop */
    while (1) {
        uint16_t voltage = 0, soc = 0, flags = 0;
        uint16_t rem_cap = 0, full_cap = 0, soh = 0;
        int16_t  current = 0, power = 0;
        uint16_t temp_raw = 0;

        bq27441_read_word(BQ27441_VOLTAGE,     &voltage);
        bq27441_read_word(BQ27441_AVG_CURRENT,  (uint16_t *)&current);
        bq27441_read_word(BQ27441_SOC,          &soc);
        bq27441_read_word(BQ27441_FLAGS,        &flags);
        bq27441_read_word(BQ27441_REM_CAP,      &rem_cap);
        bq27441_read_word(BQ27441_FULL_CHG_CAP, &full_cap);
        bq27441_read_word(BQ27441_AVG_POWER,    (uint16_t *)&power);
        bq27441_read_word(BQ27441_SOH,          &soh);
        bq27441_read_word(BQ27441_TEMPERATURE,  &temp_raw);

        float temp_c = ((float)temp_raw / 10.0f) - 273.15f;

        ESP_LOGI(TAG, "V=%dmV  I=%dmA  SOC=%d%%  T=%.1f°C  Cap=%d/%dmAh  P=%dmW  SOH=%d%%",
                 voltage, current, soc, temp_c, rem_cap, full_cap, power,
                 soh & 0xFF);
        ESP_LOGI(TAG, "Flags: 0x%04X [%s%s%s]",
                 flags,
                 (flags & FLAG_DSG) ? "DSG " : "",
                 (flags & FLAG_CHG) ? "CHG " : "",
                 (flags & FLAG_FC)  ? "FC "  : "");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
