# PicoPictureFrame

This project is my attempt to build a small handheld device around the Raspberry Pi Pico 2 W. It started out as a picture frame (hence the name), but it has since turned into something completely different: a pocket sized device with a touchscreen, buttons, a joystick and a speaker, all for under $50.

## Goals

- Keep the total cost of parts under **$50 USD**
- Everything runs off the Pico 2 W, no Raspberry Pi 5 or other computer on board
- Written in C using the Raspberry Pi Pico SDK
- Simple enough to build on a breadboard, then move to a custom case (see `cad/`)

## Components

| Part | Purpose | Cost |
| --- | --- | --- |
| Raspberry Pi Pico 2 W | The brain of the project | *TBD* |
| Waveshare Pico-ResTouch-LCD-2.8 | 2.8" 320x240 screen with resistive touch and a microSD slot | *TBD* |
| Adafruit Mono 2.5W Class D Audio Amplifier (PAM8302A) | Drives the speaker | *TBD* |
| Speaker (PiShop SKU 172) | Sound | *TBD* |
| Buttons | Controls | *TBD* |
| Switch | Power / mute | *TBD* |
| Joystick | Analog input | *TBD* |
| **Total** | | *TBD / $50* |

## Repository structure

```
PicoPictureFrame/
├── PictureFrame/        Firmware (C, Pico SDK)
│   ├── src/
│   │   ├── main.c
│   │   ├── display/     Screen driver, canvas (software framebuffer) and fonts
│   │   ├── input/       Buttons, switch and joystick
│   │   └── audio/       Speaker output through the amplifier
│   ├── CMakeLists.txt
│   └── pico_sdk_import.cmake
├── cad/                 Case and chassis models
├── doc/                 Specification and design documents
└── README.md
```

## Environment Setup

1. Install the [Raspberry Pi Pico VS Code extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico). It downloads the Pico SDK (2.2.0), the toolchain, CMake and Ninja for you.
2. Open the repository in VS Code and import the `PictureFrame` folder as a Pico project.
3. The board is set to `pico2_w` in `PictureFrame/CMakeLists.txt`. Leave it as is.
4. Build the project, then hold **BOOTSEL** while plugging in the Pico and flash the generated `.uf2` file onto it.

Plug the Waveshare screen onto the Pico, flash the `.uf2` and open the USB serial port to see the numbers printed.

## Display test

`main.c` is currently a display test. It loops between two screens:

- A full resolution (320x240) menu, which also shows the results of the last run
- A game like scene rendered at 160x120 and scaled up 2x on the way to the screen, with a scrolling background, bouncing sprites and an FPS counter

The red 1px borders let you check that nothing is cropped and that the 2x scaling is working. Note that the screen still has to be sent all 320x240 pixels every frame, so the SPI speed (`LCD_SPI_BAUD` in `display.c`) sets the maximum frame rate. The lower resolution saves RAM and rendering time, not bandwidth.

The display code is based on Waveshare's demo for the screen (pins, init sequence and fonts), rewritten to use DMA.

## Status

The old code (an ILI9341 driver written for a different display) has been scrapped. Everything is being rebuilt around the new parts.

- [x] New project skeleton for the Pico 2 W
- [x] Screen output (test written, needs checking on real hardware)
- [ ] Touch input
- [ ] Buttons, switch and joystick
- [ ] Audio
- [ ] Case design
