#!/usr/bin/env python3
"""gen_qrcode.py — 预生成管理页二维码位图（纯标准库，无第三方依赖）。

内容固定为 http://stockwatcher.local，版本 3 / ECC_MEDIUM。
把二维码模块位图输出为 firmware/main/display/qr_admin.h（const 数组），
固件运行时只读取该数组绘制像素，不再内置二维码编码库。

算法为 ricmoo/QRCode（MIT, https://github.com/ricmoo/QRCode）的 Python 移植
（LOCK_VERSION=3 裁剪版），仅在构建期运行一次。

用法：
    python tools/gen_qrcode.py
"""

import os
import sys

# ---------------- 参数 ----------------
CONTENT = b"http://stockwatcher.local"
VERSION = 3        # 29x29
ECC = 1            # ECC_MEDIUM
QUIET = 4          # 静区模块数（标准 4）

# ---------------- LOCK_VERSION=3 常量表 ----------------
# 索引 = eccFormatBits（0=Medium, 1=Low, 2=High, 3=Quartile）
ECC_CODEWORDS = (26, 15, 44, 36)
ECC_BLOCKS = (1, 1, 2, 2)
RAW_DATA_MODULES = 567
ECC_FORMAT_BITS = (0x02 << 6) | (0x03 << 4) | (0x00 << 2) | (0x01 << 0)

MODE_NUMERIC = 0
MODE_ALPHANUMERIC = 1
MODE_BYTE = 2


def get_mode_bits(mode):
    mode_info = 0x7bbb80a
    # LOCK_VERSION=3（version 3）：无移位
    result = 8 + ((mode_info >> (3 * mode)) & 0x07)
    return 16 if result == 15 else result


class BitBuffer(object):
    def __init__(self):
        self.bit_len = 0
        self.data = bytearray()

    def append(self, val, length):
        for i in range(length - 1, -1, -1):
            if (val >> i) & 1:
                idx = self.bit_len >> 3
                while idx >= len(self.data):
                    self.data.append(0)
                self.data[idx] |= 1 << (7 - (self.bit_len & 7))
            self.bit_len += 1


