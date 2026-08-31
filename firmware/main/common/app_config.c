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
    cfg->brightness = 80;
    cfg->buzzer_enabled = true;
    cfg->buzzer_volume = 70;
    cfg->interface_count = 0;
    /* 默认一个空应用「盯盘」，保证开机有可进入的第一个应用 */
    cfg->app_count = 1;
    snprintf(cfg->apps[0].name, sizeof(cfg->apps[0].name), "盯盘");
    cfg->apps[0].widget_count = 0;
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
    /* blob 尺寸与当前结构不一致（旧版本/损坏数据）时回退默认值，
     * 避免使用未初始化的结构尾部字段 */
    if (err != ESP_OK || len != sizeof(*cfg)) {
        config_defaults(cfg);
        return err != ESP_OK ? err : ESP_ERR_NOT_FOUND;
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
    cJSON_AddNumberToObject(w, "interface_id", src->interface_id);
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

static void interface_to_json(cJSON *it, const interface_t *src)
{
    cJSON_AddNumberToObject(it, "id", src->id);
    cJSON_AddStringToObject(it, "name", src->name);
    cJSON_AddStringToObject(it, "url", src->url);
    cJSON_AddNumberToObject(it, "refresh_interval_ms", src->refresh_interval_ms);
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
    cJSON_AddNumberToObject(root, "brightness", cfg->brightness);
    cJSON_AddBoolToObject(root, "buzzer_enabled", cfg->buzzer_enabled);
    cJSON_AddNumberToObject(root, "buzzer_volume", cfg->buzzer_volume);

    cJSON *ia = cJSON_AddArrayToObject(root, "interfaces");
    for (uint32_t i = 0; i < cfg->interface_count; i++) {
        cJSON *it = cJSON_CreateObject();
        interface_to_json(it, &cfg->interfaces[i]);
        cJSON_AddItemToArray(ia, it);
    }

    cJSON *arr = cJSON_AddArrayToObject(root, "apps");
    for (uint32_t a = 0; a < cfg->app_count; a++) {
        const app_t *app = &cfg->apps[a];
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "name", app->name);
        cJSON *ws = cJSON_AddArrayToObject(ap, "widgets");
        for (uint32_t i = 0; i < app->widget_count; i++) {
            cJSON *w = cJSON_CreateObject();
            widget_to_json(w, &app->widgets[i]);
            cJSON_AddItemToArray(ws, w);
        }
        cJSON_AddItemToArray(arr, ap);
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

static void widget_from_json(cJSON *it, widget_t *w)
{
    memset(w, 0, sizeof(*w));
    str_field(it, "label", w->label, sizeof(w->label));
    str_field(it, "field_path", w->field_path, sizeof(w->field_path));
    str_field(it, "unit", w->unit, sizeof(w->unit));
    cJSON *n = cJSON_GetObjectItem(it, "format");
    if (cJSON_IsNumber(n)) {
        w->format = (format_type_t)n->valueint;
    }
    n = cJSON_GetObjectItem(it, "interface_id");
    if (cJSON_IsNumber(n)) {
        w->interface_id = (uint32_t)n->valueint;
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

    cJSON *v = cJSON_GetObjectItem(root, "brightness");
    if (cJSON_IsNumber(v)) {
        cfg->brightness = (uint8_t)(v->valueint > 100 ? 100 : v->valueint);
    }

    v = cJSON_GetObjectItem(root, "buzzer_enabled");
    if (cJSON_IsBool(v)) {
        cfg->buzzer_enabled = cJSON_IsTrue(v);
    }
    v = cJSON_GetObjectItem(root, "buzzer_volume");
    if (cJSON_IsNumber(v)) {
        cfg->buzzer_volume = (uint8_t)(v->valueint > 100 ? 100 : v->valueint);
    }

    /* interfaces 整体替换（上限 CONFIG_INTERFACE_MAX） */
    cJSON *ia = cJSON_GetObjectItem(root, "interfaces");
    if (cJSON_IsArray(ia)) {
        cfg->interface_count = 0;
        cJSON *it;
        cJSON_ArrayForEach(it, ia) {
            if (cfg->interface_count >= CONFIG_INTERFACE_MAX) {
                break;
            }
            interface_t *iface = &cfg->interfaces[cfg->interface_count];
            memset(iface, 0, sizeof(*iface));
            cJSON *n = cJSON_GetObjectItem(it, "id");
            if (cJSON_IsNumber(n)) {
                iface->id = (uint32_t)n->valueint;
            }
            str_field(it, "name", iface->name, sizeof(iface->name));
            str_field(it, "url", iface->url, sizeof(iface->url));
            n = cJSON_GetObjectItem(it, "refresh_interval_ms");
            if (cJSON_IsNumber(n) && n->valueint > 0) {
                iface->refresh_interval_ms = (uint32_t)n->valueint;
            } else {
                iface->refresh_interval_ms = 5000; /* 缺省 5 秒 */
            }
            cfg->interface_count++;
        }
    }

    /* apps 整体替换（上限 CONFIG_APP_MAX，每个应用含独立 widgets） */
    cJSON *apps = cJSON_GetObjectItem(root, "apps");
    if (cJSON_IsArray(apps)) {
        cfg->app_count = 0;
        cJSON *ap;
        cJSON_ArrayForEach(ap, apps) {
            if (cfg->app_count >= CONFIG_APP_MAX) {
                break;
            }
            app_t *app = &cfg->apps[cfg->app_count];
            memset(app, 0, sizeof(*app));
            str_field(ap, "name", app->name, sizeof(app->name));
            cJSON *ws = cJSON_GetObjectItem(ap, "widgets");
            if (cJSON_IsArray(ws)) {
                cJSON *it;
                cJSON_ArrayForEach(it, ws) {
                    if (app->widget_count >= CONFIG_WIDGET_MAX) {
                        break;
                    }
                    widget_from_json(it, &app->widgets[app->widget_count]);
                    app->widget_count++;
                }
            }
            cfg->app_count++;
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}
