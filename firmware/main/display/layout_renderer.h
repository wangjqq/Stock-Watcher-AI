#pragma once

#include <stdbool.h>

#include "app_config.h"

/* 按配置把各接口数据渲染到画布。
 *
 * bodies[i]  对应 cfg->interfaces[i] 最近一次获取到的 JSON 原文（可能为 NULL）
 * has_body[i] 表示该接口是否已有有效数据
 * 每个 widget 通过 interface_id 找到自己的数据源接口。
 */
void layout_render(const app_config_t *cfg, const char *const *bodies, const bool *has_body);
