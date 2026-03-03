#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "max17261-example";

/**
 * @brief I2C Master Config
 */
#define I2C_MASTER_SCL_IO           5
#define I2C_MASTER_SDA_IO           4
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

#define MAX17261_ADDR               0x36

// Key MAX17261 registers
#define REG_STATUS      0x00
#define REG_REP_CAP     0x05
#define REG_REP_SOC     0x06
#define REG_TEMP        0x08
#define REG_VCELL       0x09
#define REG_CURRENT     0x0A
#define REG_AVG_CURRENT 0x0B
#define REG_FULL_CAP    0x10
#define REG_TTE         0x11
#define REG_DESIGN_CAP  0x18
#define REG_DEV_NAME    0x21
#define REG_FSTAT       0x3D
#define REG_HIBCFG      0xBA
#define REG_MODELCFG    0xDB
#define REG_VFSOC0_LOCK 0x60

// Unit conversion (Rsense = 10 mOhm)
#define R_SENSE              0.01
#define CAPACITY_MULT        (5e-3 / R_SENSE)     // 0.5 mAh/LSB
#define CURRENT_MULT         (1.5625e-4 / R_SENSE) // 15.625 mA/LSB (= mA)
#define VOLTAGE_MULT_MV      0.078125              // mV/LSB
#define TIME_MULT_SEC        5.625                 // sec/LSB

/**
 * @brief i2c master initialization
 */
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

/**
 * @brief Read a 16-bit word from a MAX17261 register
 */
static esp_err_t max17261_read_word(uint8_t reg, uint16_t *data)
{
    uint8_t buf[2] = {0};
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, MAX17261_ADDR,
                                                  &reg, 1, buf, 2,
                                                  I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret == ESP_OK) {
        *data = buf[0] | ((uint16_t)buf[1] << 8); // LSB first
    }
    return ret;
}

/**
 * @brief Write a 16-bit word to a MAX17261 register
 */
static esp_err_t max17261_write_word(uint8_t reg, uint16_t data)
{
    uint8_t buf[3] = {reg, data & 0xFF, (data >> 8) & 0xFF};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MAX17261_ADDR,
                                       buf, 3,
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // I2C Scanner
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (int i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at: 0x%02x", i);
        }
    }
    ESP_LOGI(TAG, "I2C scan complete");

    // Verify device ID
    uint16_t dev_id = 0;
    max17261_read_word(REG_DEV_NAME, &dev_id);
    ESP_LOGI(TAG, "MAX17261 Device ID: 0x%04X (expected 0x4033)", dev_id);

    // Check POR status
    uint16_t status = 0;
    max17261_read_word(REG_STATUS, &status);
    ESP_LOGI(TAG, "Status: 0x%04X (POR=%d)", status, (status >> 1) & 1);

    if (status & 0x0002) {
        ESP_LOGI(TAG, "POR detected, performing setup...");

        // Wait for DNR bit to clear
        uint16_t fstat;
        do {
            max17261_read_word(REG_FSTAT, &fstat);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } while (fstat & 0x0001);

        // Exit hibernate mode
        uint16_t hibcfg;
        max17261_read_word(REG_HIBCFG, &hibcfg);
        max17261_write_word(REG_VFSOC0_LOCK, 0x90);
        max17261_write_word(REG_HIBCFG, 0x0000);
        max17261_write_word(REG_VFSOC0_LOCK, 0x00);

        // Configure battery: 5000 mAh design capacity
        uint16_t design_cap = (uint16_t)(5000.0 / CAPACITY_MULT);
        max17261_write_word(REG_DESIGN_CAP, design_cap);

        // Write ModelCFG
        max17261_write_word(REG_MODELCFG, 0x8000);

        // Wait for refresh to complete
        uint16_t modelcfg;
        do {
            max17261_read_word(REG_MODELCFG, &modelcfg);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } while (modelcfg & 0x8000);

        // Restore hibernate config
        max17261_write_word(REG_HIBCFG, hibcfg);

        // Clear POR bit
        max17261_read_word(REG_STATUS, &status);
        max17261_write_word(REG_STATUS, status & 0xFFFD);

        ESP_LOGI(TAG, "Setup complete");
    }

    ESP_LOGI(TAG, "MAX17261 Example Started - reading battery data every 2 seconds");

    while (1) {
        uint16_t raw;

        // SOC
        max17261_read_word(REG_REP_SOC, &raw);
        uint8_t soc = raw >> 8;

        // Voltage
        max17261_read_word(REG_VCELL, &raw);
        float voltage_mv = (float)raw * VOLTAGE_MULT_MV;

        // Current
        max17261_read_word(REG_CURRENT, &raw);
        float current_mA = (float)((int16_t)raw) * CURRENT_MULT * 1000.0f;

        // Temperature
        max17261_read_word(REG_TEMP, &raw);
        int16_t temp_raw = (int16_t)raw;
        float temp_c = (float)temp_raw / 256.0f;

        // Capacity
        max17261_read_word(REG_REP_CAP, &raw);
        float cap_mAh = (float)raw * CAPACITY_MULT;

        // TTE
        max17261_read_word(REG_TTE, &raw);
        float tte_min = (float)raw * TIME_MULT_SEC / 60.0f;

        ESP_LOGI(TAG, "SOC: %d%% | Voltage: %.0f mV | Current: %.1f mA | Temp: %.1f C | Cap: %.0f mAh | TTE: %.1f min",
                 soc, voltage_mv, current_mA, temp_c, cap_mAh, tte_min);

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
