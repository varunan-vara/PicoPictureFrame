"""Desktop stand-in for the Pico's screen, for trying out app code with pygame.

It mirrors the C canvas API (canvas.h / display.h) so drawing code ports over with
little change: draw into a small RGB565 canvas, then show() it on a window that is
scaled up and letterboxed to keep the screen's 4:3 shape.
"""

import re
from pathlib import Path

import pygame

# The Waveshare 2.8" panel in landscape (LCD_WIDTH / LCD_HEIGHT in display.h)
LCD_WIDTH = 320
LCD_HEIGHT = 240

# Window size the simulator opens at, roughly a real 2.8" screen on a Mac
DEFAULT_WINDOW = (502, 376)

FONT_SOURCE = Path(__file__).resolve().parent.parent / "MenuFirmware/src/display/font/font5x7.c"
FONT_W, FONT_H = 6, 8


def rgb565(r, g, b):
    """Same packing as the RGB565() macro in canvas.h."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def _expand(color):
    """RGB565 back to 8 bit RGB, so the window shows the colours the panel can."""
    r, g, b = (color >> 11) & 0x1F, (color >> 5) & 0x3F, color & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def _load_font5x7():
    """Reads the glyph table out of the firmware's font5x7.c so there is one copy of it."""
    src = FONT_SOURCE.read_text()
    body = src[src.index("Font5x7_Table[] = {"):src.index("};")]
    data = bytes(int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", body))
    assert len(data) == 95 * FONT_H, "font5x7.c is not in the expected format"
    return data


class Display:
    """A canvas plus the window it is shown in.

    width is 320 (full res) or 160 (half res, drawn at 160x120 and doubled by the
    real display_blit()). Colours are RGB565 ints, as on the Pico.
    """

    def __init__(self, width=160, title="Pico simulator"):
        if width not in (LCD_WIDTH, LCD_WIDTH // 2):
            raise ValueError("canvas width must be 320 or 160, like display_blit()")
        self.w = width
        self.h = width * LCD_HEIGHT // LCD_WIDTH

        pygame.init()
        self.canvas = pygame.Surface((self.w, self.h))
        self.screen = pygame.display.set_mode(DEFAULT_WINDOW, pygame.RESIZABLE)
        pygame.display.set_caption(title)
        self.clock = pygame.time.Clock()
        self._font = _load_font5x7()

    # Drawing, same names and argument order as canvas.h ------------------------------

    def fill_rect(self, x, y, w, h, color):
        self.canvas.fill(_expand(color), (x, y, w, h))

    def rect_outline(self, x, y, w, h, color):
        c = _expand(color)
        self.canvas.fill(c, (x, y, w, 1))
        self.canvas.fill(c, (x, y + h - 1, w, 1))
        self.canvas.fill(c, (x, y, 1, h))
        self.canvas.fill(c, (x + w - 1, y, 1, h))

    def text(self, x, y, s, scale, fg, bg=None):
        """Font5x7 text. bg=None leaves the pixels under the text alone (CANVAS_NO_BG)."""
        fg_rgb = _expand(fg)
        bg_rgb = None if bg is None else _expand(bg)
        for ch in s:
            if " " <= ch <= "~":
                glyph = self._font[(ord(ch) - 32) * FONT_H:][:FONT_H]
                for row, bits in enumerate(glyph):
                    for col in range(FONT_W):
                        rect = (x + col * scale, y + row * scale, scale, scale)
                        if bits & (0x80 >> col):
                            self.canvas.fill(fg_rgb, rect)
                        elif bg_rgb is not None:
                            self.canvas.fill(bg_rgb, rect)
            x += FONT_W * scale

    # Output --------------------------------------------------------------------------

    def poll(self):
        """Handles window events. Returns False once the window has been closed."""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
        return True

    def show(self, fps=30):
        """The display_blit() equivalent: scales the canvas into the window, with black
        bars if the window isn't 4:3, then waits to hold the frame rate."""
        win_w, win_h = self.screen.get_size()
        scale = min(win_w / self.w, win_h / self.h)
        out_w, out_h = max(1, int(self.w * scale)), max(1, int(self.h * scale))

        self.screen.fill((0, 0, 0))
        frame = pygame.transform.scale(self.canvas, (out_w, out_h))
        self.screen.blit(frame, ((win_w - out_w) // 2, (win_h - out_h) // 2))
        pygame.display.flip()
        self.clock.tick(fps)

    def close(self):
        pygame.quit()
