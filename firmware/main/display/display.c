#include "display.h"
#include "font_zh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ili9341";

/* ------------------------------------------------------------------
 * ILI9341 2.4" 240x320 模块接线（按实际接线改这里）
 *
 *  模块引脚:  VCC GND CS  RESET DC/RS SDI(MOSI) SCK LED SDO(MISO)
 *             T_CLK T_CS T_DIN T_DO T_IRQ
 *  T_CLK / T_DIN 与 SCK / SDI 共用同一 SPI 总线（连到同一个 GPIO），
 *  触摸屏 T_DO 接 MISO（LCD 的 SDO 可不接，写屏无需读回）。
 * ------------------------------------------------------------------ */
#define PIN_SCLK   GPIO_NUM_18
#define PIN_MOSI   GPIO_NUM_23
#define PIN_MISO   GPIO_NUM_11   /* 触摸屏 T_DO（触摸 SPI 读回，见 touch.c） */
#define PIN_CS     GPIO_NUM_5
#define PIN_DC     GPIO_NUM_17   /* A0 / 数据命令选择 */
#define PIN_RST    GPIO_NUM_16
#define PIN_BL     GPIO_NUM_4    /* 背光（高电平点亮） */

#define SPI_HOST_ID  SPI2_HOST
#define SPI_FREQ_HZ  (26 * 1000 * 1000)

/* 整屏刷新的 SPI 传输分块大小（DMA 单次传输上限内）；fb 在 PSRAM 时
 * 需先拷到内部 RAM 的 DMA 缓冲再发出。 */
#define FB_CHUNK     2048

#define RGB565(r, g, b) ((((uint16_t)(r) & 0xF8) << 8) | (((uint16_t)(g) & 0xFC) << 3) | ((uint16_t)(b) >> 3))

static spi_device_handle_t s_spi;
static uint16_t *s_fb = NULL; /* 帧缓冲（240x320 = 150KB）：PSRAM 分配，不足时退回内部 RAM */
static bool      s_fb_psram = false;
static uint8_t   s_dma_buf[FB_CHUNK] __attribute__((aligned(4))); /* SPI DMA 用内部 RAM 缓冲 */

/* ---------------- SPI 底层 ---------------- */

static void spi_send_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_DC, 0);
    spi_transaction_t t = { 0 };
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = cmd;
    spi_device_transmit(s_spi, &t);
}

static void spi_send_data(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return;
    }
    gpio_set_level(PIN_DC, 1);
    if (len <= 4) {
        spi_transaction_t t = { 0 };
        t.flags = SPI_TRANS_USE_TXDATA;
        t.length = len * 8;
        memcpy(t.tx_data, data, len);
        spi_device_transmit(s_spi, &t);
        return;
    }
    /* 命令参数数组在 flash，DMA 需要内部 RAM，复制到临时缓冲 */
    uint8_t *buf = heap_caps_malloc(len, MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "no dma mem for %d bytes", (int)len);
        return;
    }
    memcpy(buf, data, len);
    spi_transaction_t t = { 0 };
    t.length = len * 8;
    t.tx_buffer = buf;
    spi_device_transmit(s_spi, &t);
    free(buf);
}

static void write_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    spi_send_cmd(cmd);
    spi_send_data(data, len);
}

/* ---------------- 屏幕初始化 ---------------- */

