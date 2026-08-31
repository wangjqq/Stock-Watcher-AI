#pragma once

#include <stdbool.h>

#include "app_config.h"

/* 按配置把各接口数据渲染到画布。
 *
 * bodies[i]  对应 cfg->interfaces[i] 最近一次获取到的 JSON 原文（可能为 NULL）
 * has_body[i] 表示该接口是否已有有效数据
 * app_index 当前显示的应用（渲染该应用的 widget 布局）
 * trend_out（可空）返回涨跌趋势：1 涨 / -1 跌 / 0 平（取第一个涨跌色数字字段）。
 */
void layout_render(const app_config_t *cfg, uint32_t app_index,
                   const char *const *bodies, const bool *has_body, int *trend_out);
