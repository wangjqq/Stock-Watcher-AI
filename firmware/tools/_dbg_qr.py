import sys

sys.path.insert(0, r"d:\code\Stock-Watcher-AI\firmware\tools")
sys.path.insert(0, r"d:\code\Stock-Watcher-AI\.verify_py")

import segno
from gen_qrcode import CONTENT, VERSION, build_base, finalize, Grid

content = CONTENT.decode()
modules, is_function, ecc_fmt = build_base(CONTENT, VERSION, 1)
size = VERSION * 4 + 17
mine = finalize(modules, is_function, ecc_fmt, 0)
qr = segno.make_qr(content, error="m", version=VERSION, mask=0, boost_error=False)
seg = qr.matrix

seg_grid = Grid(size)
for y in range(size):
    for x in range(size):
        seg_grid.set(x, y, bool(seg[y][x]))

# 区域分类统计
func_mism = []
data_mism = []
for y in range(size):
    for x in range(size):
        a = mine.get(x, y)
        b = seg_grid.get(x, y)
        if a != b:
            if is_function.get(x, y):
                func_mism.append((x, y))
            else:
                data_mism.append((x, y))

print("mismatches total:", len(func_mism) + len(data_mism),
      " func:", len(func_mism), " data:", len(data_mism))
print("func mismatches:", func_mism[:20])


def zigzag_seq(grid, mask):
    """按 zigzag 顺序提取模块序列（不反向掩码）。"""
    size = grid.size
    out = []
    right = size - 1
    while right >= 1:
        if right == 6:
            right = 5
        for vert in range(size):
            for j in range(2):
                x = right - j
                upwards = ((right & 2) == 0) ^ (x < 6)
                y = size - 1 - vert if upwards else vert
                out.append(1 if grid.get(x, y) else 0)
        right -= 2
    return out


# 比较：mine 已施加 mask 0；seg 也是 mask 0，数据位应对 mask 公式一致
seq_mine = zigzag_seq(mine, 0)
seq_seg = zigzag_seq(seg_grid, 0)
diff_pos = [i for i in range(len(seq_mine)) if seq_mine[i] != seq_seg[i]]
print("zigzag seq len:", len(seq_mine), " diffs:", len(diff_pos))
print("first diff indices:", diff_pos[:20])


def extract_data(grid, is_func):
    """反向 zigzag，跳过功能模块，按 mask 0 反掩码，提取数据码字字节。"""
    size = grid.size
    out = []
    acc = 0
    bl = 0

    def push(bit):
        nonlocal acc, bl
        acc = (acc << 1) | bit
        bl += 1
        if bl == 8:
            out.append(acc)
            acc = 0
            bl = 0

    right = size - 1
    while right >= 1:
        if right == 6:
            right = 5
        for vert in range(size):
            for j in range(2):
                x = right - j
                upwards = ((right & 2) == 0) ^ (x < 6)
                y = size - 1 - vert if upwards else vert
                if is_func.get(x, y):
                    continue
                raw = grid.get(x, y)
                bit = raw ^ ((x + y) % 2 == 0)  # mask 0 反掩码
                push(bit)
        right -= 2
    return bytes(out)


a = extract_data(mine, is_function)
b = extract_data(seg_grid, is_function)
print("data bytes len: mine", len(a), " seg", len(b))
print("mine first 46:", " ".join("%02X" % v for v in a[:46]))
print("seg  first 46:", " ".join("%02X" % v for v in b[:46]))
diffs = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
print("byte diff indices:", diffs[:30])

