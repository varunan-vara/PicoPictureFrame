#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include <stdbool.h>
#include "font/fonts.h"

// A software framebuffer in RGB565. Draw into it, then send it with display_blit()
typedef struct {
    uint16_t *px;
    int w, h;
} canvas_t;

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define CANVAS_NO_BG (-1)

void canvas_fill_rect(canvas_t *c, int x, int y, int w, int h, uint16_t color);
void canvas_rect_outline(canvas_t *c, int x, int y, int w, int h, uint16_t color);

// bg is a colour, or CANVAS_NO_BG to leave the pixels under the text alone
void canvas_text(canvas_t *c, int x, int y, const char *s, const sFONT *font,
                 int scale, uint16_t fg, int32_t bg);

#endif
