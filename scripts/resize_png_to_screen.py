#!/usr/bin/env python3
"""Crop and scale a PNG of any size to the half resolution screen (160x120).

Usage: resize_png_to_screen.py <input.png> [-o OUT.png] [--size WxH] [--nearest] [--auto]

Opens the image with a crop rectangle that keeps the screen's aspect ratio:
    drag inside      move the rectangle
    drag a corner    resize it (the opposite corner stays put)
    mouse wheel      grow / shrink it around its centre
    Enter / Esc      save / cancel

The crop is then scaled up or down to --size (default 160x120). Use --nearest for pixel art
so edges stay sharp, and --auto to skip the picker and take the largest centred crop.

Requires Pillow: pip install --group tools
"""
import argparse
import sys
import tkinter as tk
from pathlib import Path

from PIL import Image, ImageTk

DEFAULT_SIZE = (160, 120)
MAX_VIEW = (900, 650)  # largest the picker shows the image, bigger images are shrunk to fit
HANDLE = 6             # half-size of a corner handle, in screen pixels
MIN_BOX_W = 24         # smallest crop width, in screen pixels
WHEEL_STEP = 1.05

Box = tuple[float, float, float, float]


def parse_size(text: str) -> tuple[int, int]:
    """Parses '160x120' into (160, 120)"""
    try:
        width, height = (int(part) for part in text.lower().split("x"))
    except ValueError:
        raise argparse.ArgumentTypeError("size must look like 160x120") from None
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("size must be positive")
    return width, height


def centered_box(img_w: float, img_h: float, ratio: float) -> Box:
    """Largest box with width / height == ratio, centred in the image"""
    width = min(img_w, img_h * ratio)
    height = width / ratio
    x0, y0 = (img_w - width) / 2, (img_h - height) / 2
    return x0, y0, x0 + width, y0 + height


def moved_box(box: Box, dx: float, dy: float, img_w: float, img_h: float) -> Box:
    """Moves the box by (dx, dy), keeping it inside the image"""
    x0, y0, x1, y1 = box
    width, height = x1 - x0, y1 - y0
    new_x = min(max(x0 + dx, 0), img_w - width)
    new_y = min(max(y0 + dy, 0), img_h - height)
    return new_x, new_y, new_x + width, new_y + height


def resized_box(anchor: tuple[float, float], mouse: tuple[float, float], ratio: float,
                img_w: float, img_h: float, min_w: float) -> Box:
    """Box from the fixed anchor corner towards the mouse, keeping the ratio and the image bounds"""
    anchor_x, anchor_y = anchor
    mouse_x, mouse_y = mouse
    sign_x = 1 if mouse_x >= anchor_x else -1
    sign_y = 1 if mouse_y >= anchor_y else -1
    room_w = img_w - anchor_x if sign_x > 0 else anchor_x
    room_h = img_h - anchor_y if sign_y > 0 else anchor_y

    width = max(abs(mouse_x - anchor_x), abs(mouse_y - anchor_y) * ratio)
    width = min(max(width, min_w), room_w, room_h * ratio)
    height = width / ratio

    far_x, far_y = anchor_x + sign_x * width, anchor_y + sign_y * height
    return min(anchor_x, far_x), min(anchor_y, far_y), max(anchor_x, far_x), max(anchor_y, far_y)


def scaled_box(box: Box, factor: float, img_w: float, img_h: float, ratio: float) -> Box:
    """Grows or shrinks the box around its centre, keeping it inside the image"""
    x0, y0, x1, y1 = box
    width = min(max((x1 - x0) * factor, MIN_BOX_W), img_w, img_h * ratio)
    height = width / ratio
    centre_x, centre_y = (x0 + x1) / 2, (y0 + y1) / 2
    return moved_box((centre_x - width / 2, centre_y - height / 2,
                      centre_x + width / 2, centre_y + height / 2), 0, 0, img_w, img_h)


