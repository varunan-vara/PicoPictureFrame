#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "display.h"

// Pico-ResTouch-LCD-2.8 wiring (the LCD, touch and SD card share spi1)
#define PIN_DC   8
#define PIN_CS   9
#define PIN_CLK  10
#define PIN_MOSI 11
#define PIN_BL   13
#define PIN_RST  15
#define PIN_TP_CS 16
#define PIN_SD_CS 22

#define LCD_SPI spi1
// Lower this if the picture is garbled
#define LCD_SPI_BAUD (50 * 1000 * 1000)

// Memory access control. 0x60 = landscape, flip to 0xA0 if the image is rotated 180
#define LCD_MADCTL 0x60

// Two rows of up to 320 pixels, so a 2x blit can send each source row as two screen rows
static uint16_t linebuf[2][2 * LCD_WIDTH];
static int dma_ch;

static void lcd_cmd(uint8_t cmd) {
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(LCD_SPI, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

static void lcd_data(const uint8_t *data, size_t len) {
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(LCD_SPI, data, len);
    gpio_put(PIN_CS, 1);
}

static void lcd_cmd_args(uint8_t cmd, const uint8_t *args, size_t len) {
    lcd_cmd(cmd);
    if (len) lcd_data(args, len);
}

#define CMD(c, ...) do { \
    static const uint8_t a[] = { __VA_ARGS__ }; \
    lcd_cmd_args(c, a, sizeof a); \
} while (0)

static void lcd_set_window(int x0, int y0, int x1, int y1) {
    const uint8_t cols[] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    const uint8_t rows[] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    lcd_cmd_args(0x2A, cols, sizeof cols);
    lcd_cmd_args(0x2B, rows, sizeof rows);
    lcd_cmd(0x2C);
}

void display_init(void) {
    spi_init(LCD_SPI, LCD_SPI_BAUD);
    gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Control pins. Touch and SD chip selects stay high so they ignore the shared bus
    const uint outs[] = { PIN_DC, PIN_CS, PIN_BL, PIN_RST, PIN_TP_CS, PIN_SD_CS };
    for (size_t i = 0; i < sizeof outs / sizeof outs[0]; i++) {
        gpio_init(outs[i]);
        gpio_set_dir(outs[i], GPIO_OUT);
    }
    gpio_put(PIN_CS, 1);
    gpio_put(PIN_TP_CS, 1);
    gpio_put(PIN_SD_CS, 1);

    gpio_put(PIN_RST, 1);
    sleep_ms(50);
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);

    // Init sequence from Waveshare's demo for the 2.8" board
    lcd_cmd(0x11);                      // sleep out
    sleep_ms(120);
    CMD(0x36, LCD_MADCTL);
    CMD(0x3A, 0x55);                    // 16 bit colour
    CMD(0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33);
    CMD(0xB7, 0x35);
    CMD(0xBB, 0x28);
    CMD(0xC0, 0x3C);
    CMD(0xC2, 0x01);
    CMD(0xC3, 0x0B);
    CMD(0xC4, 0x20);
    CMD(0xC6, 0x0F);
    CMD(0xD0, 0xA4, 0xA1);
    CMD(0xE0, 0xD0, 0x01, 0x08, 0x0F, 0x11, 0x2A, 0x36, 0x55, 0x44, 0x3A, 0x0B, 0x06, 0x11, 0x20);
    CMD(0xE1, 0xD0, 0x02, 0x07, 0x0A, 0x0B, 0x18, 0x34, 0x43, 0x4A, 0x2B, 0x1B, 0x1C, 0x22, 0x1F);
    CMD(0x55, 0xB0);
    lcd_cmd(0x29);                      // display on
    sleep_ms(20);

    gpio_put(PIN_BL, 1);

    dma_ch = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_dreq(&cfg, spi_get_dreq(LCD_SPI, true));
    dma_channel_configure(dma_ch, &cfg, &spi_get_hw(LCD_SPI)->dr, NULL, 0, false);
}

static inline void dma_send(const void *src, size_t bytes) {
    dma_channel_set_read_addr(dma_ch, src, false);
    dma_channel_set_trans_count(dma_ch, bytes, true);
}

void display_blit(const canvas_t *c, int scale) {
    lcd_set_window(0, 0, c->w * scale - 1, c->h * scale - 1);
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    int out_w = c->w * scale;
    bool busy = false;

    for (int y = 0; y < c->h; y++) {
        // Build the next row while the previous one is still being sent
        uint16_t *buf = linebuf[y & 1];
        const uint16_t *src = &c->px[y * c->w];

        if (scale == 2) {
            for (int x = 0; x < c->w; x++) {
                uint16_t p = __builtin_bswap16(src[x]);   // panel wants big endian
                buf[2 * x] = p;
                buf[2 * x + 1] = p;
            }
            memcpy(buf + out_w, buf, out_w * sizeof(uint16_t));
        } else {
            for (int x = 0; x < c->w; x++) buf[x] = __builtin_bswap16(src[x]);
        }

        if (busy) dma_channel_wait_for_finish_blocking(dma_ch);
        // A 2x row is sent twice (two screen rows), a 1x row once
        dma_send(buf, out_w * scale * sizeof(uint16_t));
        busy = true;
    }

    dma_channel_wait_for_finish_blocking(dma_ch);
    while (spi_get_hw(LCD_SPI)->sr & SPI_SSPSR_BSY_BITS) tight_loop_contents();
    gpio_put(PIN_CS, 1);
}
