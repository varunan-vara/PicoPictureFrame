#!/usr/bin/env python3
"""Convert every PNG in a folder to the PPF1 .bin format.

Usage: convert_png_to_bin.py <png_folder> [-o OUT_DIR] [--little-endian]

Requires Pillow: pip install pillow

File layout (header fields little-endian):
    0   4  magic "PPF1"
    4   2  width
    6   2  height
    8   1  format (0 = RGB565, 1 = RGB565 with colour key)
    9   1  frames (always 1 here)
    10  2  colour key (RGB565, ignored for format 0)
    12  .. pixels, row-major from the top-left, 2 bytes each
"""
import argparse
import struct
import sys
from pathlib import Path

from PIL import Image

MAGIC = b"PPF1"
FORMAT_RGB565 = 0
FORMAT_RGB565_KEYED = 1
COLOUR_KEY = 0xF81F  # magenta
ALPHA_THRESHOLD = 128


def rgb565(r: int, g: int, b: int) -> int:
    """Converts rgb value to 16b int"""
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def convert(png_path: str, out_path: str, big_endian: bool) -> list:
    """Given a png bath, uses rgb565(...) to create a binary file of the rgb values"""
    img = Image.open(png_path).convert("RGBA")
    width, height = img.size
    pixel_order = ">H" if big_endian else "<H"

    pixels = bytearray()
    keyed = False
    raw = img.tobytes()
    for r, g, b, a in zip(raw[0::4], raw[1::4], raw[2::4], raw[3::4]):
        if a < ALPHA_THRESHOLD:
            value = COLOUR_KEY
            keyed = True
        else:
            value = rgb565(r, g, b)
            if value == COLOUR_KEY:
                value ^= 1  # nudge so opaque pixels never match the key
        pixels += struct.pack(pixel_order, value)

    fmt = FORMAT_RGB565_KEYED if keyed else FORMAT_RGB565
    header = MAGIC + struct.pack("<HHBBH", width, height, fmt, 1, COLOUR_KEY)
    out_path.write_bytes(header + pixels)
    return width, height, fmt


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", type=Path, help="folder containing .png files")
    parser.add_argument("-o", "--out", type=Path, help="output folder (default: same as input)")
    parser.add_argument("--little-endian", action="store_true",
                        help="store pixels little-endian (default: big-endian, as most SPI displays expect)")
    args = parser.parse_args()

    if not args.folder.is_dir():
        sys.exit(f"error: {args.folder} is not a folder")

    pngs = sorted(args.folder.glob("*.png"))
    if not pngs:
        sys.exit(f"error: no .png files in {args.folder}")

    out_dir = args.out or args.folder
    out_dir.mkdir(parents=True, exist_ok=True)

    for png in pngs:
        out_path = out_dir / (png.stem + ".bin")
        width, height, fmt = convert(png, out_path, not args.little_endian)
        print(f"{png.name} -> {out_path.name} ({width}x{height}, format {fmt})")


if __name__ == "__main__":
    main()
