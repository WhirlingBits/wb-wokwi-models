#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "r2221-example";

/**
 * @brief I2C Master Config
 */
#define I2C_MASTER_SCL_IO           5       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           4       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0       /*!< I2C master i2c port number */
#define I2C_MASTER_FREQ_HZ          100000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

#define R2221_ADDR                  0x32    /*!< I2C address of R2221 RTC */

// R2221 Registers
#define REG_SECOND          0x00
#define REG_MINUTE          0x01
#define REG_HOUR            0x02
#define REG_DAY_WEEK        0x03
#define REG_DAY_MONTH       0x04
#define REG_MONTH           0x05
#define REG_YEAR            0x06
#define REG_CTRL1           0x0E
#define REG_CTRL2           0x0F

/**
 * @brief Helper: decimal to BCD
 */
static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
}

/**
 * @brief Helper: BCD to decimal
 */
static uint8_t bcd2dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

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
 * @brief Write a byte to an R2221 register
 * Note: R2221 uses (reg << 4) as the register address byte on the wire
 */
static esp_err_t r2221_register_write(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {(reg_addr << 4), data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, R2221_ADDR,
                                       write_buf, sizeof(write_buf),
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Read a byte from an R2221 register
 */
static esp_err_t r2221_register_read(uint8_t reg_addr, uint8_t *data)
{
    uint8_t addr_byte = (reg_addr << 4);
    return i2c_master_write_read_device(I2C_MASTER_NUM, R2221_ADDR,
                                         &addr_byte, 1, data, 1,
                                         I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Read multiple consecutive registers from R2221
 */
static esp_err_t r2221_register_read_multi(uint8_t reg_addr, uint8_t *data, size_t len)
{
    uint8_t addr_byte = (reg_addr << 4);
    return i2c_master_write_read_device(I2C_MASTER_NUM, R2221_ADDR,
                                         &addr_byte, 1, data, len,
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

    // Clear power-on flags: write 0x00 to Control 2
    ESP_LOGI(TAG, "Clearing R2221 power-on flags...");
    ESP_ERROR_CHECK(r2221_register_write(REG_CTRL2, 0x00));

    // Set 24-hour mode (bit 5 of Control 1)
    ESP_LOGI(TAG, "Setting 24-hour mode...");
    uint8_t ctrl1;
    r2221_register_read(REG_CTRL1, &ctrl1);
    ctrl1 |= (1 << 5);
    ESP_ERROR_CHECK(r2221_register_write(REG_CTRL1, ctrl1));

    // Set time: 12:30:00, Monday, March 3rd, 2026
    ESP_LOGI(TAG, "Setting R2221 time...");
    ESP_ERROR_CHECK(r2221_register_write(REG_SECOND,    dec2bcd(0)));
    ESP_ERROR_CHECK(r2221_register_write(REG_MINUTE,    dec2bcd(30)));
    ESP_ERROR_CHECK(r2221_register_write(REG_HOUR,      dec2bcd(12)));
    ESP_ERROR_CHECK(r2221_register_write(REG_DAY_WEEK,  dec2bcd(1)));  // Monday
    ESP_ERROR_CHECK(r2221_register_write(REG_DAY_MONTH, dec2bcd(3)));
    ESP_ERROR_CHECK(r2221_register_write(REG_MONTH,     dec2bcd(3)));
    ESP_ERROR_CHECK(r2221_register_write(REG_YEAR,      dec2bcd(26)));

    ESP_LOGI(TAG, "R2221 Example Started - reading time every second");

    while (1) {
        // Read all 7 time registers at once
        uint8_t time_data[7] = {0};
        if (r2221_register_read_multi(REG_SECOND, time_data, 7) == ESP_OK) {
            uint8_t sec   = bcd2dec(time_data[0] & 0x7F);
            uint8_t min   = bcd2dec(time_data[1] & 0x7F);
            uint8_t hour  = bcd2dec(time_data[2] & 0x3F);
            uint8_t wday  = bcd2dec(time_data[3] & 0x07);
            uint8_t day   = bcd2dec(time_data[4] & 0x3F);
            uint8_t month = bcd2dec(time_data[5] & 0x1F);
            uint8_t year  = bcd2dec(time_data[6]);

            ESP_LOGI(TAG, "Time: 20%02d-%02d-%02d (W%d) %02d:%02d:%02d",
                     year, month, day, wday, hour, min, sec);
        } else {
            ESP_LOGE(TAG, "Failed to read time from R2221");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
