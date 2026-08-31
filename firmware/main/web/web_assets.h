#pragma once

#include <stdint.h>

/* 内嵌到固件里的一个 web 资源 */
typedef struct {
    const char *path;   /* 请求 URI，如 "/"、"/assets/foo.js" */
    const uint8_t *data;
    uint32_t size;
    const char *mime;
    int gzip;           /* 1 = data 已是 gzip 压缩 */
} web_asset_t;

/* 由 tools/gen_web_assets.py 生成到 web_assets.c */
extern const web_asset_t web_assets[];
extern const uint32_t web_assets_count;
