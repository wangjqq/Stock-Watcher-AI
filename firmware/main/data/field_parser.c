#include "field_parser.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"

static void record_field(field_list_t *list, const char *path, cJSON *node)
{
    if (list->count >= FIELD_MAX) {
        return;
    }
    field_info_t *f = &list->items[list->count++];
    memset(f, 0, sizeof(*f));
    strlcpy(f->path, path, sizeof(f->path));

    if (cJSON_IsString(node)) {
        strlcpy(f->type, "string", sizeof(f->type));
        strlcpy(f->sample, node->valuestring ? node->valuestring : "", sizeof(f->sample));
    } else if (cJSON_IsBool(node)) {
        strlcpy(f->type, "bool", sizeof(f->type));
        strlcpy(f->sample, cJSON_IsTrue(node) ? "true" : "false", sizeof(f->sample));
    } else if (cJSON_IsNumber(node)) {
        if (node->valuedouble == (double)node->valueint) {
            strlcpy(f->type, "int", sizeof(f->type));
            snprintf(f->sample, sizeof(f->sample), "%d", node->valueint);
        } else {
            strlcpy(f->type, "float", sizeof(f->type));
            snprintf(f->sample, sizeof(f->sample), "%g", node->valuedouble);
        }
    } else {
        strlcpy(f->type, "null", sizeof(f->type));
        strlcpy(f->sample, "-", sizeof(f->sample));
    }
}

static void parse_node(cJSON *node, const char *path, field_list_t *list)
{
    if (!node) {
        return;
    }
    if (cJSON_IsArray(node)) {
        int idx = 0;
        for (cJSON *it = node->child; it && list->count < FIELD_MAX; it = it->next, idx++) {
            char p[CONFIG_FIELD_PATH_MAX];
            snprintf(p, sizeof(p), "%s[%d]", path, idx);
            parse_node(it, p, list);
        }
        return;
    }
    if (cJSON_IsObject(node)) {
        for (cJSON *it = node->child; it && list->count < FIELD_MAX; it = it->next) {
            char p[CONFIG_FIELD_PATH_MAX];
            if (strlen(path) > 0) {
                snprintf(p, sizeof(p), "%s.%s", path, it->string);
            } else {
                snprintf(p, sizeof(p), "%s", it->string);
            }
            parse_node(it, p, list);
        }
        return;
    }
    record_field(list, path, node);
}

esp_err_t field_parse(const char *json, field_list_t *list)
{
    if (!json || !list) {
        return ESP_ERR_INVALID_ARG;
    }
    list->count = 0;

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }
    parse_node(root, "", list);
    cJSON_Delete(root);
    return ESP_OK;
}

cJSON *json_get_by_path(cJSON *root, const char *path)
{
    if (!root || !path) {
        return NULL;
    }
    cJSON *cur = root;
    const char *p = path;
    char token[CONFIG_FIELD_PATH_MAX];

    while (*p && cur) {
        int k = 0;
        while (*p && *p != '.' && *p != '[' && k < (int)sizeof(token) - 1) {
            token[k++] = *p++;
        }
        token[k] = '\0';

        if (k > 0) {
            cur = cJSON_GetObjectItemCaseSensitive(cur, token);
            if (!cur) {
                return NULL;
            }
        }
        if (*p == '[') {
            p++;
            int idx = 0;
            while (*p >= '0' && *p <= '9') {
                idx = idx * 10 + (*p - '0');
                p++;
            }
            if (*p == ']') {
                p++;
            }
            cur = cJSON_GetArrayItem(cur, idx);
        } else if (*p == '.') {
            p++;
        }
    }
    return cur;
}