class Grid(object):
    def __init__(self, size):
        self.size = size
        self.data = bytearray((size * size + 7) // 8)

    def set(self, x, y, on):
        offset = y * self.size + x
        mask = 1 << (7 - (offset & 7))
        if on:
            self.data[offset >> 3] |= mask
        else:
            self.data[offset >> 3] &= ~mask

    def get(self, x, y):
        offset = y * self.size + x
        return bool(self.data[offset >> 3] & (1 << (7 - (offset & 7))))

    def invert(self, x, y, invert):
        on = self.get(x, y)
        self.set(x, y, not on if invert else on)


def get_alphanumeric(c):
    # c 为字节值（int）
    if 48 <= c <= 57:      # '0'-'9'
        return c - 48
    if 65 <= c <= 90:      # 'A'-'Z'
        return c - 65 + 10
    return {32: 36, 36: 37, 37: 38, 42: 39, 43: 40,
            45: 41, 46: 42, 47: 43, 58: 44}.get(c, -1)


def is_numeric(text):
    return all(48 <= c <= 57 for c in text)


def is_alphanumeric(text):
    return all(get_alphanumeric(c) != -1 for c in text)


def encode_data_codewords(bits, text):
    length = len(text)
    if is_numeric(text):
        bits.append(1 << MODE_NUMERIC, 4)
        bits.append(length, get_mode_bits(MODE_NUMERIC))
        accum = 0
        count = 0
        for c in text:
            accum = accum * 10 + (c - 48)
            count += 1
            if count == 3:
                bits.append(accum, 10)
                accum = 0
                count = 0
        if count > 0:
            bits.append(accum, count * 3 + 1)
    elif is_alphanumeric(text):
        bits.append(1 << MODE_ALPHANUMERIC, 4)
        bits.append(length, get_mode_bits(MODE_ALPHANUMERIC))
        accum = 0
        count = 0
        for c in text:
            accum = accum * 45 + get_alphanumeric(c)
            count += 1
            if count == 2:
                bits.append(accum, 11)
                accum = 0
                count = 0
        if count > 0:
            bits.append(accum, 6)
    else:
        bits.append(1 << MODE_BYTE, 4)
        bits.append(length, get_mode_bits(MODE_BYTE))
        for b in text:
            bits.append(b, 8)


def rs_multiply(x, y):
    z = 0
    for i in range(7, -1, -1):
        z = (z << 1) ^ ((z >> 7) * 0x11D)
        z ^= ((y >> i) & 1) * x
    return z


def rs_init(degree):
    coeff = [0] * degree
    coeff[degree - 1] = 1
    root = 1
    for _ in range(degree):
        for j in range(degree):
            coeff[j] = rs_multiply(coeff[j], root)
            if j + 1 < degree:
                coeff[j] ^= coeff[j + 1]
        root = (root << 1) ^ ((root >> 7) * 0x11D)
    return coeff


def rs_get_remainder(degree, coeff, data, length, result, stride, base):
    for i in range(length):
        factor = data[i] ^ result[base]
        for j in range(1, degree):
            result[base + (j - 1) * stride] = result[base + j * stride]
        result[base + (degree - 1) * stride] = 0
        for j in range(degree):
            result[base + j * stride] ^= rs_multiply(coeff[j], factor)


def perform_error_correction(ecc_fmt, data):
    num_blocks = ECC_BLOCKS[ecc_fmt]
    total_ecc = ECC_CODEWORDS[ecc_fmt]
    module_count = RAW_DATA_MODULES
    block_ecc_len = total_ecc // num_blocks
    num_short_blocks = num_blocks - (module_count // 8) % num_blocks
    short_block_len = module_count // 8 // num_blocks
    short_data_len = short_block_len - block_ecc_len

    capacity = (module_count + 7) // 8
    result = bytearray(capacity)
    data_bytes = data.data

    # 交错所有短块（LOCK_VERSION=3：无长块）
    offset = 0
    for i in range(short_data_len):
        index = i
        stride = short_data_len
        for _ in range(num_blocks):
            result[offset] = data_bytes[index]
            offset += 1
            index += stride

    # 各块 ECC，交错写入
    coeff = rs_init(block_ecc_len)
    block_size = short_data_len
    for b in range(num_blocks):
        rs_get_remainder(block_ecc_len, coeff, data_bytes, block_size,
                         result, num_blocks, offset + b)
        data_bytes = data_bytes[block_size:]

    data.data = result
    data.bit_len = module_count


def set_function_module(modules, is_function, x, y, on):
    modules.set(x, y, on)
    is_function.set(x, y, True)


def draw_finder_pattern(modules, is_function, x, y):
    size = modules.size
    for i in range(-4, 5):
        for j in range(-4, 5):
            dist = max(abs(i), abs(j))
            xx, yy = x + j, y + i
            if 0 <= xx < size and 0 <= yy < size:
                set_function_module(modules, is_function, xx, yy, dist != 2 and dist != 4)


def draw_alignment_pattern(modules, is_function, x, y):
    for i in range(-2, 3):
        for j in range(-2, 3):
            set_function_module(modules, is_function, x + j, y + i,
                                max(abs(i), abs(j)) != 1)


def draw_format_bits(modules, is_function, ecc, mask):
    size = modules.size
    data = (ecc << 3) | mask
    rem = data
    for _ in range(10):
        rem = (rem << 1) ^ ((rem >> 9) * 0x537)
    data = ((data << 10) | rem) ^ 0x5412

    for i in range(6):
        set_function_module(modules, is_function, 8, i, (data >> i) & 1)
    set_function_module(modules, is_function, 8, 7, (data >> 6) & 1)
    set_function_module(modules, is_function, 8, 8, (data >> 7) & 1)
    set_function_module(modules, is_function, 7, 8, (data >> 8) & 1)
    for i in range(9, 15):
        set_function_module(modules, is_function, 14 - i, 8, (data >> i) & 1)

    for i in range(8):
        set_function_module(modules, is_function, size - 1 - i, 8, (data >> i) & 1)
    for i in range(8, 15):
        set_function_module(modules, is_function, 8, size - 15 + i, (data >> i) & 1)
    set_function_module(modules, is_function, 8, size - 8, True)


def draw_version(modules, is_function, version):
    # LOCK_VERSION=3 (<7)：不画版本信息
    return


def draw_function_patterns(modules, is_function, version, ecc_fmt):
    size = modules.size
    for i in range(size):
        set_function_module(modules, is_function, 6, i, i % 2 == 0)
        set_function_module(modules, is_function, i, 6, i % 2 == 0)

    draw_finder_pattern(modules, is_function, 3, 3)
    draw_finder_pattern(modules, is_function, size - 4, 3)
    draw_finder_pattern(modules, is_function, 3, size - 4)

    # LOCK_VERSION=3：version>1 → 校准图形
    align_count = version // 7 + 2
    step = 26 if version == 32 else (version * 4 + align_count * 2 + 1) // (2 * align_count - 2) * 2
    align_pos = [0] * align_count
    align_pos[0] = 6
    idx = align_count - 1
    pos = version * 4 + 17 - 7
    for _ in range(align_count - 1):
        align_pos[idx] = pos
        idx -= 1
        pos -= step
    for i in range(align_count):
        for j in range(align_count):
            if (i == 0 and j == 0) or (i == 0 and j == align_count - 1) or (i == align_count - 1 and j == 0):
                continue
            draw_alignment_pattern(modules, is_function, align_pos[i], align_pos[j])

    draw_format_bits(modules, is_function, ecc_fmt, 0)
    draw_version(modules, is_function, version)


def draw_codewords(modules, is_function, codewords):
    bit_length = codewords.bit_len
    data = codewords.data
    size = modules.size
    i = 0
    right = size - 1
    while right >= 1:
        if right == 6:
            right = 5
        for vert in range(size):
            for j in range(2):
                x = right - j
                upwards = ((right & 2) == 0) ^ (x < 6)
                y = size - 1 - vert if upwards else vert
                if not is_function.get(x, y) and i < bit_length:
                    modules.set(x, y, bool((data[i >> 3] >> (7 - (i & 7))) & 1))
                    i += 1
        right -= 2


def apply_mask(modules, is_function, mask):
    size = modules.size
    for y in range(size):
        for x in range(size):
            if is_function.get(x, y):
                continue
            invert = False
            if mask == 0:
                invert = (x + y) % 2 == 0
            elif mask == 1:
                invert = y % 2 == 0
            elif mask == 2:
                invert = x % 3 == 0
            elif mask == 3:
                invert = (x + y) % 3 == 0
            elif mask == 4:
                invert = (x // 3 + y // 2) % 2 == 0
            elif mask == 5:
                invert = x * y % 2 + x * y % 3 == 0
            elif mask == 6:
                invert = (x * y % 2 + x * y % 3) % 2 == 0
            elif mask == 7:
                invert = ((x + y) % 2 + x * y % 3) % 2 == 0
            modules.invert(x, y, invert)


def get_penalty_score(modules):
    result = 0
    size = modules.size

    for y in range(size):
        color_x = modules.get(0, y)
        run_x = 1
        for x in range(1, size):
            cx = modules.get(x, y)
            if cx != color_x:
                color_x = cx
                run_x = 1
            else:
                run_x += 1
                if run_x == 5:
                    result += 3
                elif run_x > 5:
                    result += 1

    for x in range(size):
        color_y = modules.get(x, 0)
        run_y = 1
        for y in range(1, size):
            cy = modules.get(x, y)
            if cy != color_y:
                color_y = cy
                run_y = 1
            else:
                run_y += 1
                if run_y == 5:
                    result += 3
                elif run_y > 5:
                    result += 1

    black = 0
    for y in range(size):
        bits_row = 0
        bits_col = 0
        for x in range(size):
            color = modules.get(x, y)
            if x > 0 and y > 0:
                ul = modules.get(x - 1, y - 1)
                ur = modules.get(x, y - 1)
                ll = modules.get(x - 1, y)
                if color == ul and color == ur and color == ll:
                    result += 3
            bits_row = ((bits_row << 1) & 0x7FF) | (1 if color else 0)
            bits_col = ((bits_col << 1) & 0x7FF) | (1 if modules.get(y, x) else 0)
            if x >= 10:
                if bits_row == 0x05D or bits_row == 0x5D0:
                    result += 40
                if bits_col == 0x05D or bits_col == 0x5D0:
                    result += 40
            if color:
                black += 1

    total = size * size
    k = 0
    while black * 20 < (9 - k) * total or black * 20 > (11 + k) * total:
        result += 10
        k += 1
    return result


def build_base(text, version, ecc):
    """返回 (modules, is_function)：完成数据/功能图案绘制、尚未选掩码的网格。"""
    size = version * 4 + 17
    ecc_fmt = (ECC_FORMAT_BITS >> (2 * ecc)) & 0x03
    data_capacity = RAW_DATA_MODULES // 8 - ECC_CODEWORDS[ecc_fmt]

    codewords = BitBuffer()
    encode_data_codewords(codewords, text)
    padding = (data_capacity * 8) - codewords.bit_len
    if padding > 4:
        padding = 4
    codewords.append(0, padding)
    codewords.append(0, (8 - codewords.bit_len % 8) % 8)
    pad = 0xEC
    while codewords.bit_len < data_capacity * 8:
        codewords.append(pad, 8)
        pad ^= 0xEC ^ 0x11

    perform_error_correction(ecc_fmt, codewords)

    modules = Grid(size)
    is_function = Grid(size)
    draw_function_patterns(modules, is_function, version, ecc_fmt)
    draw_codewords(modules, is_function, codewords)
    return modules, is_function, ecc_fmt


def finalize(modules, is_function, ecc_fmt, mask):
    """对基础网格施加指定掩码与格式信息，返回最终模块网格。"""
    m = Grid(modules.size)
    m.data = bytearray(modules.data)
    f = Grid(is_function.size)
    f.data = bytearray(is_function.data)
    draw_format_bits(m, f, ecc_fmt, mask)
    apply_mask(m, f, mask)
    return m


def make_qrcode(text, version, ecc):
    """生成完整二维码，返回 (最终网格, 选中掩码)。"""
    modules, is_function, ecc_fmt = build_base(text, version, ecc)
    best = 0
    best_pen = None
    for i in range(8):
        m = finalize(modules, is_function, ecc_fmt, i)
        pen = get_penalty_score(m)
        if best_pen is None or pen < best_pen:
            best_pen = pen
            best = i
    return finalize(modules, is_function, ecc_fmt, best), best


def grid_to_bytes(grid):
    """模块网格 → 按位打包字节数组（每字节高位在前）。"""
    return bytes(grid.data)


def emit_c_file(path, grid, mask):
    size = grid.size
    data = grid_to_bytes(grid)
    lines = []
    lines.append("/* 自动生成：tools/gen_qrcode.py — 管理页二维码，请勿手改。")
    lines.append(" * 内容: %s" % CONTENT.decode())
    lines.append(" * 版本 %d / ECC_MEDIUM / mask=%d / 模块位图（1=黑，每字节高位在前）" % (VERSION, mask))
    lines.append(" */")
    lines.append("#ifndef QR_ADMIN_H")
    lines.append("#define QR_ADMIN_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#define QR_ADMIN_SIZE  %d  /* 模块数（不含静区） */" % size)
    lines.append("#define QR_ADMIN_QUIET %d  /* 静区模块数 */" % QUIET)
    lines.append("")
    lines.append("const uint8_t qr_admin_modules[] = {")
    per = 12
    for i in range(0, len(data), per):
        chunk = data[i:i + per]
        lines.append("    " + " ".join("0x%02X," % b for b in chunk))
    lines.append("};")
    lines.append("")
    lines.append("#endif /* QR_ADMIN_H */")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %s (%d bytes, %dx%d, mask=%d)" % (path, len(data), size, size, mask))


def main():
    grid, mask = make_qrcode(CONTENT, VERSION, ECC)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       os.pardir, "main", "display", "qr_admin.h")
    emit_c_file(os.path.normpath(out), grid, mask)


if __name__ == "__main__":
    sys.exit(main())
