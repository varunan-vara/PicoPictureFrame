#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "input/input_hw.h"

// Pins, from doc/Specification and Design.md section 3.1
#define PIN_BTN_A    2
#define PIN_BTN_B    3
#define PIN_BTN_MENU 4
#define PIN_JOY_SW   14
#define PIN_PWR_SWITCH 6
#define PIN_JOY_X    26   // VRX, ADC0
#define PIN_JOY_Y    27   // VRY, ADC1

#define ADC_CH_X 0
#define ADC_CH_Y 1

// Reads averaged per axis. The Pico's ADC is noisy, so one read jitters by a few counts
#define OVERSAMPLE 8

#define DEBUG_PRINT_MS 100

void input_hw_init(void) {
    const uint buttons[] = { PIN_BTN_A, PIN_BTN_B, PIN_BTN_MENU, PIN_JOY_SW, PIN_PWR_SWITCH };
    for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++) {
        gpio_init(buttons[i]);
        gpio_set_dir(buttons[i], GPIO_IN);
        gpio_pull_up(buttons[i]);     // idle high, pressed (or switched on) = low
    }

    adc_init();
    adc_gpio_init(PIN_JOY_X);         // makes the pin analog: no pull, digital input off
    adc_gpio_init(PIN_JOY_Y);
}

uint8_t input_hw_read_buttons(void) {
    uint8_t bits = 0;
    if (!gpio_get(PIN_BTN_A))    bits |= INPUT_HW_BTN_A;
    if (!gpio_get(PIN_BTN_B))    bits |= INPUT_HW_BTN_B;
    if (!gpio_get(PIN_BTN_MENU)) bits |= INPUT_HW_BTN_MENU;
    if (!gpio_get(PIN_JOY_SW))   bits |= INPUT_HW_BTN_JOY_SW;
    if (!gpio_get(PIN_PWR_SWITCH)) bits |= INPUT_HW_PWR_SWITCH;
    return bits;
}

static uint16_t read_axis(uint channel) {
    adc_select_input(channel);
    (void)adc_read();                 // throw one away: the input just switched, and the
                                      // stick has a high source impedance, so it needs a moment
    uint32_t sum = 0;
    for (int i = 0; i < OVERSAMPLE; i++) sum += adc_read();
    return (uint16_t)(sum / OVERSAMPLE);
}

void input_hw_read_adc(uint16_t *x, uint16_t *y) {
    *x = read_axis(ADC_CH_X);
    *y = read_axis(ADC_CH_Y);
}

void input_hw_debug_print(void) {
    static uint32_t last_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_ms < DEBUG_PRINT_MS) return;
    last_ms = now;

    uint16_t x, y;
    input_hw_read_adc(&x, &y);
    uint8_t b = input_hw_read_buttons();

    // 1 = pressed. SW is the joystick's pushbutton. PWR is the power switch: 1 = switched on.
    printf("joy x=%4u y=%4u | SW=%d A=%d B=%d MENU=%d PWR=%d\n", x, y,
           !!(b & INPUT_HW_BTN_JOY_SW), !!(b & INPUT_HW_BTN_A),
           !!(b & INPUT_HW_BTN_B), !!(b & INPUT_HW_BTN_MENU),
           !!(b & INPUT_HW_PWR_SWITCH));
}
