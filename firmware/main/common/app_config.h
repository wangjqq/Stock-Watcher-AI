#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define CONFIG_DEVICE_NAME_MAX   32
#define CONFIG_SSID_MAX          32
#define CONFIG_PASS_MAX          64
#define CONFIG_API_URL_MAX       256
#define CONFIG_LABEL_MAX         32
#define CONFIG_FIELD_PATH_MAX    128
#define CONFIG_UNIT_MAX          16
#define CONFIG_WIDGET_MAX        32

/* 字段显示格式 */
typedef enum {
    FORMAT_RAW = 0,     /* 原样输出 */
    FORMAT_PERCENT,     /* 百分号，如 +1.23% */
    FORMAT_DECIMAL,     /* 固定小数位 + 可选单位 */
} format_type_t;

/* 屏幕上的一个显示块（网格布局） */
typedef struct {
    char   label[CONFIG_LABEL_MAX];           /* 显示标签 */
    char   field_path[CONFIG_FIELD_PATH_MAX]; /* 字段路径，如 stock.name / data[0].price */
    format_type_t format;                     /* 格式化方式 */
    int    decimal_places;                    /* 小数位 */
    char   unit[CONFIG_UNIT_MAX];             /* 单位 */
    bool   use_change_color;                  /* 涨跌颜色（红涨绿跌） */
    int    x, y, w, h;                        /* 网格位置/大小 */
    int    font_size;                         /* 字号 */
} widget_t;

/* 设备全局配置 */
typedef struct {
    char     device_name[CONFIG_DEVICE_NAME_MAX];
    char     ssid[CONFIG_SSID_MAX];           /* 为空则进入 AP 配网模式 */
    char     password[CONFIG_PASS_MAX];
    char     api_url[CONFIG_API_URL_MAX];     /* 股票数据接口地址 */
    uint32_t refresh_interval_ms;             /* 刷新频率 */
    uint8_t  brightness;                      /* 屏幕亮度 0-100 */
    uint32_t widget_count;
    widget_t widgets[CONFIG_WIDGET_MAX];
} app_config_t;

/* 填充默认值 */
void config_defaults(app_config_t *cfg);

/* 从 NVS 读取；无配置时填充默认值并返回 ESP_ERR_NOT_FOUND */
esp_err_t config_load(app_config_t *cfg);

/* 保存到 NVS */
esp_err_t config_save(const app_config_t *cfg);

/* 清空全部配置（抹除 "app" 命名空间） */
esp_err_t config_reset(void);

/* 序列化为 JSON 字符串（malloc，调用方 free） */
char *config_to_json(const app_config_t *cfg);

/* 从 JSON 更新配置（未出现的字段保留原值） */
esp_err_t config_from_json(const char *json, app_config_t *cfg);
