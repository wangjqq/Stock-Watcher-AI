#pragma once

#include "app_config.h"
#include "cJSON.h"
#include "esp_err.h"

#define FIELD_MAX 128
#define FIELD_TYPE_MAX 8
#define FIELD_SAMPLE_MAX 64

/* 解析出的一个叶子字段 */
typedef struct {
    char path[CONFIG_FIELD_PATH_MAX];  /* 完整路径，如 stock.name / data[0].price */
    char type[FIELD_TYPE_MAX];         /* string / int / float / bool / null */
    char sample[FIELD_SAMPLE_MAX];     /* 示例值 */
} field_info_t;

typedef struct {
    field_info_t items[FIELD_MAX];
    uint32_t count;
} field_list_t;

/* 通用 JSON 自动解析：递归展开对象/数组，生成字段列表 */
esp_err_t field_parse(const char *json, field_list_t *list);

/* 按路径查找 JSON 节点，支持 a.b.c 和 arr[0].x 语法 */
cJSON *json_get_by_path(cJSON *root, const char *path);
