# Plan: input library (buttons and joystick)

Status: draft, waiting on the decisions in section 5.

How to divide the work between people or agents is in [input-library-tasks.md](input-library-tasks.md).

Pins and the behaviour each input needs (press, hold, analog, digital) are in `Specification and Design.md`, sections 2.2 and 3.1. This file covers how to build the library.

## 1 - Architecture

Three layers, so the tricky logic is the same in Python and C and only the bottom layer differs.

```
game code      input_pressed(BTN_A), input_joy_x(), ...
   |
logic layer    debounce, edge detect, dead zone, scaling, hysteresis
   |
raw source     Pico: GPIO + ADC            Simulator: pygame keyboard
```

The logic layer takes a plain struct of raw readings and never touches hardware, so it runs on a laptop for tests:

```c
typedef struct {
    uint8_t  buttons;   // bit per physical button, 1 = down (already inverted from active-low)
    uint16_t adc_x;     // 12 bit
    uint16_t adc_y;
    uint32_t ms;        // timestamp, for debounce lockout
} input_raw_t;
```

## 2 - API (the contract)

```c
typedef enum { BTN_A, BTN_B, BTN_MENU, BTN_JOY_SW,
               BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_COUNT } button_t;

void  input_init(void);           // sets up pins, calibrates the stick centre
void  input_update(void);         // once per frame: sample, then step the logic
void  input_step(const input_raw_t *raw);   // logic layer only, used by tests and input_update()
bool  input_pressed(button_t b);  // true only on the frame it went down
bool  input_held(button_t b);     // true every frame while down
bool  input_released(button_t b); // true only on the frame it went up
float input_joy_x(void);          // -1.0 .. 1.0
float input_joy_y(void);
```

The Python mirror (`simulator/picoinput.py`) uses the same names and behaviour.

The joystick's analog and digital modes are not a setting. A game uses `input_joy_x/y` for analog, or `BTN_UP/DOWN/LEFT/RIGHT` for digital. Both are always available.

## 3 - Logic rules

- **Buttons:** active-low with pull-ups, so invert first. Debounce with a lockout: accept the first edge immediately, then ignore changes for about 10-20 ms. This adds no press latency.
- **Press and hold:** keep the previous and current state. `pressed = now && !prev`, `released = !now && prev`. Both last exactly one `input_update()`.
- **Joystick axes:**
  1. Oversample the ADC (4-8 reads, averaged).
  2. Calibrate the centre at `input_init()` by averaging samples (stick untouched at boot).
  3. Scale each side of the centre separately, then clamp to +/-1.
  4. Apply a dead zone (start at 0.1) and rescale so the output still reaches 1.0.
  5. Per-axis invert and swap flags for mounting orientation.
- **Digital directions:** on above 0.5, off below 0.3, checked per axis so a diagonal gives two directions. The result goes through the same press/hold/release state machine as the buttons.
- **Simulator source:** arrows or WASD for the stick (+/-1.0, optionally smoothed), Z and X for A and B, Esc for Menu, Enter for the stick switch. Read with `pygame.key.get_pressed()` inside `update()`, after `Display.poll()` has pumped events.

## 4 - Files

| File | Purpose |
| --- | --- |
| `MenuFirmware/src/input/input.h` | The contract |
| `MenuFirmware/src/input/input.c` | Logic layer, no hardware calls |
| `MenuFirmware/src/input/input_hw.c` | GPIO and ADC reads for GP2, GP3, GP4, GP14, GP26, GP27 |
| `simulator/picoinput.py` | Python mirror with keyboard source |
| `simulator/tests/` | Python logic tests |
| `tests/input_vectors/` | Shared sample streams and expected outputs (CSV), used by both the Python and C tests |
| `MenuFirmware/tests/` | Host build of `input.c` that replays the vectors |
| `doc/Specification and Design.md` | API table added next to the existing input section |

## 5 - Open decisions

Defaults are in bold. Work on the contract can start with them and change cheaply before step T0 is closed.

1. **Y-axis sign:** **up = +1.0** (the `BTN_UP` digital input is the unambiguous alternative), or up = -1.0 to match screen coordinates.
2. **Update rate:** **`input_update()` once per game frame (30 fps)**. A tap shorter than one frame can be missed. Latching edges in a GPIO interrupt is the later fix if that matters.
3. **GP6 switch:** **not part of this library.** The spec notes it may end up cutting power rather than being an input.