static void lcd_reset(void)
{
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

/* ILI9341 初始化（240x320 常用序列）。若颜色/方向不对，参考下方注释微调 */
static void ili9341_init(void)
{
    spi_send_cmd(0x01);                        /* SWRESET */
    vTaskDelay(pdMS_TO_TICKS(120));

    spi_send_cmd(0x11);                        /* SLPOUT */
    vTaskDelay(pdMS_TO_TICKS(120));

    static const uint8_t d1[] = { 0x23 };
    write_cmd_data(0xC0, d1, 1);               /* PWCTRL1 */
    static const uint8_t d2[] = { 0x10 };
    write_cmd_data(0xC1, d2, 1);               /* PWCTRL2 */
    static const uint8_t d3[] = { 0x3E, 0x28 };
    write_cmd_data(0xC5, d3, 2);               /* VMCTRL1 */
    static const uint8_t d4[] = { 0x86 };
    write_cmd_data(0xC7, d4, 1);               /* VMCTRL2 */
    static const uint8_t d5[] = { 0x48 };      /* MADCTL 竖屏 240x320 */
    write_cmd_data(0x36, d5, 1);
    static const uint8_t d6[] = { 0x55 };      /* COLMOD 16bit/像素 */
    write_cmd_data(0x3A, d6, 1);
    static const uint8_t d7[] = { 0x00, 0x1B };
    write_cmd_data(0xB1, d7, 2);               /* FRMCTR1 */
    static const uint8_t d8[] = { 0x00, 0x12, 0x07 };
    write_cmd_data(0xB6, d8, 3);               /* DFUNCTR */
    static const uint8_t d9[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                                  0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
    write_cmd_data(0xE0, d9, 15);              /* GMCTRP1 正极性 Gamma */
    static const uint8_t d10[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                                   0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
    write_cmd_data(0xE1, d10, 15);             /* GMCTRN1 负极性 Gamma */

    spi_send_cmd(0x20);                        /* INVOFF（画面反色则改 0x21 INVON） */
    spi_send_cmd(0x29);                        /* DISPON */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 不同批次面板颜色/方向可能不同，可尝试：
     * - 颜色偏红蓝：MADCTL 加 0x08（BGR）
     * - 画面反色：INVOFF(0x20) 改 INVON(0x21)
     * - 方向不对：MADCTL 改 0xC8 / 0x88 / 0x08 / 0x28 等 */
}

/* ---------------- 背光（LEDC PWM，亮度 0-100） ---------------- */
/* buzzer 用 TIMER_0/CH_0（低），led 用 TIMER_1/CH_1-3（低），背光独立用高模式 CH_4 */
#define BL_LEDC_MODE    LEDC_HIGH_SPEED_MODE
#define BL_LEDC_TIMER   LEDC_TIMER_2
#define BL_LEDC_CHANNEL LEDC_CHANNEL_4

static void backlight_set(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, (uint32_t)pct * 255 / 100);
    ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}

static void backlight_init(uint8_t pct)
{
    ledc_timer_config_t timer = {
        .speed_mode = BL_LEDC_MODE,
        .timer_num = BL_LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT, /* 0-255 对应 0-100% */
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = PIN_BL,
        .speed_mode = BL_LEDC_MODE,
        .channel = BL_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch);

    backlight_set(pct);
}

void display_set_brightness(uint8_t pct)
{
    backlight_set(pct);
}

esp_err_t display_init(uint8_t brightness)
{
    gpio_config_t io = {
        .pin_bit_mask = BIT(PIN_DC) | BIT(PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    backlight_init(brightness); /* 背光走 LEDC PWM */

    /* 帧缓冲：优先 PSRAM（150KB），PSRAM 不可用时退回内部 RAM */
    s_fb = heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_fb_psram = (s_fb != NULL);
    if (!s_fb) {
        ESP_LOGW(TAG, "no psram fb, fallback to internal RAM");
        s_fb = heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
                                MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    }
    if (!s_fb) {
        ESP_LOGE(TAG, "no memory for framebuffer");
        return ESP_ERR_NO_MEM;
    }

    /* LCD 与触摸屏共用一条 SPI 总线：MISO 接触摸屏 T_DO */
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = FB_CHUNK,
    };
    esp_err_t err = spi_bus_initialize(SPI_HOST_ID, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t dev = {
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };
    err = spi_bus_add_device(SPI_HOST_ID, &dev, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi add device failed: %s", esp_err_to_name(err));
        return err;
    }

    lcd_reset();
    ili9341_init();
    display_clear(0x000000);
    display_update();
    ESP_LOGI(TAG, "ili9341 ready %dx%d, canvas=%dx%d, status bar=%dpx, fb=%s",
             DISPLAY_WIDTH, DISPLAY_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, STATUS_BAR_HEIGHT,
             s_fb_psram ? "psram" : "internal-ram");
    return ESP_OK;
}

/* 在共享 SPI 总线上注册触摸屏设备（供 touch.c 使用） */
esp_err_t display_spi_add_device(const spi_device_interface_config_t *dev, spi_device_handle_t *out)
{
    return spi_bus_add_device(SPI_HOST_ID, dev, out);
}

/* ---------------- 帧缓冲绘制 ---------------- */

static void fb_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > DISPLAY_WIDTH ? DISPLAY_WIDTH : x + w;
    int y1 = y + h > DISPLAY_HEIGHT ? DISPLAY_HEIGHT : y + h;
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            s_fb[yy * DISPLAY_WIDTH + xx] = color;
        }
    }
}

void display_clear(uint32_t color)
{
    uint16_t c = RGB565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        s_fb[i] = c;
    }
}