def crop_and_scale(img: Image.Image, box: Box, size: tuple[int, int], nearest: bool) -> Image.Image:
    """Crops the box out of the image and scales it to size"""
    x0, y0, x1, y1 = (round(v) for v in box)
    crop = img.crop((max(x0, 0), max(y0, 0), min(x1, img.width), min(y1, img.height)))
    resample = Image.Resampling.NEAREST if nearest else Image.Resampling.LANCZOS
    return crop.resize(size, resample)


class CropPicker:
    """Window showing the image with a draggable crop rectangle that keeps the screen's ratio"""

    def __init__(self, img: Image.Image, size: tuple[int, int]):
        self.img = img
        self.size = size
        self.ratio = size[0] / size[1]
        self.result: Box | None = None  # crop box in the source image's pixels, None if cancelled

        self.root = tk.Tk()
        self.root.title(f"Pick the area to scale to {size[0]}x{size[1]}")
        self.root.resizable(False, False)

        self.zoom = min(1.0, MAX_VIEW[0] / img.width, MAX_VIEW[1] / img.height)
        self.view_w = max(1, round(img.width * self.zoom))
        self.view_h = max(1, round(img.height * self.zoom))
        backdrop = Image.new("RGBA", img.size, (200, 200, 200, 255))
        backdrop.alpha_composite(img.convert("RGBA"))
        self.photo = ImageTk.PhotoImage(
            backdrop.convert("RGB").resize((self.view_w, self.view_h), Image.Resampling.LANCZOS))

        self.canvas = tk.Canvas(self.root, width=self.view_w, height=self.view_h,
                                highlightthickness=0, cursor="crosshair")
        self.canvas.pack()
        self.canvas.create_image(0, 0, image=self.photo, anchor="nw")
        self.dim = [self.canvas.create_rectangle(0, 0, 0, 0, fill="black", stipple="gray50",
                                                 outline="") for _ in range(4)]
        self.outline = self.canvas.create_rectangle(0, 0, 0, 0, outline="#ff3030", width=2)
        self.handles = [self.canvas.create_rectangle(0, 0, 0, 0, fill="#ff3030", outline="white")
                        for _ in range(4)]

        controls = tk.Frame(self.root)
        controls.pack(fill="x", padx=8, pady=6)
        self.status = tk.Label(controls, anchor="w")
        self.status.pack(side="left", fill="x", expand=True)
        tk.Button(controls, text="Cancel", command=self.cancel).pack(side="right")
        tk.Button(controls, text="Save", command=self.save).pack(side="right", padx=6)

        self.box = centered_box(self.view_w, self.view_h, self.ratio)
        self.drag_mode: str | None = None
        self.anchor = (0.0, 0.0)
        self.last = (0.0, 0.0)

        self.canvas.bind("<ButtonPress-1>", self.on_press)
        self.canvas.bind("<B1-Motion>", self.on_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_release)
        self.canvas.bind("<MouseWheel>", self.on_wheel)
        self.root.bind("<Return>", lambda event: self.save())
        self.root.bind("<Escape>", lambda event: self.cancel())
        self.redraw()

    def corners(self) -> list[tuple[float, float]]:
        """The box's corners, clockwise from the top left"""
        x0, y0, x1, y1 = self.box
        return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]

    def on_press(self, event) -> None:
        """Starts a resize from a corner, or a move from inside the box"""
        corners = self.corners()
        for i, (cx, cy) in enumerate(corners):
            if abs(event.x - cx) <= HANDLE * 2 and abs(event.y - cy) <= HANDLE * 2:
                self.drag_mode = "resize"
                self.anchor = corners[(i + 2) % 4]  # opposite corner stays fixed
                return
        x0, y0, x1, y1 = self.box
        if x0 <= event.x <= x1 and y0 <= event.y <= y1:
            self.drag_mode = "move"
            self.last = (event.x, event.y)

    def on_drag(self, event) -> None:
        """Resizes or moves the box to follow the mouse"""
        if self.drag_mode == "resize":
            self.box = resized_box(self.anchor, (event.x, event.y), self.ratio,
                                   self.view_w, self.view_h, MIN_BOX_W)
        elif self.drag_mode == "move":
            self.box = moved_box(self.box, event.x - self.last[0], event.y - self.last[1],
                                 self.view_w, self.view_h)
            self.last = (event.x, event.y)
        else:
            return
        self.redraw()

    def on_release(self, event) -> None:
        """Ends the current drag"""
        self.drag_mode = None

    def on_wheel(self, event) -> None:
        """Grows the box on scroll up, shrinks it on scroll down"""
        factor = WHEEL_STEP if event.delta > 0 else 1 / WHEEL_STEP
        self.box = scaled_box(self.box, factor, self.view_w, self.view_h, self.ratio)
        self.redraw()

    def source_box(self) -> Box:
        """The crop box in the source image's pixels"""
        return tuple(v / self.zoom for v in self.box)

    def redraw(self) -> None:
        """Moves the rectangle, handles and dimmed strips to the box, and updates the status line"""
        x0, y0, x1, y1 = self.box
        w, h = self.view_w, self.view_h
        strips = [(0, 0, w, y0), (0, y1, w, h), (0, y0, x0, y1), (x1, y0, w, y1)]
        for item, coords in zip(self.dim, strips):
            self.canvas.coords(item, *coords)
        self.canvas.coords(self.outline, x0, y0, x1, y1)
        for item, (cx, cy) in zip(self.handles, self.corners()):
            self.canvas.coords(item, cx - HANDLE, cy - HANDLE, cx + HANDLE, cy + HANDLE)

        crop_w = (x1 - x0) / self.zoom
        factor = self.size[0] / crop_w
        direction = "up" if factor > 1 else "down"
        crop_h = crop_w / self.ratio
        self.status.config(
            text=f"crop {round(crop_w)}x{round(crop_h)} of {self.img.width}x{self.img.height}  ->  "
                 f"{self.size[0]}x{self.size[1]} ({factor:.2f}x {direction}scale)")

    def save(self) -> None:
        """Stores the chosen crop and closes the window"""
        self.result = self.source_box()
        self.root.destroy()

    def cancel(self) -> None:
        """Closes the window without a crop"""
        self.result = None
        self.root.destroy()

    def run(self) -> Box | None:
        """Shows the window until it is closed, returns the chosen crop box or None if cancelled"""
        self.root.mainloop()
        return self.result


