#!/usr/bin/env python3
"""Convert a binary PPM (P6) written by tests/preview.c into a PNG."""

import struct
import sys
import zlib


def write_png(path, width, height, rgb):
    def chunk(tag, data):
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw = b"".join(b"\x00" + rgb[y * width * 3 : (y + 1) * width * 3] for y in range(height))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def read_ppm(path):
    with open(path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6":
            raise SystemExit("not a P6 ppm")
        line = f.readline()
        while line.startswith(b"#"):
            line = f.readline()
        w, h = map(int, line.split())
        maxv = int(f.readline())
        if maxv != 255:
            raise SystemExit("unexpected maxval")
        rgb = f.read()
    if len(rgb) != w * h * 3:
        raise SystemExit(f"short ppm {len(rgb)}")
    return w, h, rgb


def main():
    src, dst = sys.argv[1], sys.argv[2]
    w, h, rgb = read_ppm(src)
    write_png(dst, w, h, rgb)


if __name__ == "__main__":
    main()
