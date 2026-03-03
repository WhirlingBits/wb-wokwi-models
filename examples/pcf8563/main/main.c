#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "pcf8563-example";

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

#define PCF8563_ADDR                0x51    /*!< I2C address of PCF8563 RTC */

// PCF8563 Registers
#define REG_CTRL_STATUS_1   0x00
#define REG_CTRL_STATUS_2   0x01
#define REG_SECONDS         0x02
#define REG_MINUTES         0x03
#define REG_HOURS           0x04
#define REG_DAYS            0x05
#define REG_WEEKDAYS        0x06
#define REG_MONTHS          0x07
#define REG_YEARS           0x08

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
 * @brief Write a byte to a PCF8563 register
 */
static esp_err_t pcf8563_register_write(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, PCF8563_ADDR,
                                       write_buf, sizeof(write_buf),
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Read a byte from a PCF8563 register
 */
static esp_err_t pcf8563_register_read(uint8_t reg_addr, uint8_t *data)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, PCF8563_ADDR,
                                         &reg_addr, 1, data, 1,
                                         I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Read multiple consecutive registers from PCF8563
 */
static esp_err_t pcf8563_register_read_multi(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, PCF8563_ADDR,
                                         &reg_addr, 1, data, len,
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

    // Check VL bit (bit 7 of seconds register) - indicates clock integrity
    uint8_t sec_reg;
    pcf8563_register_read(REG_SECONDS, &sec_reg);
    if (sec_reg & 0x80) {
        ESP_LOGW(TAG, "VL bit set - clock integrity not guaranteed, setting time...");
    }

    // Clear control registers
    ESP_ERROR_CHECK(pcf8563_register_write(REG_CTRL_STATUS_1, 0x00));
    ESP_ERROR_CHECK(pcf8563_register_write(REG_CTRL_STATUS_2, 0x00));

    // Set time: 12:30:00, Tuesday, March 3rd, 2026
    ESP_LOGI(TAG, "Setting PCF8563 time...");
    ESP_ERROR_CHECK(pcf8563_register_write(REG_SECONDS,  dec2bcd(0)));
    ESP_ERROR_CHECK(pcf8563_register_write(REG_MINUTES,  dec2bcd(30)));
    ESP_ERROR_CHECK(pcf8563_register_write(REG_HOURS,    dec2bcd(12)));
    ESP_ERROR_CHECK(pcf8563_register_write(REG_DAYS,     dec2bcd(3)));
    ESP_ERROR_CHECK(pcf8563_register_write(REG_WEEKDAYS, 2)); // Tuesday
    ESP_ERROR_CHECK(pcf8563_register_write(REG_MONTHS,   dec2bcd(3)));
    ESP_ERROR_CHECK(pcf8563_register_write(REG_YEARS,    dec2bcd(26)));

    ESP_LOGI(TAG, "PCF8563 Example Started - reading time every second");

    while (1) {
        // Read all 7 time registers at once (0x02 - 0x08)
        uint8_t time_data[7] = {0};
        if (pcf8563_register_read_multi(REG_SECONDS, time_data, 7) == ESP_OK) {
            uint8_t sec   = bcd2dec(time_data[0] & 0x7F); // Mask VL bit
            uint8_t min   = bcd2dec(time_data[1] & 0x7F);
            uint8_t hour  = bcd2dec(time_data[2] & 0x3F);
            uint8_t day   = bcd2dec(time_data[3] & 0x3F);
            uint8_t wday  = time_data[4] & 0x07;
            uint8_t month = bcd2dec(time_data[5] & 0x1F);
            uint8_t year  = bcd2dec(time_data[6]);

            ESP_LOGI(TAG, "Time: 20%02d-%02d-%02d (W%d) %02d:%02d:%02d",
                     year, month, day, wday, hour, min, sec);
        } else {
            ESP_LOGE(TAG, "Failed to read time from PCF8563");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
