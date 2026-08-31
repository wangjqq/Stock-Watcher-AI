#pragma once

#include "app_config.h"

/* 按配置把接口返回数据渲染到屏幕 */
void layout_render(const app_config_t *cfg, const char *json_body);
