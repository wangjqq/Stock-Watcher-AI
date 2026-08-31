#pragma once

#include "app_config.h"
#include "cJSON.h"

#include <stdint.h>

typedef struct {
    char text[128];    /* 格式化后的文本 */
    uint32_t color;    /* RGB888，涨跌色等 */
} formatted_t;

/* 根据 widget 配置格式化 cJSON 值 */
void formatter_apply(const cJSON *value, const widget_t *w, formatted_t *out);
