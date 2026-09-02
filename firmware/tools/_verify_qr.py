import sys

sys.path.insert(0, r"d:\code\Stock-Watcher-AI\firmware\tools")
sys.path.insert(0, r"d:\code\Stock-Watcher-AI\.verify_py")

import segno
from gen_qrcode import (CONTENT, VERSION, build_base, finalize,
                        rs_init, rs_get_remainder)

content = CONTENT.decode()
modules, is_function, ecc_fmt = build_base(CONTENT, VERSION, 1)
size = VERSION * 4 + 17


def extract_data(grid, is_func, mask):
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
                # 反掩码
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
                bit = raw ^ invert
                push(bit)
        right -= 2
    return bytes(out)


def decode_payload(cw):
    bits = ''.join(format(b, '08b') for b in cw[:44])
    mode = int(bits[0:4], 2)
    if mode != 0b0100:
        return None, "bad mode %d" % mode
    count = int(bits[4:12], 2)
    data = bits[12:12 + count * 8]
    return bytes(int(data[i * 8:(i + 1) * 8], 2) for i in range(count)), None


ok = True
for mask in range(8):
    mine = finalize(modules, is_function, ecc_fmt, mask)
    cw = extract_data(mine, is_function, mask)

    # 1) RS 一致性：由数据字节重算 ECC，应与提取的 ECC 完全一致
    ecc = bytearray(26)
    rs_get_remainder(26, rs_init(26), cw[:44], 44, ecc, 1, 0)
    rs_ok = bytes(ecc) == cw[44:70]

    # 2) 载荷解析 == 内容
    payload, err = decode_payload(cw)
    payload_ok = (err is None) and (payload == CONTENT)

    # 3) 与 segno 对比：功能模块应 0 差异；数据差异应仅出现在 padding 区（27 字节之后）
    qr = segno.make_qr(content, error="m", version=VERSION, mask=mask, boost_error=False)
    seg = qr.matrix
    func_mism = 0
    data_prefix_mism = 0  # 前 27 个数据字节内的差异
    for y in range(size):
        for x in range(size):
            a = mine.get(x, y)
            b = bool(seg[y][x])
            if a != b and is_function.get(x, y):
                func_mism += 1

    # 前 27 数据字节（内容区）与 segno 对比
    seg_grid_cw = []
    seg_mine_ok = True
    # 直接复用 extract：把 segno 矩阵塞进 Grid
    from gen_qrcode import Grid
    seg_grid = Grid(size)
    for y in range(size):
        for x in range(size):
            seg_grid.set(x, y, bool(seg[y][x]))
    seg_cw = extract_data(seg_grid, is_function, mask)
    prefix_ok = cw[:27] == seg_cw[:27]

    status = "OK" if (rs_ok and payload_ok and func_mism == 0 and prefix_ok) else "FAIL"
    print("mask %d: rs=%s payload=%s func_mism=%d prefix27=%s  -> %s"
          % (mask, rs_ok, payload_ok, func_mism, prefix_ok, status))
    if status != "OK":
        ok = False
        print("  cw len", len(cw), " payload err", err, "rs_ok", rs_ok,
              " func_mism", func_mism, " prefix", prefix_ok)

print("VERIFY:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
