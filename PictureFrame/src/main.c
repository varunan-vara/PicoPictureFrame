#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "display/display.h"

// Game canvas is half the screen in each direction, the menu is full resolution
#define GAME_W (LCD_WIDTH / 2)
#define GAME_H (LCD_HEIGHT / 2)

#define MENU_MS 3000
#define GAME_MS 10000

#define NUM_SPRITES 24
#define SPRITE_SIZE 8

static uint16_t menu_px[LCD_WIDTH * LCD_HEIGHT];
static uint16_t game_px[GAME_W * GAME_H];
static canvas_t menu = { menu_px, LCD_WIDTH, LCD_HEIGHT };
static canvas_t game = { game_px, GAME_W, GAME_H };

typedef struct {
    float x, y, dx, dy;
    uint16_t color;
} sprite_t;

typedef struct {
    float fps;
    float frame_ms;
    float menu_blit_ms;
} results_t;

static void draw_menu(const results_t *r, int selected) {
    canvas_fill_rect(&menu, 0, 0, menu.w, menu.h, RGB565(16, 24, 48));
    canvas_fill_rect(&menu, 0, 0, menu.w, 40, RGB565(40, 60, 120));
    canvas_text(&menu, 16, 12, "PICO GAME TEST", &Font16, 2, RGB565(255, 255, 255), CANVAS_NO_BG);

    static const char *items[] = { "Start game", "Settings", "About" };
    for (int i = 0; i < 3; i++) {
        int y = 64 + i * 36;
        uint16_t bg = i == selected ? RGB565(255, 200, 40) : RGB565(32, 44, 80);
        uint16_t fg = i == selected ? RGB565(0, 0, 0) : RGB565(220, 220, 220);
        canvas_fill_rect(&menu, 24, y, menu.w - 48, 28, bg);
        canvas_text(&menu, 36, y + 6, items[i], &Font16, 1, fg, CANVAS_NO_BG);
    }

    // 1px outlines check the panel shows all 320x240 pixels with nothing cropped
    canvas_rect_outline(&menu, 0, 0, menu.w, menu.h, RGB565(255, 0, 0));

    char line[48];
    if (r->fps > 0) {
        snprintf(line, sizeof line, "Game: %.1f fps  (%.1f ms/frame)", r->fps, r->frame_ms);
        canvas_text(&menu, 16, 184, line, &Font16, 1, RGB565(120, 255, 120), CANVAS_NO_BG);
        snprintf(line, sizeof line, "Full-res blit: %.1f ms", r->menu_blit_ms);
        canvas_text(&menu, 16, 204, line, &Font16, 1, RGB565(120, 255, 120), CANVAS_NO_BG);
    } else {
        canvas_text(&menu, 16, 184, "Running first test...", &Font16, 1, RGB565(180, 180, 180), CANVAS_NO_BG);
    }
}

static void init_sprites(sprite_t *s) {
    srand(1);
    for (int i = 0; i < NUM_SPRITES; i++) {
        s[i].x = rand() % (GAME_W - SPRITE_SIZE);
        s[i].y = rand() % (GAME_H - SPRITE_SIZE);
        s[i].dx = (rand() % 200 - 100) / 50.0f;
        s[i].dy = (rand() % 200 - 100) / 50.0f;
        s[i].color = RGB565(80 + rand() % 176, 80 + rand() % 176, 80 + rand() % 176);
    }
}

static void draw_game(sprite_t *s, int frame, float fps) {
    // Scrolling checkerboard, drawn per pixel so it exercises the renderer
    int sx = frame, sy = frame / 2;
    uint16_t a = RGB565(24, 96, 48), b = RGB565(32, 128, 64);
    for (int y = 0; y < GAME_H; y++) {
        for (int x = 0; x < GAME_W; x++) {
            game.px[y * GAME_W + x] = (((x + sx) >> 4) ^ ((y + sy) >> 4)) & 1 ? a : b;
        }
    }

    for (int i = 0; i < NUM_SPRITES; i++) {
        s[i].x += s[i].dx;
        s[i].y += s[i].dy;
        if (s[i].x < 0 || s[i].x > GAME_W - SPRITE_SIZE) s[i].dx = -s[i].dx;
        if (s[i].y < 0 || s[i].y > GAME_H - SPRITE_SIZE) s[i].dy = -s[i].dy;
        canvas_fill_rect(&game, (int)s[i].x, (int)s[i].y, SPRITE_SIZE, SPRITE_SIZE, s[i].color);
        canvas_rect_outline(&game, (int)s[i].x, (int)s[i].y, SPRITE_SIZE, SPRITE_SIZE, RGB565(0, 0, 0));
    }

    // "Player" on a looping path
    int px = GAME_W / 2 + (int)(50 * sinf(frame * 0.05f));
    int py = GAME_H / 2 + (int)(30 * sinf(frame * 0.08f));
    canvas_fill_rect(&game, px - 5, py - 5, 10, 10, RGB565(255, 255, 255));
    canvas_fill_rect(&game, px - 3, py - 3, 6, 6, RGB565(220, 40, 40));

    // 1px border: should look 2px thick on screen if the 2x scaling is right
    canvas_rect_outline(&game, 0, 0, GAME_W, GAME_H, RGB565(255, 0, 0));

    char line[16];
    snprintf(line, sizeof line, "FPS %.0f", fps);
    canvas_text(&game, 4, 4, line, &Font8, 1, RGB565(255, 255, 0), RGB565(0, 0, 0));
}

static results_t run_game(void) {
    sprite_t sprites[NUM_SPRITES];
    init_sprites(sprites);

    uint32_t start = to_ms_since_boot(get_absolute_time());
    uint32_t last_report = start;
    int frame = 0, frames_in_window = 0;
    float shown_fps = 0;

    while (to_ms_since_boot(get_absolute_time()) - start < GAME_MS) {
        draw_game(sprites, frame++, shown_fps);
        display_blit(&game, 2);
        frames_in_window++;

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_report >= 500) {
            shown_fps = frames_in_window * 1000.0f / (now - last_report);
            printf("fps %.1f\n", shown_fps);
            frames_in_window = 0;
            last_report = now;
        }
    }

    uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;
    results_t r = { 0 };
    r.fps = frame * 1000.0f / elapsed;
    r.frame_ms = (float)elapsed / frame;
    return r;
}

int main() {
    stdio_init_all();
    display_init();

    results_t results = { 0 };
    int selected = 0;

    while (true) {
        draw_menu(&results, selected);
        uint64_t t0 = time_us_64();
        display_blit(&menu, 1);
        results.menu_blit_ms = (time_us_64() - t0) / 1000.0f;
        printf("full-res blit %.1f ms\n", results.menu_blit_ms);
        sleep_ms(MENU_MS);

        float blit_ms = results.menu_blit_ms;
        results = run_game();
        results.menu_blit_ms = blit_ms;
        printf("game avg %.1f fps (%.1f ms/frame)\n", results.fps, results.frame_ms);

        selected = (selected + 1) % 3;
    }
}
