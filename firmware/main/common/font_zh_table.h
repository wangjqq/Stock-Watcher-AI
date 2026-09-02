#pragma once

#include <stdint.h>

/* Unicode(UTF-8 解码后) → GB2312 映射表。
 * 由 tools/gen_font.py 自动生成（按 unicode 升序，供二分查找），
 * 覆盖 GB2312 一级汉字区（区 16~55，共 3755 个常用字）。 */
typedef struct {
    uint16_t unicode;
    uint16_t gb; /* GB2312 码，如 0xB0A1 */
} utf8_gb_entry_t;

extern const utf8_gb_entry_t g_utf8_gb_table[];
extern const unsigned g_utf8_gb_count;
