# Plan: using both cores

Status: draft. Nothing here is implemented. Numbers marked "measure" are estimates from reading the code and need checking on the board.

## 1 - Hardware

`MenuFirmware/CMakeLists.txt` targets `pico2_w`, so the chip is an RP2350: two Cortex-M33 cores at 150 MHz with an FPU, 520 KB of SRAM (not the 264 KB of the original Pico), and no data cache. Both cores see the same memory, so sharing a buffer needs no copying, only agreement on who may touch it when.

## 2 - Where the time goes today

`main.c` is single threaded. Each frame it draws, then calls `display_blit()`, which blocks until the whole frame has been sent.

- **Draw** runs first and alone. The CPU does all the game and rendering work.
- **Blit** builds one row at a time (byte swap, and pixel doubling at half res) while DMA sends the previous row. The CPU is busy for most of the blit, but only because it builds rows, and the SPI is the slower side.
- **The SPI sets the frame rate ceiling.** A full frame is 320 x 240 x 16 bits, about 1.23 Mbit. `display.c` asks for 50 MHz, but the SDK rounds the baud rate down to what the clock divider allows, which may be 37.5 MHz. That gives roughly 25-33 ms per frame, so a ceiling around 30-40 fps (measure: print `spi_get_baudrate(spi1)`).

So the second core will not make frames go out faster. It frees core 0 to draw the next frame while the last one is being sent, and it takes the housekeeping jobs (input, audio) off the game's critical path. The things that do raise the ceiling are on the display side: send only the changed rectangle (dirty rectangles), or a higher SPI clock if the panel tolerates it.

## 3 - Principles

1. **Game code never mentions a core.** Games call canvas, input and audio functions. Which core does the work is a detail of the firmware backend. This keeps the pygame simulator valid, since it just does the same work inline.
2. **One writer per piece of data.** Anything shared has exactly one core that writes it. The other only reads. This avoids most locking.
3. **The game thread never blocks on slow hardware** if it can avoid it. It hands work over and carries on.
4. **Shared hardware is the real hazard**, more than shared memory. See section 6.
5. **Measure before and after each phase.** The benchmark in `main.c` already prints fps and frame time. Keep it as the yardstick.

## 4 - Task allocation (the split)

| Task | Core | Rate / trigger | Why there |
| --- | --- | --- | --- |
| Game logic and rendering into the back canvas | 0 | Once per frame | Needs the most CPU and runs in the order the game wants |
| Menu logic and drawing | 0 | Once per frame | Same code path as the game |
| Display blit (row build, DMA, wait) | 1 | When core 0 presents a frame | Removes the blocking wait from core 0 |
| Input sampling (buttons, ADC) | 1 | Timer, about 1 kHz | Catches taps shorter than a frame, debounces independently of frame rate |
| Audio mixing into the PWM buffer | 1 | Refill when the DMA ring is half empty | Has hard deadlines, so it must not wait on game code |
| Touch read (XPT2046) | 1 | Between blits, on demand | Shares the SPI with the display, so the display owner does it |
| Asset loading from SD | 0, while the display is idle | Menu to game transition | Shares the SPI with the display, see section 6 |
| Serial logging (`printf`) | whoever calls it | Occasional | USB stdio is interrupt driven and safe from either core, but slow, so keep it out of the frame loop |

Core 1 is the "services" core: it owns every piece of hardware that has a deadline. Core 0 is the "game" core and owns nothing except its own state and the back canvas.

Rendering split across both cores is a later option, not part of the first phases. See phase 5.

## 5 - Frame handoff

Two half-res canvases (about 38 KB each) swap roles every frame.

```
core 0                         core 1
draw into canvas[back]
present(back) --------------->  blit canvas[back] to the panel
swap: back = the other one       (the other canvas is free to draw into)
draw into new back               done flag set
wait only if the previous blit
is still running
```

- **Hand-over:** `present(i)` pushes the canvas index through the hardware inter-core FIFO (or a `queue_t` from `pico_util`). Core 1 sets a flag when the blit finishes.
- **Back-pressure:** if core 0 finishes drawing before the previous blit is done, it waits there and only there. The frame rate then equals the slower of draw and blit.
- **Memory:** 2 half-res canvases (75 KB) plus the 150 KB full-res menu canvas is 225 KB of 520 KB. A third half-res canvas (triple buffering) is affordable if the wait turns out to hurt.
- **No data cache** on the RP2350, so no cache maintenance is needed, but a flag shared between cores still needs `volatile` or an atomic and the right memory barrier.
- **Tearing:** the panel is written while it is scanning, so a moving edge can tear. Not a multicore issue, but it becomes visible once frames are sent back to back. The TE pin may fix it if the module exposes it. Check the board.

