#pragma once

#include "app_config.h"
#include "esp_err.h"

/* 初始化 Wi-Fi：
 * - 配置里已有 ssid → 以 STA 连接
 * - 否则 → 开启 AP 配网模式（热点名 StockWatcher-xxxx）
 */
esp_err_t wifi_manager_init(const app_config_t *cfg);

/* 网页保存配置后调用：ssid 变化时重新配置并重连（无需重启），未变化则无操作 */
esp_err_t wifi_manager_apply_config(const app_config_t *cfg);

/* 启动 mDNS，注册 http 服务，可通过 http://<hostname>.local 访问 */
esp_err_t wifi_manager_start_mdns(const char *hostname);

/* 当前是否已接入网络（STA 已连上） */
bool wifi_manager_is_connected(void);

/* 返回当前 STA 信号强度 RSSI（dBm）；未连接返回 0 */
int wifi_manager_get_rssi(void);

/* 返回当前 IP 字符串（未连接返回空串），写入 out */
void wifi_manager_ip_str(char *out, size_t out_size);