void display_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    fb_fill_rect(x, y, w, h, RGB565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
}

/* ---------------- 8x8 点阵字体（ASCII 0x20-0x7E，经典 font8x8） ---------------- */

static const uint8_t FONT8X8[95][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ' ' */
    { 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00 }, /* '!' */
    { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '"' */
    { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00 }, /* '#' */
    { 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00 }, /* '$' */
    { 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00 }, /* '%' */
    { 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00 }, /* '&' */
    { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ''' */
    { 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00 }, /* '(' */
    { 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00 }, /* ')' */
    { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 }, /* '*' */
    { 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00 }, /* '+' */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06 }, /* ',' */
    { 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00 }, /* '-' */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00 }, /* '.' */
    { 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00 }, /* '/' */
    { 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00 }, /* '0' */
    { 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00 }, /* '1' */
    { 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00 }, /* '2' */
    { 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00 }, /* '3' */
    { 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00 }, /* '4' */
    { 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00 }, /* '5' */
    { 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00 }, /* '6' */
    { 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00 }, /* '7' */
    { 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00 }, /* '8' */
    { 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00 }, /* '9' */
    { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00 }, /* ':' */
    { 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06 }, /* ';' */
    { 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00 }, /* '<' */
    { 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00 }, /* '=' */
    { 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00 }, /* '>' */
    { 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00 }, /* '?' */
    { 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00 }, /* '@' */
    { 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 }, /* 'A' */
    { 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00 }, /* 'B' */
    { 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00 }, /* 'C' */
    { 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00 }, /* 'D' */
    { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00 }, /* 'E' */
    { 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00 }, /* 'F' */
    { 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00 }, /* 'G' */
    { 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00 }, /* 'H' */
    { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }, /* 'I' */
    { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00 }, /* 'J' */
    { 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00 }, /* 'K' */
    { 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00 }, /* 'L' */
    { 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00 }, /* 'M' */
    { 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00 }, /* 'N' */
    { 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00 }, /* 'O' */
    { 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00 }, /* 'P' */
    { 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00 }, /* 'Q' */
    { 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00 }, /* 'R' */
    { 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00 }, /* 'S' */
    { 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }, /* 'T' */
    { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00 }, /* 'U' */
    { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 }, /* 'V' */
    { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 }, /* 'W' */
    { 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00 }, /* 'X' */
    { 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00 }, /* 'Y' */
    { 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00 }, /* 'Z' */
    { 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00 }, /* '[' */
    { 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00 }, /* '\' */
    { 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00 }, /* ']' */
    { 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00 }, /* '^' */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF }, /* '_' */
    { 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '`' */
    { 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00 }, /* 'a' */
    { 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00 }, /* 'b' */
    { 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00 }, /* 'c' */
    { 0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00 }, /* 'd' */
    { 0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00 }, /* 'e' */
    { 0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00 }, /* 'f' */
    { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F }, /* 'g' */
    { 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00 }, /* 'h' */
    { 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }, /* 'i' */
    { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E }, /* 'j' */
    { 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00 }, /* 'k' */
    { 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 }, /* 'l' */
    { 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00 }, /* 'm' */
    { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x66, 0x67, 0x00 }, /* 'n' */
    { 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00 }, /* 'o' */
    { 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F }, /* 'p' */
    { 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78 }, /* 'q' */
    { 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00 }, /* 'r' */
    { 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00 }, /* 's' */
    { 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00 }, /* 't' */
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00 }, /* 'u' */
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 }, /* 'v' */
    { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00 }, /* 'w' */
    { 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00 }, /* 'x' */
    { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F }, /* 'y' */
    { 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00 }, /* 'z' */
    { 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00 }, /* '{' */
    { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 }, /* '|' */
    { 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00 }, /* '}' */
    { 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* '~' */
};

/* 按比例放大绘制一个字符（idx = 字符 - 0x20） */
static void draw_char_scaled(int x, int y, int idx, float scale, uint16_t color)
{
    if (idx < 0 || idx > 95) {
        idx = 95; /* '?' 兜底 */
    }
    const uint8_t *g = FONT8X8[idx];
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = g[gy];
        for (int gx = 0; gx < 8; gx++) {
            if (!(row & (0x80 >> gx))) {
                continue;
            }
            int dx = (int)(gx * scale);
            int dy = (int)(gy * scale);
            int w = (int)((gx + 1) * scale) - dx;
            int h = (int)((gy + 1) * scale) - dy;
            fb_fill_rect(x + dx, y + dy, w, h, color);
        }
    }
}

void display_draw_text(int x, int y, const char *text, int font_size, uint32_t color)
{
    if (!text || *text == '\0') {
        return;
    }
    if (font_size < 8) {
        font_size = 8;
    }
    float scale = (float)font_size / 8.0f;
    int adv = (int)(8 * scale); /* 字符步进（等宽） */
    uint16_t c = RGB565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    int cx = x;
    while (*text) {
        unsigned char ch = (unsigned char)*text++;
        if (ch == '\n') {
            cx = x;
            continue;
        }
        if (ch < 0x20 || ch > 0x7E) {
            /* 非 ASCII（中文 UTF-8 多字节）暂用方块占位 */
            fb_fill_rect(cx, y, (int)(6 * scale), (int)(8 * scale), c);
        } else {
            draw_char_scaled(cx, y, ch - 0x20, scale, c);
        }
        cx += adv;
    }
}

/* ---------------- 中英混排文本（16px 高） ---------------- */

/* 绘制单个 16x16 汉字字模（g 为 32 字节，每行 2 字节高位在前） */
static void draw_zh_glyph(int x, int y, const uint8_t *g, int scale, uint16_t color)
{
    for (int gy = 0; gy < 16; gy++) {
        uint8_t lo = g[gy * 2];     /* 左 8 像素 */
        uint8_t hi = g[gy * 2 + 1]; /* 右 8 像素 */
        for (int gx = 0; gx < 8; gx++) {
            if (lo & (0x80 >> gx)) {
                fb_fill_rect(x + gx * scale, y + gy * scale, scale, scale, color);
            }
            if (hi & (0x80 >> gx)) {
                fb_fill_rect(x + (8 + gx) * scale, y + gy * scale, scale, scale, color);
            }
        }
    }
}

int display_draw_text_zh(int x, int y, const char *text, int scale, uint32_t color)
{
    if (!text || *text == '\0') {
        return 0;
    }
    if (scale != 2) {
        scale = 1;
    }
    uint16_t c = RGB565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    int cx = x;
    const uint8_t *p = (const uint8_t *)text;
    while (*p) {
        if (*p < 0x80) { /* ASCII */
            unsigned char ch = *p++;
            if (ch == '\n') {
                cx = x;
                continue;
            }
            if (ch >= 0x20 && ch <= 0x7E) {
                draw_char_scaled(cx, y, ch - 0x20, 2.0f * scale, c); /* 8x16 */
            }
            cx += 16 * scale;
        } else if (p[0] >= 0xE0 && p[0] <= 0xEF && p[1] && p[2]) { /* 中文 3 字节 */
            uint16_t gb = font_zh_utf8_to_gb(p);
            const uint8_t *g = gb ? font_zh_get_glyph(gb) : NULL;
            if (g) {
                draw_zh_glyph(cx, y, g, scale, c);
            } else {
                fb_fill_rect(cx, y, 16 * scale, 16 * scale, c); /* 缺字占位 */
            }
            p += 3;
            cx += 16 * scale;
        } else {
            p++;
        }
    }
    return cx - x;
}

/* ---------------- 状态栏（顶部 16px） ---------------- */

static int rssi_bars(int rssi)
{
    if (rssi >= -55) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    if (rssi >= -85) return 1;
    return 0;
}

void display_draw_status_bar(const char *time_str, int rssi_dbm, int battery_pct)
{
    static const uint32_t FG = 0xFFFFFF;
    static const uint32_t BAR_BG = 0x111111;

    fb_fill_rect(0, 0, DISPLAY_WIDTH, STATUS_BAR_HEIGHT, RGB565(0x11, 0x11, 0x11));

    /* 左侧：时间 */
    if (time_str) {
        display_draw_text(2, 4, time_str, 8, FG);
    }

    /* 右侧：信号（4 格） */
    int bars = rssi_bars(rssi_dbm);
    int sx = DISPLAY_WIDTH - 37; /* 信号图标左缘 */
    int bot = 4 + 8;
    for (int i = 0; i < 4; i++) {
        int h = 2 + i * 2; /* 2,4,6,8 */
        uint16_t c = (i < bars) ? RGB565(0xFF, 0xFF, 0xFF) : RGB565(0x33, 0x33, 0x33);
        fb_fill_rect(sx + i * 3, bot - h, 2, h, c);
    }

    /* 右侧：电池（外框 + 填充 + 正极帽） */
    int bw = 14, bh = 8, by = 4;
    int bx = DISPLAY_WIDTH - 2 - bw - 2; /* 靠右边缘，预留 2px 正极帽 + 2px 边距 */
    fb_fill_rect(bx, by, bw, bh, RGB565(0x66, 0x66, 0x66));                        /* 外框 */
    fb_fill_rect(bx + 1, by + 1, bw - 2, bh - 2, RGB565(0x00, 0x00, 0x00));        /* 内底 */
    int fill = (bw - 4) * battery_pct / 100;
    if (fill > 0) {
        uint16_t bc = RGB565(battery_pct > 20 ? 0x00 : 0xFF,
                             battery_pct > 20 ? 0xE6 : 0x17,
                             battery_pct > 20 ? 0x76 : 0x44);
        fb_fill_rect(bx + 2, by + 2, fill, bh - 4, bc);
    }
    fb_fill_rect(bx + bw, by + 2, 2, bh - 4, RGB565(0xFF, 0xFF, 0xFF));            /* 正极帽 */
}

/* ---------------- 整屏刷新 ---------------- */

void display_update(void)
{
    spi_send_cmd(0x2A); /* CASET */
    uint8_t d[4] = { 0, 0, 0, DISPLAY_WIDTH - 1 };
    spi_send_data(d, 4);
    spi_send_cmd(0x2B); /* RASET */
    d[2] = 0;
    d[3] = DISPLAY_HEIGHT - 1;
    spi_send_data(d, 4);
    spi_send_cmd(0x2C); /* RAMWR */

    gpio_set_level(PIN_DC, 1);
    const uint8_t *p = (const uint8_t *)s_fb;
    size_t total = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    size_t off = 0;
    while (off < total) {
        size_t n = (total - off > FB_CHUNK) ? FB_CHUNK : (total - off);
        /* fb 在 PSRAM 时 SPI DMA 无法直接读，先拷到内部 RAM 缓冲再发出；
         * fb 在内部 RAM 时同样走该缓冲，逻辑统一。 */
        memcpy(s_dma_buf, p + off, n);
        spi_transaction_t t = { 0 };
        t.length = n * 8;
        t.tx_buffer = s_dma_buf;
        if (off + n < total) {
            t.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
        }
        spi_device_transmit(s_spi, &t);
        off += n;
    }
}
