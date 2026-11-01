#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "canvas.h"

// Panel in landscape: the Waveshare 2.8" is a 240x320 panel rotated 90 degrees
#define LCD_WIDTH  320
#define LCD_HEIGHT 240

void display_init(void);

// Sends a canvas to the whole screen. The canvas is either 320x240 (scale 1)
// or 160x120 (scale 2, every pixel is doubled in both directions)
void display_blit(const canvas_t *c, int scale);

#endif
