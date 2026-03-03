#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ads1115-example";

/**
 * @brief I2C Master Config
 */
#define I2C_MASTER_SCL_IO           5       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           4       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0       /*!< I2C master i2c port number */
#define I2C_MASTER_FREQ_HZ          100000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define ADS1115_ADDR                0x48    /*!< Slave address of the ADS1115 sensor */

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief Read a sequence of bytes from a MPU9250 sensor registers
 */
static esp_err_t ads1115_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, ADS1115_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Write a 16-bit word to a ADS1115 sensor register
 */
static esp_err_t ads1115_register_write(uint8_t reg_addr, uint16_t data)
{
    int ret;
    uint8_t write_buf[3] = {reg_addr, (uint8_t)(data >> 8), (uint8_t)(data & 0xFF)};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, ADS1115_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
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
    
    // Config Register (0x01) Setup
    // OS=1, MUX=100 (AIN0-GND), PGA=001 (4.096V), MODE=0 (Continuous)
    // DR=100 (128SPS), COMP_QUE=11 (Disable)
    // 0xC283
    uint16_t config = 0xC283;
    ESP_LOGI(TAG, "Configuring ADS1115 with 0x%04X", config);
    ESP_ERROR_CHECK(ads1115_register_write(0x01, config));

    ESP_LOGI(TAG, "ADS1115 ESP-IDF Example Started");
    ESP_LOGI(TAG, "Reading continuous values from AIN0...");

    while (1) {
        // Point to Conversion Register (0x00)
        // In I2C Read, we often write the register address first, then read.
        // But for continuous reading from same register, pointer stays.
        // However, let's allow setting pointer explicitly each loop for robustness.
        
        uint8_t data[2];
        // ADS1115 read protocol: Write Pointer Register -> Read Data
        esp_err_t ret = ads1115_register_read(0x00, data, 2);

        if (ret == ESP_OK) {
            int16_t reading = (int16_t)((data[0] << 8) | data[1]);
            // Default PGA = 4.096V range (Gain = 1)
            float voltage = ((float)reading / 32768.0) * 4.096;
            
            // Print similar to Arduino example
            // ESP_LOG works but printf is closer to Serial.print raw output format if parsing
            // But user asked for logs, so ESP_LOG is better for IDF.
            ESP_LOGI(TAG, "Raw: %d\tVoltage: %.3f V", reading, voltage);
        } else {
            ESP_LOGE(TAG, "Failed to read from ADS1115");
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
