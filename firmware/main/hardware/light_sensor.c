#include "light_sensor.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "light";

/* ------------------------------------------------------------------
 * 光敏传感器 BH1750（I2C，环境光照度 0~65535 lux）
 *
 * 接线（与 PINMAP.md 保持一致）：
 *   VCC → 3V3   GND → GND   SDA → GPIO8   SCL → GPIO10
 *   （模块一般自带 I2C 上拉电阻；ADDR 悬空则地址为 0x23）
 *
 * 说明：
 *   - I2C 总线由本模块初始化（I2C_NUM_0），后续其他 I2C 传感器复用同一总线。
 *   - 目标芯片 ESP32-S3 N16R8（Octal PSRAM）占用 GPIO26~37，
 *     SDA/SCL 避开该区间使用 GPIO8/10（由 GPIO matrix 任意映射）。
 * ------------------------------------------------------------------ */
#define I2C_NUM        I2C_NUM_0
#define PIN_SDA        GPIO_NUM_8
#define PIN_SCL        GPIO_NUM_10
#define I2C_FREQ_HZ    100000

#define BH1750_ADDR    0x23   /* ADDR 低电平（悬空/接地）默认地址 */
#define BH1750_POWER_ON  0x01
#define BH1750_CONT_H    0x10 /* 持续高分辨率模式（1lx） */

#define I2C_TIMEOUT_MS  100

void light_sensor_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    i2c_param_config(I2C_NUM, &conf);
    if (i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK) {
        ESP_LOGE(TAG, "i2c driver install failed");
        return;
    }

    /* 上电 + 持续高分辨率模式；传感器未接时写失败，忽略即可 */
    uint8_t cmd = BH1750_POWER_ON;
    i2c_master_write_to_device(I2C_NUM, BH1750_ADDR, &cmd, 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    cmd = BH1750_CONT_H;
    i2c_master_write_to_device(I2C_NUM, BH1750_ADDR, &cmd, 1, pdMS_TO_TICKS(I2C_TIMEOUT_MS));

    ESP_LOGI(TAG, "init ok (SDA=%d SCL=%d)", PIN_SDA, PIN_SCL);
}

uint16_t light_sensor_read_lux(void)
{
    uint8_t buf[2] = { 0, 0 };
    esp_err_t err = i2c_master_read_from_device(I2C_NUM, BH1750_ADDR, buf, 2,
                                                pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        return 0; /* 读取失败 / 未接传感器 */
    }
    /* 高分辨率模式：lux = 寄存器值 / 1.2 */
    uint16_t raw = (uint16_t)((buf[0] << 8) | buf[1]);
    return (uint16_t)(raw / 1.2f);
}
