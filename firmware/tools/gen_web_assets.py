#!/usr/bin/env python3
"""生成 web_assets.c：把前端产物（firmware/web_dist）以字节数组内嵌进固件。

用法: gen_web_assets.py <src_dir> <out_c>
由 main/CMakeLists.txt 在配置阶段调用。
"""
import os
import sys

MIME = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript",
    ".css": "text/css",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".json": "application/json",
    ".woff2": "font/woff2",
}


def mime_of(path: str) -> str:
    return MIME.get(os.path.splitext(path)[1].lower(), "application/octet-stream")


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: gen_web_assets.py <src_dir> <out_c>", file=sys.stderr)
        return 1
    src_dir, out_c = sys.argv[1], sys.argv[2]

    entries = []  # (uri, data, gzip)
    for root, _dirs, files in os.walk(src_dir):
        for name in sorted(files):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, src_dir).replace("\\", "/")
            gzip = rel.endswith(".gz")
            uri_rel = rel[:-3] if gzip else rel
            uri = "/" + uri_rel if uri_rel != "index.html" else "/"
            with open(full, "rb") as f:
                data = f.read()
            entries.append((uri, data, gzip))

    if not entries:
        print("warning: web_dist is empty, run: cd web && npm run build", file=sys.stderr)

    with open(out_c, "w", encoding="utf-8") as f:
        f.write("/* 自动生成，请勿手动修改。由 tools/gen_web_assets.py 生成。 */\n")
        f.write('#include "web_assets.h"\n\n')
        for i, (_uri, data, _gzip) in enumerate(entries):
            f.write("static const unsigned char asset_%d_data[] = {\n    " % i)
            for j, b in enumerate(data):
                f.write("0x%02x, " % b)
                if (j + 1) % 12 == 0:
                    f.write("\n    ")
            f.write("\n};\n\n")
        f.write("const web_asset_t web_assets[] = {\n")
        for i, (uri, data, gzip) in enumerate(entries):
            f.write('    { "%s", asset_%d_data, %d, "%s", %d },\n'
                    % (uri, i, len(data), mime_of(uri), 1 if gzip else 0))
        f.write("};\n\n")
        f.write("const uint32_t web_assets_count = %d;\n" % len(entries))

    print("generated %s (%d assets)" % (out_c, len(entries)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
