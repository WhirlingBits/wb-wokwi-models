/*
 * EZO-RTD Wokwi Simulation Example (ESP-IDF)
 *
 * Demonstrates communication with the Atlas Scientific EZO-RTD
 * temperature circuit using the EZO I2C ASCII protocol.
 *
 * Wiring (diagram.json):
 *   GPIO 4  → SDA
 *   GPIO 5  → SCL
 *
 * The example:
 *   1. Queries device info ("I")
 *   2. Queries calibration status ("Cal,?")
 *   3. Reads temperature in a loop ("R")
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ezo-rtd-example";

/* ── I2C Configuration ────────────────────────────────────────────────── */

#define I2C_MASTER_SCL_IO           5
#define I2C_MASTER_SDA_IO           4
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

/* ── EZO-RTD Constants ────────────────────────────────────────────────── */

#define EZO_RTD_ADDR                0x66
#define EZO_DELAY_RTD_MS            600
#define EZO_DELAY_GENERIC_MS        300

#define EZO_STATUS_SUCCESS          1
#define EZO_STATUS_FAILED           2
#define EZO_STATUS_PENDING          254
#define EZO_STATUS_NO_DATA          255

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

/* ── EZO Helpers ──────────────────────────────────────────────────────── */

static esp_err_t ezo_send_command(const char *cmd)
{
    return i2c_master_write_to_device(
        I2C_MASTER_NUM, EZO_RTD_ADDR,
        (const uint8_t *)cmd, strlen(cmd),
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t ezo_read_response(char *buffer, size_t len)
{
    uint8_t *raw = calloc(1, len + 1);
    if (!raw) return ESP_ERR_NO_MEM;

    esp_err_t err = i2c_master_read_from_device(
        I2C_MASTER_NUM, EZO_RTD_ADDR,
        raw, len,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (err != ESP_OK) {
        free(raw);
        return err;
    }

    uint8_t status = raw[0];
    switch (status) {
    case EZO_STATUS_SUCCESS:
        /* Copy data after status byte */
        memcpy(buffer, &raw[1], len - 1);
        buffer[len - 1] = '\0';
        /* Trim trailing nulls */
        for (int i = (int)strlen(buffer) - 1; i >= 0 && buffer[i] == '\0'; i--)
            buffer[i] = '\0';
        err = ESP_OK;
        break;
    case EZO_STATUS_PENDING:
        ESP_LOGW(TAG, "Device still processing");
        err = ESP_ERR_TIMEOUT;
        break;
    case EZO_STATUS_FAILED:
        ESP_LOGE(TAG, "Command failed");
        err = ESP_FAIL;
        break;
    case EZO_STATUS_NO_DATA:
        ESP_LOGW(TAG, "No data available");
        err = ESP_ERR_NOT_FOUND;
        break;
    default:
        ESP_LOGE(TAG, "Unknown status: %d", status);
        err = ESP_FAIL;
        break;
    }

    free(raw);
    return err;
}

/**
 * @brief Send command, wait processing delay, read response
 */
static esp_err_t ezo_command(const char *cmd, uint32_t delay_ms,
                             char *buffer, size_t len)
{
    esp_err_t err = ezo_send_command(cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send '%s': %s", cmd, esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    err = ezo_read_response(buffer, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read response for '%s': %s", cmd, esp_err_to_name(err));
    }
    return err;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C master...");
    ESP_ERROR_CHECK(i2c_master_init());

    char resp[32] = {0};

    /* Query device info */
    if (ezo_command("I", EZO_DELAY_GENERIC_MS, resp, sizeof(resp)) == ESP_OK) {
        ESP_LOGI(TAG, "Device Info: %s", resp);
    }

    /* Query calibration status */
    if (ezo_command("Cal,?", EZO_DELAY_GENERIC_MS, resp, sizeof(resp)) == ESP_OK) {
        ESP_LOGI(TAG, "Calibration: %s", resp);
    }

    /* Continuous temperature reading loop */
    while (1) {
        memset(resp, 0, sizeof(resp));

        if (ezo_command("R", EZO_DELAY_RTD_MS, resp, sizeof(resp)) == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %s", resp);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