def main():
    """Parses the arguments, picks the crop (or centres it with --auto) and saves the scaled png"""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("input", type=Path, help="png to crop and scale")
    parser.add_argument("-o", "--out", type=Path,
                        help="output png (default: <input>_<W>x<H>.png next to the input)")
    parser.add_argument("--size", type=parse_size, default=DEFAULT_SIZE, metavar="WxH",
                        help="output size (default: 160x120, the half resolution screen)")
    parser.add_argument("--nearest", action="store_true",
                        help="nearest-neighbour scaling, for pixel art (default: smooth)")
    parser.add_argument("--auto", action="store_true",
                        help="skip the picker and use the largest centred crop")
    args = parser.parse_args()

    if not args.input.is_file():
        sys.exit(f"error: {args.input} is not a file")
    img = Image.open(args.input)
    img.load()

    if args.auto:
        box = centered_box(img.width, img.height, args.size[0] / args.size[1])
    else:
        box = CropPicker(img, args.size).run()
        if box is None:
            sys.exit("cancelled, nothing saved")

    width, height = args.size
    out_path = args.out or args.input.with_name(f"{args.input.stem}_{width}x{height}.png")
    crop_and_scale(img, box, args.size, args.nearest).save(out_path)
    print(f"{args.input.name} -> {out_path} ({width}x{height})")


if __name__ == "__main__":
    main()
