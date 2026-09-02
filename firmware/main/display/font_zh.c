#include "font_zh.h"

#include "esp_log.h"
#include "esp_partition.h"

#include "font_zh_table.h"

static const char *TAG = "font_zh";

#define GLYPH_BYTES      32   /* 16x16 每字字节数 */
#define GB_L1_Q_START    0xB0 /* 一级汉字区起始（区 16） */
#define GB_L1_Q_END      0xD7 /* 一级汉字区结束（区 55） */

static const void                *s_map = NULL;
static esp_partition_mmap_handle_t s_mmap;

esp_err_t font_zh_init(void)
{
    if (s_map) {
        return ESP_OK;
    }
    /* fonts 数据分区（subtype 0x82），未烧录时分区不存在 → 退化为方块 */
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                           (esp_partition_subtype_t)0x82, "fonts");
    if (!part) {
        ESP_LOGW(TAG, "fonts partition not found, zh display degraded");
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t err = esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA,
                                       &s_map, &s_mmap);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mmap fonts failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "fonts ready, %lu KB mapped", (unsigned long)part->size / 1024);
    return ESP_OK;
}

bool font_zh_is_ready(void)
{
    return s_map != NULL;
}

/* UTF-8 三字节 → Unicode 码点（非 3 字节中文时返回 0） */
static uint32_t utf8_decode(const uint8_t *s)
{
    if (s[0] >= 0xE0 && s[0] <= 0xEF && s[1] && s[2]) {
        return ((uint32_t)(s[0] & 0x0F) << 12) |
               ((uint32_t)(s[1] & 0x3F) << 6) |
               (uint32_t)(s[2] & 0x3F);
    }
    return 0;
}

uint16_t font_zh_utf8_to_gb(const uint8_t *s)
{
    if (!s || !s_map) {
        return 0;
    }
    uint32_t u = utf8_decode(s);
    if (u == 0) {
        return 0;
    }
    /* 映射表按 unicode 升序，二分查找 */
    int lo = 0, hi = (int)g_utf8_gb_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t v = g_utf8_gb_table[mid].unicode;
        if (v == u) {
            return g_utf8_gb_table[mid].gb;
        }
        if (v < u) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

const uint8_t *font_zh_get_glyph(uint16_t gb)
{
    if (!s_map || gb == 0) {
        return NULL;
    }
    uint8_t q = gb >> 8;   /* 区 */
    uint8_t w = gb & 0xFF; /* 位 */
    if (q < GB_L1_Q_START || q > GB_L1_Q_END || w < 0xA1 || w > 0xFE) {
        return NULL;
    }
    uint32_t off = ((uint32_t)(q - GB_L1_Q_START) * 94 + (uint32_t)(w - 0xA1)) * GLYPH_BYTES;
    return (const uint8_t *)s_map + off;
}
