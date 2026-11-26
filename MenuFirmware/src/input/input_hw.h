#ifndef INPUT_HW_H
#define INPUT_HW_H

#include <stdint.h>

// Raw hardware reads for the buttons and the joystick. No scaling, dead zone or debouncing
// here: that belongs in the logic layer (see doc/plans/input-library.md).

// Bits of the value returned by input_hw_read_buttons(), in the order of button_t
#define INPUT_HW_BTN_A      (1u << 0)
#define INPUT_HW_BTN_B      (1u << 1)
#define INPUT_HW_BTN_MENU   (1u << 2)
#define INPUT_HW_BTN_JOY_SW (1u << 3)

// Sets up the button pins (inputs with pull-ups) and the joystick's two ADC pins
void input_hw_init(void);

// One bit per button, 1 = pressed (already inverted from the active-low pins)
uint8_t input_hw_read_buttons(void);

// Joystick axes as raw 12 bit ADC values (0..4095, about 2048 when centred),
// each averaged over a few samples
void input_hw_read_adc(uint16_t *x, uint16_t *y);

// Bring-up helper: prints the joystick and button state to the USB serial port, at most
// 10 times a second. Call it as often as you like from any loop.
void input_hw_debug_print(void);

#endif
