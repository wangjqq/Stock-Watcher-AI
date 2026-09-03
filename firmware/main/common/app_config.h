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
#define CONFIG_INTERFACE_MAX     8
#define CONFIG_NAME_MAX          32
#define CONFIG_WIDGET_MAX        32
#define CONFIG_APP_MAX           8
#define CONFIG_ALERT_MAX         8
#define CONFIG_HEADER_MAX        4    /* 每个接口的自定义头数量上限 */
#define CONFIG_HEADER_LEN        128  /* 单条头 "Key: Value" 的最大长度 */
#define CONFIG_BODY_MAX          256  /* POST 请求体最大长度 */

/* 字段显示格式 */
typedef enum {
    FORMAT_RAW = 0,     /* 原样输出 */
    FORMAT_PERCENT,     /* 百分号，如 +1.23% */
    FORMAT_DECIMAL,     /* 固定小数位 + 可选单位 */
} format_type_t;

/* 请求方法 */
typedef enum {
    REQ_GET = 0,
    REQ_POST,
} http_method_t;

/* 一个数据接口（每个接口可独立配置刷新时间） */
typedef struct {
    uint32_t id;                        /* 接口 ID（widget 通过它引用数据源） */
    char     name[CONFIG_NAME_MAX];     /* 显示名 */
    char     url[CONFIG_API_URL_MAX];   /* 数据接口地址 */
    uint32_t refresh_interval_ms;       /* 该接口的刷新频率 */
    http_method_t method;               /* GET / POST */
    char     headers[CONFIG_HEADER_MAX][CONFIG_HEADER_LEN]; /* 自定义头，每行 "Key: Value" */
    char     post_body[CONFIG_BODY_MAX];/* POST 请求体（可空） */
} interface_t;

/* 屏幕上的一个显示块（像素布局） */
typedef struct {
    uint32_t interface_id;            /* 数据源接口 ID */
    char   label[CONFIG_LABEL_MAX];           /* 显示标签 */
    char   field_path[CONFIG_FIELD_PATH_MAX]; /* 字段路径，如 stock.name / data[0].price */
    format_type_t format;                     /* 格式化方式 */
    int    decimal_places;                    /* 小数位 */
    char   unit[CONFIG_UNIT_MAX];             /* 单位 */
    bool   use_change_color;                  /* 涨跌颜色（红涨绿跌） */
    int    x, y, w, h;                        /* 画布内像素位置/大小 */
    int    font_size;                         /* 字号 */
} widget_t;

/* 一个应用 = 一套屏幕布局（一个页面 = 一个应用） */
typedef struct {
    char     name[CONFIG_NAME_MAX];           /* 应用名 */
    uint32_t widget_count;
    widget_t widgets[CONFIG_WIDGET_MAX];
} app_t;

/* 提醒条件：> 或 < */
typedef enum {
    ALERT_GT = 0,   /* 字段值 > 阈值 */
    ALERT_LT,       /* 字段值 < 阈值 */
} alert_cond_t;

/* 条件提醒规则（全局，监控某个接口的数值字段） */
typedef struct {
    bool     enabled;                        /* 是否启用 */
    uint32_t interface_id;                   /* 数据源接口 ID */
    char     field_path[CONFIG_FIELD_PATH_MAX]; /* 要监控的数值字段路径 */
    alert_cond_t condition;                  /* > 或 < */
    float    threshold;                      /* 触发阈值 */
} alert_t;

/* 设备全局配置 */
typedef struct {
    char     device_name[CONFIG_DEVICE_NAME_MAX];
    char     ssid[CONFIG_SSID_MAX];           /* 为空则 STA 待机，由设备端 WiFi 页配置 */
    char     password[CONFIG_PASS_MAX];
    uint8_t  brightness;                      /* 屏幕亮度 0-100 */
    bool     auto_brightness;                 /* 光敏自动调节屏幕亮度 */
    uint32_t screen_timeout_s;                /* 无操作后屏幕休眠秒数，0=关闭 */
    uint32_t auto_rotate_s;                   /* 应用自动轮播间隔秒数，0=关闭 */
    bool     deep_sleep_enabled;              /* 深度睡眠总开关（固定时段整机休眠 <1mA） */
    uint8_t  deep_sleep_start_hh;             /* 入睡时间 小时 0-23 */
    uint8_t  deep_sleep_start_mm;             /* 入睡时间 分钟 0-59 */
    uint8_t  deep_sleep_end_hh;               /* 唤醒时间 小时 0-23（end<start 视为跨午夜，相等视为未配置） */
    uint8_t  deep_sleep_end_mm;               /* 唤醒时间 分钟 0-59 */
    bool     buzzer_enabled;                  /* 蜂鸣器开关 */
    uint8_t  buzzer_volume;                   /* 蜂鸣音量 0-100 */
    uint32_t interface_count;
    interface_t interfaces[CONFIG_INTERFACE_MAX];
    uint32_t app_count;
    app_t    apps[CONFIG_APP_MAX];            /* 应用列表，第一个为开机默认应用 */
    uint32_t alert_count;
    alert_t  alerts[CONFIG_ALERT_MAX];        /* 条件提醒规则 */
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
