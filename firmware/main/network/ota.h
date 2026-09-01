#pragma once

#include "esp_http_server.h"

/* 注册 OTA 升级端点：POST /api/ota，请求体为固件 bin。
 * 边收边写入空闲 OTA 分区，校验通过后切换启动分区并重启。 */
esp_err_t ota_register(httpd_handle_t server);
