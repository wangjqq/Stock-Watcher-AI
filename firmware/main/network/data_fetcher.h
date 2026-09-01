#pragma once

#include "esp_err.h"

#include "app_config.h"

/* 按接口配置同步请求（GET/POST + 自定义头 + POST body），
 * 响应体写入 out（以 \0 结尾，最多 out_size-1 字节） */
esp_err_t data_fetch_iface(const interface_t *it, char *out, size_t out_size, int timeout_ms);

/* 简单同步 GET（用于接口测试等无需配置的场景） */
esp_err_t data_fetch(const char *url, char *out, size_t out_size, int timeout_ms);
