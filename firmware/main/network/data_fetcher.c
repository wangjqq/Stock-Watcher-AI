#include "data_fetcher.h"

#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "fetch";

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} fetch_ctx_t;

static esp_err_t http_event(esp_http_client_event_t *evt)
{
    fetch_ctx_t *ctx = evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && ctx) {
        size_t room = ctx->cap - ctx->len - 1;
        size_t copy = evt->data_len < room ? evt->data_len : room;
        memcpy(ctx->buf + ctx->len, evt->data, copy);
        ctx->len += copy;
    }
    return ESP_OK;
}

esp_err_t data_fetch(const char *url, char *out, size_t out_size, int timeout_ms)
{
    if (!url || !out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    fetch_ctx_t ctx = {
        .buf = out,
        .cap = out_size,
        .len = 0,
    };

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = timeout_ms,
        .event_handler = http_event,
        .user_data = &ctx,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    out[ctx.len] = '\0';
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "fetched %u bytes from %s", (unsigned)ctx.len, url);
    } else {
        ESP_LOGW(TAG, "fetch %s failed: %s", url, esp_err_to_name(err));
    }
    return err;
}
