#!/usr/bin/env python3
"""gen_manual.py — 从共享手册数据源生成固件端 C 数组（manual_data.h / manual_data.c）。

数据源：docs/manual.json（与网页端 web/src/pages/UserManual.tsx 共用同一内容，
页面端直接 import 该 JSON 渲染）。本脚本在 CMake 配置阶段运行：读取 JSON，
按设备屏幕 8px 显示宽度折行、分页，生成 C 数组到构建目录，固件直接引用。

设备端约定：
  - 每页 MANUAL_LINES 行；
  - 每行不超过 LINE_UNITS 个 8px 宽度单位（ASCII=1，全角/中文=2），适配 240 宽画布；
  - 章节标题行以 '#' 开头，设备端渲染时用高亮色并去掉 '#'。

用法：
    python gen_manual.py <manual.json> <out.c> <out.h>
由 firmware/main/CMakeLists.txt 在配置阶段调用。
"""

import json
import sys
import unicodedata

MANUAL_LINES = 12   # 每页最大行数
LINE_UNITS = 28     # 每行最大显示宽度单位（8px 单位；28 单位 = 224px < 240 画布）

# ASCII 词允许包含的字符（用于识别可整体回退的英文词/URL，避免被折行拆断）
ASCII_WORD_CHARS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:/-_+%'


def disp_units(ch):
    return 2 if unicodedata.east_asian_width(ch) in ('W', 'F') else 1


def wrap(text):
    """按显示宽度折行，返回行列表。优先在空格/中文标点后断行，避免拆断 URL 等英文词。"""
    lines = []
    cur = ''
    units = 0
    last_break = -1        # 当前行内最后一个允许断行处（cur 的长度）
    last_break_units = 0   # 对应单位数
    for ch in text:
        u = disp_units(ch)
        if units + u > LINE_UNITS:
            if 0 < last_break < len(cur):
                # 回退到最近的断点
                lines.append(cur[:last_break].rstrip())
                cur = cur[last_break:]
                units -= last_break_units
                # 重扫剩余部分里的断点
                last_break, last_break_units = -1, 0
                acc = 0
                for i, c in enumerate(cur):
                    acc += disp_units(c)
                    if c in BREAK_AFTER:
                        last_break = i + 1
                        last_break_units = acc
            else:
                lines.append(cur.rstrip())
                cur = ''
                units = 0
                last_break, last_break_units = -1, 0
        cur += ch
        units += u
        if ch in BREAK_AFTER:
            last_break = len(cur)
            last_break_units = units
    if cur:
        lines.append(cur.rstrip())
    return lines


def collect(data):
    """把章节+段落展平为设备端行序列，章节标题以 '#' 开头。"""
    lines = []
    for sec in data.get('sections', []):
        lines.append('#' + sec['title'])
        for para in sec.get('content', []):
            lines.extend(wrap(para))
    return lines


def c_escape(s):
    return s.replace('\\', '\\\\').replace('"', '\\"')


def main():
    if len(sys.argv) != 4:
        print('usage: gen_manual.py <manual.json> <out.c> <out.h>', file=sys.stderr)
        return 1
    src, out_c, out_h = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(src, encoding='utf-8') as f:
        data = json.load(f)

    lines = collect(data)
    pages = [lines[i:i + MANUAL_LINES] for i in range(0, len(lines), MANUAL_LINES)]
    if not pages:
        pages = [[]]
    n = len(pages)

    with open(out_h, 'w', encoding='utf-8') as f:
        f.write('/* 自动生成：tools/gen_manual.py（数据源 docs/manual.json），请勿手改。 */\n')
        f.write('#ifndef MANUAL_DATA_H\n')
        f.write('#define MANUAL_DATA_H\n\n')
        f.write('#define MANUAL_PAGES %d\n' % n)
        f.write('#define MANUAL_LINES %d\n\n' % MANUAL_LINES)
        f.write('extern const char *const manual_pages[][MANUAL_LINES];\n\n')
        f.write('#endif /* MANUAL_DATA_H */\n')

    with open(out_c, 'w', encoding='utf-8') as f:
        f.write('/* 自动生成：tools/gen_manual.py（数据源 docs/manual.json），请勿手改。 */\n')
        f.write('#include "manual_data.h"\n\n')
        f.write('const char *const manual_pages[%d][MANUAL_LINES] = {\n' % n)
        for p in pages:
            f.write('    {\n')
            for line in p:
                f.write('        "%s",\n' % c_escape(line))
            for _ in range(MANUAL_LINES - len(p)):
                f.write('        "",\n')
            f.write('    },\n')
        f.write('};\n')

    print('generated %s / %s (%d pages, %d lines)' % (out_c, out_h, n, len(lines)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
