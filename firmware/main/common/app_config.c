#include "app_config.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "config";

void config_defaults(app_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->device_name, sizeof(cfg->device_name), "StockWatcher");
    cfg->refresh_interval_ms = 5000;
    cfg->brightness = 80;
    cfg->widget_count = 0;
}

esp_err_t config_load(app_config_t *cfg)
{
    nvs_handle_t h;
    if (nvs_open("app", NVS_READONLY, &h) != ESP_OK) {
        config_defaults(cfg);
        return ESP_ERR_NOT_FOUND;
    }
    size_t len = sizeof(*cfg);
    esp_err_t err = nvs_get_blob(h, "app_cfg", cfg, &len);
    nvs_close(h);
    if (err != ESP_OK) {
        config_defaults(cfg);
        return err;
    }
    return ESP_OK;
}

esp_err_t config_save(const app_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("app", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs open failed: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(h, "app_cfg", cfg, sizeof(*cfg));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t config_reset(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("app", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs open failed: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_erase_all(h);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "config erased");
    }
    return err;
}

static void widget_to_json(cJSON *w, const widget_t *src)
{
    cJSON_AddStringToObject(w, "label", src->label);
    cJSON_AddStringToObject(w, "field_path", src->field_path);
    cJSON_AddNumberToObject(w, "format", src->format);
    cJSON_AddNumberToObject(w, "decimal_places", src->decimal_places);
    cJSON_AddStringToObject(w, "unit", src->unit);
    cJSON_AddBoolToObject(w, "use_change_color", src->use_change_color);
    cJSON_AddNumberToObject(w, "x", src->x);
    cJSON_AddNumberToObject(w, "y", src->y);
    cJSON_AddNumberToObject(w, "w", src->w);
    cJSON_AddNumberToObject(w, "h", src->h);
    cJSON_AddNumberToObject(w, "font_size", src->font_size);
}

char *config_to_json(const app_config_t *cfg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "device_name", cfg->device_name);
    cJSON_AddStringToObject(root, "ssid", cfg->ssid);
    cJSON_AddStringToObject(root, "password", cfg->password);
    cJSON_AddStringToObject(root, "api_url", cfg->api_url);
    cJSON_AddNumberToObject(root, "refresh_interval_ms", cfg->refresh_interval_ms);
    cJSON_AddNumberToObject(root, "brightness", cfg->brightness);

    cJSON *arr = cJSON_AddArrayToObject(root, "widgets");
    for (uint32_t i = 0; i < cfg->widget_count; i++) {
        cJSON *w = cJSON_CreateObject();
        widget_to_json(w, &cfg->widgets[i]);
        cJSON_AddItemToArray(arr, w);
    }

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return str;
}

static void str_field(cJSON *root, const char *key, char *dst, size_t dst_size)
{
    cJSON *v = cJSON_GetObjectItem(root, key);
    if (cJSON_IsString(v)) {
        strlcpy(dst, v->valuestring, dst_size);
    }
}

esp_err_t config_from_json(const char *json, app_config_t *cfg)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    str_field(root, "device_name", cfg->device_name, sizeof(cfg->device_name));
    str_field(root, "ssid", cfg->ssid, sizeof(cfg->ssid));
    str_field(root, "password", cfg->password, sizeof(cfg->password));
    str_field(root, "api_url", cfg->api_url, sizeof(cfg->api_url));

    cJSON *v = cJSON_GetObjectItem(root, "refresh_interval_ms");
    if (cJSON_IsNumber(v) && v->valueint > 0) {
        cfg->refresh_interval_ms = v->valueint;
    }
    v = cJSON_GetObjectItem(root, "brightness");
    if (cJSON_IsNumber(v)) {
        cfg->brightness = (uint8_t)(v->valueint > 100 ? 100 : v->valueint);
    }

    /* widgets 整体替换（上限 CONFIG_WIDGET_MAX） */
    cJSON *arr = cJSON_GetObjectItem(root, "widgets");
    if (cJSON_IsArray(arr)) {
        cfg->widget_count = 0;
        cJSON *it;
        cJSON_ArrayForEach(it, arr) {
            if (cfg->widget_count >= CONFIG_WIDGET_MAX) {
                break;
            }
            widget_t *w = &cfg->widgets[cfg->widget_count];
            memset(w, 0, sizeof(*w));
            str_field(it, "label", w->label, sizeof(w->label));
            str_field(it, "field_path", w->field_path, sizeof(w->field_path));
            str_field(it, "unit", w->unit, sizeof(w->unit));
            cJSON *n = cJSON_GetObjectItem(it, "format");
            if (cJSON_IsNumber(n)) {
                w->format = (format_type_t)n->valueint;
            }
            n = cJSON_GetObjectItem(it, "decimal_places");
            if (cJSON_IsNumber(n)) {
                w->decimal_places = n->valueint;
            }
            n = cJSON_GetObjectItem(it, "use_change_color");
            if (cJSON_IsBool(n)) {
                w->use_change_color = cJSON_IsTrue(n);
            }
            n = cJSON_GetObjectItem(it, "x");
            if (cJSON_IsNumber(n)) {
                w->x = n->valueint;
            }
            n = cJSON_GetObjectItem(it, "y");
            if (cJSON_IsNumber(n)) {
                w->y = n->valueint;
            }
            n = cJSON_GetObjectItem(it, "w");
            if (cJSON_IsNumber(n)) {
                w->w = n->valueint;
            }
            n = cJSON_GetObjectItem(it, "h");
            if (cJSON_IsNumber(n)) {
                w->h = n->valueint;
            }
            n = cJSON_GetObjectItem(it, "font_size");
            if (cJSON_IsNumber(n)) {
                w->font_size = n->valueint;
            }
            cfg->widget_count++;
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}
