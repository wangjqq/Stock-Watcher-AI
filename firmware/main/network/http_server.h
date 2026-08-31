#pragma once

#include "esp_err.h"

/* 启动设备内置 HTTP 服务：
 * - 静态资源：前端管理页面（内嵌在固件里，见 web_assets.c）
 * - REST API：/api/config、/api/interface/test、/api/fields、/api/status
 */
esp_err_t http_server_start(void);
