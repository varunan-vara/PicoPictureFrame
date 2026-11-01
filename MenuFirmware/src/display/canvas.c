#include "canvas.h"

void canvas_fill_rect(canvas_t *c, int x, int y, int w, int h, uint16_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > c->w ? c->w : x + w;
    int y1 = y + h > c->h ? c->h : y + h;

    for (int row = y0; row < y1; row++) {
        uint16_t *p = &c->px[row * c->w];
        for (int col = x0; col < x1; col++) p[col] = color;
    }
}

void canvas_rect_outline(canvas_t *c, int x, int y, int w, int h, uint16_t color) {
    canvas_fill_rect(c, x, y, w, 1, color);
    canvas_fill_rect(c, x, y + h - 1, w, 1, color);
    canvas_fill_rect(c, x, y, 1, h, color);
    canvas_fill_rect(c, x + w - 1, y, 1, h, color);
}

void canvas_text(canvas_t *c, int x, int y, const char *s, const sFONT *font,
                 int scale, uint16_t fg, int32_t bg) {
    int row_bytes = (font->Width + 7) / 8;

    for (; *s; s++, x += font->Width * scale) {
        if (*s < ' ' || *s > '~') continue;
        const uint8_t *glyph = &font->table[(*s - ' ') * font->Height * row_bytes];

        for (int row = 0; row < font->Height; row++) {
            for (int col = 0; col < font->Width; col++) {
                bool on = glyph[row * row_bytes + col / 8] & (0x80 >> (col % 8));
                if (on) {
                    canvas_fill_rect(c, x + col * scale, y + row * scale, scale, scale, fg);
                } else if (bg != CANVAS_NO_BG) {
                    canvas_fill_rect(c, x + col * scale, y + row * scale, scale, scale, (uint16_t)bg);
                }
            }
        }
    }
}
