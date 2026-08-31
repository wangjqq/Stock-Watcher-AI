#include "display.h"

#include "esp_log.h"

static const char *TAG = "display";

/* TODO(真机): 使用 esp_lcd 或 LVGL 初始化 LCD/TFT，替换下面的模拟实现 */
esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "display init (fake, 待接入 TFT)");
    return ESP_OK;
}

void display_clear(uint32_t color)
{
    ESP_LOGD(TAG, "clear #%06X", color);
}

void display_draw_text(int x, int y, const char *text, int font_size, uint32_t color)
{
    ESP_LOGI(TAG, "text @(%d,%d) size=%d color=#%06X: %s", x, y, font_size, color, text);
}

void display_update(void)
{
    ESP_LOGD(TAG, "update");
}
