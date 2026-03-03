#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "tca9554-example";

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

#define TCA9554_ADDR                0x20    /*!< Slace address of the TCA9554 sensor (A0=A1=A2=GND) */

// TCA9554 Registers
#define REG_INPUT_PORT    0x00
#define REG_OUTPUT_PORT   0x01
#define REG_POLARITY_INV  0x02
#define REG_CONFIG        0x03

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
 * @brief Write a byte to a TCA9554 register
 */
static esp_err_t tca9554_register_write(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(I2C_MASTER_NUM, TCA9554_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return ret;
}

/**
 * @brief Read a byte from a TCA9554 register
 */
static esp_err_t tca9554_register_read(uint8_t reg_addr, uint8_t *data)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, TCA9554_ADDR, &reg_addr, 1, data, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // I2C Scanner to verify connectivity
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

    // Configure TCA9554
    // Set P0-P3 as Output (0), P4-P7 as Input (1) -> 0xF0
    ESP_LOGI(TAG, "Configuring TCA9554 (P0-P3 Output, P4-P7 Input)...");
    ESP_ERROR_CHECK(tca9554_register_write(REG_CONFIG, 0xF0));
    
    // Initial Output: All Off (Low)
    uint8_t output_state = 0x00;
    ESP_ERROR_CHECK(tca9554_register_write(REG_OUTPUT_PORT, output_state));

    ESP_LOGI(TAG, "TCA9554 Example Started");

    while (1) {
        // Toggle P0 (LED)
        output_state ^= 0x01; // Toggle Bit 0
        ESP_LOGI(TAG, "Setting Output Port: 0x%02x", output_state);
        tca9554_register_write(REG_OUTPUT_PORT, output_state);

        // Read Inputs
        uint8_t input_val;
        if (tca9554_register_read(REG_INPUT_PORT, &input_val) == ESP_OK) {
            ESP_LOGI(TAG, "Input Port Read: 0x%02x", input_val);
        } else {
            ESP_LOGE(TAG, "Failed to read Input Port");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
