#!/usr/bin/env python3
"""Build the assets for every app so it is ready for the simulator or the SD card.

Usage: build_assets.py [--app NAME] [--sd SD_DIR] [--little-endian]

For each apps/<name>/art folder, converts the PNGs to apps/<name>/assets (see
convert_png_to_bin.py). The simulator reads apps/<name>/assets directly.

With --sd, the built assets are also copied to SD_DIR/<name>/, so SD_DIR is the folder
on the card that holds the games, e.g. --sd /Volumes/SDCARD/games.

Requires Pillow: pip install --group tools
"""
import argparse
import shutil
import sys
from pathlib import Path

from convert_png_to_bin import convert_folder, default_out_dir

APPS_DIR = Path(__file__).resolve().parent.parent / "apps"


def build_app(app_dir: Path, big_endian: bool, sd_dir: Path | None) -> None:
    """Converts the app's art to assets, and copies the assets to the SD card if given"""
    art_dir = app_dir / "art"
    assets_dir = default_out_dir(art_dir)

    print(f"[{app_dir.name}]")
    convert_folder(art_dir, assets_dir, big_endian)

    if sd_dir is not None:
        dest = sd_dir / app_dir.name
        shutil.copytree(assets_dir, dest, dirs_exist_ok=True)
        print(f"copied {assets_dir} -> {dest}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--app", help="only build this app (a folder name under apps/)")
    parser.add_argument("--sd", type=Path, help="also copy the built assets to SD_DIR/<app>/")
    parser.add_argument("--little-endian", action="store_true",
                        help="store pixels little-endian (default: big-endian)")
    args = parser.parse_args()

    if args.sd is not None and not args.sd.is_dir():
        sys.exit(f"error: {args.sd} is not a folder (SD Card may not be mounted)")

    app_dirs = [APPS_DIR / args.app] if args.app else sorted(APPS_DIR.iterdir())
    app_dirs = [d for d in app_dirs if (d / "art").is_dir()]
    if not app_dirs:
        sys.exit(f"error: no apps with an art/ folder in {APPS_DIR}")

    for app_dir in app_dirs:
        build_app(app_dir, not args.little_endian, args.sd)


if __name__ == "__main__":
    main()
