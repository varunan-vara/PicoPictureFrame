# simulator

Runs app drawing code on your computer with pygame, so you can try it without flashing the Pico.

`picodisplay.py` mocks the firmware's canvas and display:

- `Display(width)` is a canvas of 160x120 (half res) or 320x240 (full res), the same two sizes `display_blit()` accepts
- `fill_rect`, `rect_outline` and `text` take the same arguments as the functions in `canvas.h`
- Colours are RGB565 ints made with `rgb565(r, g, b)`, and are shown with the precision the panel has
- `text` uses Font5x7, read straight from `MenuFirmware/src/display/font/font5x7.c`
- `show()` is the stand-in for `display_blit()`. It scales the canvas to the window, and adds black bars when the window isn't 4:3

The window opens at 502x376 (about a 2.8" screen) and can be resized.

## Running

```
pip install -r requirements.txt
python3 example_app.py
```

Write an app the way `example_app.py` does: build a `Display`, draw each frame, call `show()`, and loop while `poll()` returns `True`.
