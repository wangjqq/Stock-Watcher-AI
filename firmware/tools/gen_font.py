#!/usr/bin/env python3
"""生成中文字库相关产物（GB2312 一级汉字字库）。

产物：
  1. firmware/main/common/font_zh_table.c   Unicode -> GB2312 映射表（纯标准库，可离线生成）
  2. firmware/build/fonts.bin               一级汉字 16x16 点阵字库（写入 fonts 数据分区）

fonts.bin 布局（GB2312 区位码索引）：
  offset = (区 - 0xB0) * 94 * 32 + (位 - 0xA1) * 32    每个汉字 32 字节（16x16 位图）
  共 3755 字 = 120160 字节。

字库数据来源（按优先级）：
  1. tools/HZK16.bin：开源 GB2312 全量 16x16 字库，直接截取一级区（区 0xB0 起连续 3755 字）
  2. PIL + 系统字体渲染（Windows 自带宋体/雅黑）
  3. 均不可用：生成全 0 占位并警告（固件可编译，中文显示为方块）

烧录字库分区：
  python tools/gen_font.py
  idf.py -p COMx write_partition --partition-name fonts --partition-offset 0xC13000 --input build/fonts.bin
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # firmware/
FIRMWARE = ROOT
MAIN = os.path.join(FIRMWARE, "main")
TABLE_C = os.path.join(MAIN, "common", "font_zh_table.c")
FONTS_BIN = os.path.join(FIRMWARE, "build", "fonts.bin")
HZK16 = os.path.join(ROOT, "tools", "HZK16.bin")

LEVEL1_COUNT = 3755          # GB2312 一级汉字个数
GLYPH = 32                   # 16x16 每字字节数
FONTS_BIN_SIZE = LEVEL1_COUNT * GLYPH  # 120160


def build_table():
    """生成 Unicode -> GB2312 映射表（GB2312 一级汉字区 16~55）。"""
    entries = []
    for q in range(16, 56):                 # 区 16~55（0xB0~0xD7）
        for w in range(1, 95):              # 位 1~94（0xA1~0xFE）
            gb = ((0xA0 + q) << 8) | (0xA0 + w)
            try:
                ch = bytes([gb >> 8, gb & 0xFF]).decode("gb2312")
            except UnicodeDecodeError:
                continue                     # 该码位无定义（一级区尾部空位）
            entries.append((ord(ch), gb))
    assert len(entries) == LEVEL1_COUNT, f"expected {LEVEL1_COUNT}, got {len(entries)}"
    entries.sort(key=lambda e: e[0])         # 按 unicode 升序，供二分查找

    with open(TABLE_C, "w", encoding="utf-8") as f:
        f.write("#include \"font_zh_table.h\"\n\n")
        f.write("/* 自动生成，勿手改。来源：tools/gen_font.py（GB2312 一级汉字 3755 字） */\n")
        f.write("const utf8_gb_entry_t g_utf8_gb_table[] = {\n")
        for u, gb in entries:
            f.write(f"    {{0x{u:04X},0x{gb:04X}}},\n")
        f.write("};\n")
        f.write("const unsigned g_utf8_gb_count = sizeof(g_utf8_gb_table) / sizeof(g_utf8_gb_table[0]);\n")
    print(f"[table] {TABLE_C} ({len(entries)} entries)")


def gen_ascii_8x16(font_path, size_px):
    """用 PIL 渲染生成 ASCII 0x20~0x7E 的 8x16 位图（备用，暂未写入 fonts.bin）。"""
    return []


def render_glyph(font_path, ch, size_px):
    """渲染单个汉字为 16x16 位图（行序，每行 2 字节高字节在前）。"""
    from PIL import Image, ImageDraw, ImageFont

    img = Image.new("1", (size_px, size_px), 0)
    draw = ImageDraw.Draw(img)
    font = ImageFont.truetype(font_path, size_px)
    bbox = draw.textbbox((0, 0), ch, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    x = (size_px - w) // 2 - bbox[0]
    y = (size_px - h) // 2 - bbox[1]
    draw.text((x, y), ch, font=font, fill=1)
    out = bytearray()
    for gy in range(size_px):
        row = 0
        for gx in range(size_px):
            row = (row << 1) | (1 if img.getpixel((gx, gy)) else 0)
        out += row.to_bytes(2, "big")
    return bytes(out)


def find_system_font():
    candidates = [
        r"C:\Windows\Fonts\simsun.ttc",
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    return None


def build_fonts_bin():
    os.makedirs(os.path.dirname(FONTS_BIN), exist_ok=True)

    # 1) 优先：开源 HZK16.bin 直接截取一级区
    if os.path.exists(HZK16):
        with open(HZK16, "rb") as f:
            data = f.read(FONTS_BIN_SIZE)
        if len(data) >= FONTS_BIN_SIZE:
            with open(FONTS_BIN, "wb") as f:
                f.write(data[:FONTS_BIN_SIZE])
            print(f"[fonts] {FONTS_BIN} (from HZK16.bin, {FONTS_BIN_SIZE} bytes)")
            return

    # 2) 其次：PIL + 系统字体渲染
    font_path = find_system_font()
    if font_path:
        try:
            from PIL import Image, ImageDraw, ImageFont  # noqa: F401
        except ImportError:
            font_path = None
    if font_path:
        buf = bytearray()
        for q in range(16, 56):
            for w in range(1, 95):
                gb = ((0xA0 + q) << 8) | (0xA0 + w)
                try:
                    ch = bytes([gb >> 8, gb & 0xFF]).decode("gb2312")
                except UnicodeDecodeError:
                    continue
                buf += render_glyph(font_path, ch, 16)
        with open(FONTS_BIN, "wb") as f:
            f.write(buf)
        print(f"[fonts] {FONTS_BIN} (rendered from {font_path}, {len(buf)} bytes)")
        return

    # 3) 兜底：全 0 占位 + 警告
    with open(FONTS_BIN, "wb") as f:
        f.write(b"\x00" * FONTS_BIN_SIZE)
    print(f"[fonts] {FONTS_BIN} (PLACEHOLDER all-zero!)")
    print("WARN: 未找到 HZK16.bin，也未找到 PIL/系统字体，字库为空占位。")
    print("      请放置 tools/HZK16.bin，或安装 Pillow 后重跑本脚本以生成真实字库。")


if __name__ == "__main__":
    build_table()
    build_fonts_bin()
    print("完成。烧录字库分区：")
    print("  idf.py -p COMx write_partition --partition-name fonts "
          "--partition-offset 0xC13000 --input build/fonts.bin")
