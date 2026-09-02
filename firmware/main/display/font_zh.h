#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* ------------------------------------------------------------------
 * 中文字库（GB2312 一级汉字 16x16 点阵，独立 fonts 数据分区，mmap 直读）
 *
 * 字库分区由 tools/gen_font.py 生成并烧录（见 PINMAP.md / README）。
 * 未烧录字库时 font_zh_is_ready() 返回 false，中文显示退化为方块，不影响运行。
 * ------------------------------------------------------------------ */

/* 初始化：内存映射 fonts 分区。可重复调用，成功后 no-op。 */
esp_err_t font_zh_init(void);

bool font_zh_is_ready(void);

/* 把一个 UTF-8 中文字符（3 字节，s 指向首字节）转成 GB2312 码（如 0xB0A1）。
 * 非中文 / 不在一级字库 / 未初始化时返回 0。 */
uint16_t font_zh_utf8_to_gb(const uint8_t *s);

/* 按 GB2312 码取 16x16 点阵字模（32 字节，行序每行 2 字节高位在前）。
 * 未命中或未初始化时返回 NULL。 */
const uint8_t *font_zh_get_glyph(uint16_t gb);
