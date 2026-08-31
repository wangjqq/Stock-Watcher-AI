#pragma once

#include "esp_err.h"

/* 同步 GET 请求 url，响应体写入 out（以 \0 结尾，最多 out_size-1 字节） */
esp_err_t data_fetch(const char *url, char *out, size_t out_size, int timeout_ms);
