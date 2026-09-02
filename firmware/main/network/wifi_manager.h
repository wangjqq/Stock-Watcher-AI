#pragma once

#include "app_config.h"
#include "esp_err.h"
#include "esp_wifi.h"

/* 初始化 Wi-Fi（STA + SoftAP 并存）：
 * - 配置里已有 ssid → STA 连接该网络；否则 → STA 待机，等待设备端「系统 → WiFi」页手动配置
 * - 同时始终开放 SoftAP 无密码热点（StockWatcher-XXXX），连上热点后经 192.168.4.1 访问配置页 */
esp_err_t wifi_manager_init(const app_config_t *cfg);

/* 配置变化后调用：ssid 或 password 变化时重新配置并重连（无需重启），未变化则无操作 */
esp_err_t wifi_manager_apply_config(const app_config_t *cfg);

/* 设备端 WiFi 页手动连接指定网络（仅 STA）。只切换连接，不读写配置 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/* 手动断开并停止自动重连，进入 STA 待机（不连接任何网络） */
void wifi_manager_disconnect(void);

/* 启动一次异步扫描（STA 模式）。完成后触发 WIFI_EVENT_SCAN_DONE，
 * 可轮询 wifi_manager_scan_done() 判断是否结束。 */
esp_err_t wifi_manager_scan_start(void);

/* 扫描是否已完成（完成后用 wifi_manager_scan_get_results 取结果） */
bool wifi_manager_scan_done(void);

/* 取扫描结果：复制到 aps（最多 max 条），返回实际数量；取完后自动释放扫描内存 */
int wifi_manager_scan_get_results(wifi_ap_record_t *aps, int max);

/* 取消/结束当前扫描并释放扫描结果内存（未在扫描时调用无副作用） */
void wifi_manager_scan_stop(void);

/* 启动 mDNS，注册 http 服务，可通过 http://<hostname>.local 访问 */
esp_err_t wifi_manager_start_mdns(const char *hostname);

/* 当前是否已接入网络（STA 已连上并拿到 IP） */
bool wifi_manager_is_connected(void);

/* 返回当前 STA 信号强度 RSSI（dBm）；未连接返回 0 */
int wifi_manager_get_rssi(void);

/* 返回最近一次断开原因码（esp_wifi 的 WIFI_REASON_*）；0=无/已连接 */
int wifi_manager_last_disconnect_reason(void);

/* 返回当前已连接网络的 SSID；未连接返回空串 */
const char *wifi_manager_connected_ssid(void);

/* 返回当前 IP 字符串（未连接返回空串），写入 out */
void wifi_manager_ip_str(char *out, size_t out_size);

/* 返回 SoftAP 热点名（如 StockWatcher-A1B2，无密码） */
const char *wifi_manager_ap_ssid(void);

/* 返回 SoftAP 自身 IP（默认 192.168.4.1），写入 out */
void wifi_manager_ap_ip_str(char *out, size_t out_size);
