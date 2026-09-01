#include "ota.h"

#include <string.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";

/* POST /api/ota
 * body: 固件 bin（app 镜像）。边收边写，失败不切换分区，旧固件不受影响。 */
static esp_err_t handle_ota(httpd_req_t *req)
{
    const esp_partition_t *upd = esp_ota_get_next_update_partition(NULL); /* 空闲槽 */
    if (!upd) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no ota partition");
    }
    ESP_LOGI(TAG, "ota -> %s @0x%lx, %lu bytes",
             upd->label, (unsigned long)upd->address, (unsigned long)req->content_len);

    esp_ota_handle_t h;
    esp_err_t err = esp_ota_begin(upd, OTA_WITHOUT_SIZE, &h);
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
    }

    char buf[4096];
    int remaining = req->content_len;
    while (remaining > 0) {
        int len = httpd_req_recv(req, buf, remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf));
        if (len <= 0) { /* 上传中断：丢弃本次 OTA，当前分区原样 */
            esp_ota_abort(h);
            ESP_LOGE(TAG, "recv aborted, remaining=%d", remaining);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "upload aborted");
        }
        esp_ota_write(h, buf, len);
        remaining -= len;
    }

    err = esp_ota_end(h); /* 收尾 + 镜像校验（哈希/签名） */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota end failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "image invalid");
    }

    err = esp_ota_set_boot_partition(upd); /* 写入 ota_data 切槽 */
    if (err != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "ota done, rebooting...");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    vTaskDelay(pdMS_TO_TICKS(200)); /* 让响应先发出，再重启 */
    esp_restart();
    return ESP_OK;
}

esp_err_t ota_register(httpd_handle_t server)
{
    static const httpd_uri_t uri = {
        .uri = "/api/ota",
        .method = HTTP_POST,
        .handler = handle_ota,
        .user_ctx = NULL,
    };
    return httpd_register_uri_handler(server, &uri);
}