## 6 - Shared resources and hazards

| Resource | Problem | Rule |
| --- | --- | --- |
| **SPI1** | The LCD, the touch controller and the SD card are on the same bus (separate chip selects, see `display.c`). Two cores using it at once corrupts transfers. | One owner: core 1. Anyone else asks core 1, or waits for a "display idle" window. SD reads happen only when no blit is running. |
| **Flash** | Writing flash (saving scores or settings) stops code fetching from flash. If core 1 is running from flash at the time, it crashes. | Use `flash_safe_execute()`, which pauses the other core. Keep time-critical core 1 code (audio refill, the 1 kHz sampler) in RAM with `__not_in_flash_func`. |
| **ADC** | The joystick uses ADC0 and ADC1 through one shared converter. | Only the input sampler touches it. |
| **DMA channels** | The display claims one; audio needs one or two. | Claim with `dma_claim_unused_channel()`, never hard-code numbers. |
| **GPIO and interrupts** | Interrupts are per-core. An interrupt handler runs on the core that enabled it. | Enable each interrupt from the core that should handle it. Core 1 launches, then sets up its own timer and DMA IRQs. |
| **`stdio`** | Printing from both cores interleaves lines. | Fine for debugging. Don't log from the 1 kHz sampler. |
| **Wi-Fi (CYW43)** | Not used today (`pico_cyw43_arch_none`). If it ever is, it has its own constraints on which core runs its loop. | Revisit if a feature needs it. |

## 7 - Input and audio on core 1

**Input.** Core 1 samples the buttons and the stick at about 1 kHz, runs the debounce and edge logic from `input-library.md`, and publishes a small struct of state plus latched "pressed since last read" bits. Core 0's `input_update()` copies a snapshot once per frame and clears the latches. This resolves the open question in the input plan about taps shorter than one frame: a short tap is latched by core 1 and still seen as a press on the next frame. The input logic layer stays the same, only the thing that feeds it changes (a timer on core 1 instead of a call per frame).

**Audio.** `GP7` drives the amplifier by PWM (not wired yet). The standard approach is a ring buffer of samples that a DMA channel feeds to the PWM at the sample rate (for example 22 kHz). Core 1 refills the ring when it drops below half, mixing the active sound effects. Core 0 only calls `audio_play(sound)`, which posts a message to core 1.

## 8 - Simulator

The pygame simulator stays single threaded. `present()` just draws the canvas to the window, `input_update()` reads the keyboard, and `audio_play()` plays a sound through pygame's mixer. The core split is invisible to game code, so a game that runs in the simulator runs unchanged against the firmware. One difference to remember: the simulator has no frame-rate ceiling from the SPI, so it will run faster than the board.

## 9 - Phases

Each phase ends with a measurement, and the next phase starts only if the numbers justify it.

0. **Baseline.** Record fps, draw ms and blit ms (add timing around `draw_game` and `display_blit`), the real SPI baud rate, and free RAM. Decide from the numbers: if blit is much shorter than draw, a second core for display buys little.
1. **Display on core 1.** Double buffering, `present()` and the FIFO hand-off. Check: fps rises or stays level while core 0's draw time is free of blit time, and no tearing or glitches appear.
2. **Input on core 1.** The 1 kHz sampler and latched snapshots. Check: a very short tap is never lost, and input latency doesn't grow.
3. **Audio on core 1.** Once the amplifier is wired. Check: no clicks or dropouts while the game runs at full load.
4. **Safe flash and SD access.** The flash-lockout and bus rules from section 6, when saving and loading are needed. Check: saving during gameplay doesn't crash core 1.
5. **Optional: parallel rendering.** Only if the game, not the display, is the bottleneck. Core 1 renders the top half of the canvas while core 0 renders the bottom half. The halves touch different pixels, so no locking is needed. It requires the game to describe its drawing as a list rather than immediate calls, so it's a larger change and is deferred until profiling asks for it.

## 10 - What I'd avoid

- **An RTOS (FreeRTOS SMP).** It adds memory, complexity and scheduling surprises, and this design has only a handful of fixed jobs. Revisit only if the number of independent tasks grows a lot.
- **Splitting game logic across cores.** Games are small and sequential. The coordination cost would exceed the gain.
- **Locks on the hot path.** A one-writer-per-datum design avoids needing them.

## 11 - Open decisions

1. **Is the frame rate the goal, or freeing CPU time?** The SPI limits the first. If you want more fps, dirty-rectangle blits matter more than the second core.
2. **How much audio?** Simple beeps (a timer interrupt is enough) or mixed sound effects and music (the core 1 ring-buffer design).
3. **Will games load from SD or be built into flash?** This decides how much the SPI sharing rules matter.
