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

/* 拆分 "Key: Value" 头：key 为冒号前去除首尾空白，value 为冒号后去除首尾空白。
 * 找不到冒号时整行视为 key、value 为空。 */
static void split_header(const char *line, char *key, size_t key_size, char *value, size_t value_size)
{
    const char *colon = strchr(line, ':');
    size_t klen, vlen;

    if (colon) {
        klen = (size_t)(colon - line);
        vlen = strlen(colon + 1);
    } else {
        klen = strlen(line);
        vlen = 0;
    }

    /* 去首尾空白（空格 / tab） */
    const char *p = line;
    const char *ke = p + klen;
    while (p < ke && (*p == ' ' || *p == '\t')) {
        p++;
    }
    while (ke > p && (ke[-1] == ' ' || ke[-1] == '\t')) {
        ke--;
    }
    if (ke - p >= (ptrdiff_t)key_size) {
        ke = p + key_size - 1;
    }
    memcpy(key, p, (size_t)(ke - p));
    key[ke - p] = '\0';

    if (colon && vlen > 0) {
        const char *vs = colon + 1;
        const char *ve = vs + vlen;
        while (vs < ve && (*vs == ' ' || *vs == '\t')) {
            vs++;
        }
        while (ve > vs && (ve[-1] == ' ' || ve[-1] == '\t')) {
            ve--;
        }
        if (ve - vs >= (ptrdiff_t)value_size) {
            ve = vs + value_size - 1;
        }
        memcpy(value, vs, (size_t)(ve - vs));
        value[ve - vs] = '\0';
    } else {
        value[0] = '\0';
    }
}

esp_err_t data_fetch_iface(const interface_t *it, char *out, size_t out_size, int timeout_ms)
{
    if (!it || !it->url[0] || !out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    fetch_ctx_t ctx = {
        .buf = out,
        .cap = out_size,
        .len = 0,
    };

    esp_http_client_config_t cfg = {
        .url = it->url,
        .method = (it->method == REQ_POST) ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = timeout_ms,
        .event_handler = http_event,
        .user_data = &ctx,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    /* 自定义头：每行 "Key: Value" */
    for (int i = 0; i < CONFIG_HEADER_MAX; i++) {
        if (it->headers[i][0] == '\0') {
            continue;
        }
        char key[CONFIG_HEADER_LEN];
        char value[CONFIG_HEADER_LEN];
        split_header(it->headers[i], key, sizeof(key), value, sizeof(value));
        if (key[0] != '\0') {
            esp_http_client_set_header(client, key, value);
        }
    }

    /* POST body（仅在 POST 且有内容时发送） */
    if (it->method == REQ_POST && it->post_body[0] != '\0') {
        esp_http_client_set_post_field(client, it->post_body, (int)strlen(it->post_body));
    }

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    out[ctx.len] = '\0';
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "fetched %u bytes from %s", (unsigned)ctx.len, it->url);
    } else {
        ESP_LOGW(TAG, "fetch %s failed: %s", it->url, esp_err_to_name(err));
    }
    return err;
}

esp_err_t data_fetch(const char *url, char *out, size_t out_size, int timeout_ms)
{
    if (!url || !out || out_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }
    interface_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.method = REQ_GET;
    strlcpy(tmp.url, url, sizeof(tmp.url));
    return data_fetch_iface(&tmp, out, out_size, timeout_ms);
}
