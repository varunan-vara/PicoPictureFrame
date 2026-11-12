# Device Specification and Design

## Summary

## Table of Contents

- [Device Specification and Design](#device-specification-and-design)
  - [Summary](#summary)
  - [Table of Contents](#table-of-contents)
  - [1 - Device Overview](#1---device-overview)
    - [1.1 - Physical Description](#11---physical-description)
    - [1.2 - Commponents Used](#12---commponents-used)
    - [1.3 - Diagrams](#13---diagrams)
  - [2 - Functionality](#2---functionality)
    - [2.1 - Core Functionality Summary](#21---core-functionality-summary)
    - [2.2 - I/O](#22---io)
    - [2.3 - Power Requirements](#23---power-requirements)
  - [3 - Hardware Specifications](#3---hardware-specifications)
    - [3.1 - Raspberry Pi Pico Pinout](#31---raspberry-pi-pico-pinout)
      - [Display and touch (Waveshare Pico-ResTouch-LCD-2.8)](#display-and-touch-waveshare-pico-restouch-lcd-28)
      - [Controls](#controls)
      - [Reserved](#reserved)
      - [Pin summary](#pin-summary)
    - [3.2 - Component Details](#32---component-details)
  - [4 - Software Specifications](#4---software-specifications)
    - [4.1 - Language, Interface and SDK used](#41---language-interface-and-sdk-used)
    - [4.2 - Program Structure](#42---program-structure)
    - [4.3 - Debug and Test](#43---debug-and-test)
    - [4.4 - Compatibility with alternative devices](#44---compatibility-with-alternative-devices)
  - [5 - CAD Model and Chassis](#5---cad-model-and-chassis)
    - [5.1 - Design](#51---design)
  - [Notes to be reassigned later](#notes-to-be-reassigned-later)

## 1 - Device Overview



### 1.1 - Physical Description

### 1.2 - Commponents Used

### 1.3 - Diagrams

## 2 - Functionality

### 2.1 - Core Functionality Summary

### 2.2 - I/O

### 2.3 - Power Requirements

## 3 - Hardware Specifications

### 3.1 - Raspberry Pi Pico Pinout

Pin numbers are GPIO numbers (GPn) with the physical header pin in the Pin column. Power and ground connections are not listed in the tables.

#### Display and touch (Waveshare Pico-ResTouch-LCD-2.8)

The ST7789 screen, the XPT2046 touch controller and the microSD slot share `spi1` and are told apart by their chip select lines. The first eight signals match the `PIN_*` defines in `MenuFirmware/src/display/display.c`.

| Signal | GPIO | Pin | Function |
| --- | --- | --- | --- |
| LCD_DC | GP8 | 11 | Data / command select |
| LCD_CS | GP9 | 12 | Screen chip select |
| SPI1 SCK | GP10 | 14 | SPI clock (screen, touch, SD) |
| SPI1 TX (MOSI) | GP11 | 15 | SPI data out (screen, touch, SD) |
| LCD_BL | GP13 | 17 | Backlight |
| LCD_RST | GP15 | 20 | Screen reset |
| TP_CS | GP16 | 21 | XPT2046 chip select |
| SD_CS | GP22 | 29 | microSD chip select |
| SPI1 RX (MISO) | GP12 | 16 | SPI data in (touch, SD) |
| TP_IRQ | GP17 | 22 | XPT2046 pen interrupt, low while the screen is touched |

MISO and TP_IRQ are not used by the firmware yet. All pin assignments above are taken from the [Waveshare wiki](https://www.waveshare.com/wiki/Pico-ResTouch-LCD-2.8). The board is powered from the Pico's VSYS pin.

The microSD slot is also wired for SDIO, which takes GP5 (SDIO_CLK), GP18 (SDIO_CMD), GP19 - GP21 (SDIO data 0 - 2) and GP22 (SD_CS / data 3). Those pins are reserved even if SD is used in SPI mode, so nothing else is assigned to them.

#### Controls

| Part | Part pin | GPIO | Pin | Notes |
| --- | --- | --- | --- | --- |
| Button A | signal | GP2 | 4 | Internal pull-up |
| Button B | signal | GP3 | 5 | Internal pull-up |
| Menu button | signal | GP4 | 6 | Internal pull-up |
| Joystick | VRX | GP26 | 31 | ADC0, X axis |
| Joystick | VRY | GP27 | 32 | ADC1, Y axis |
| Joystick | SW | GP14 | 19 | Pushbutton, internal pull-up |
| 3-pin switch | middle pin | GP6 | 9 | Internal pull-up |

The buttons, the joystick's SW pin and the switch all use the Pico's internal pull-up resistors (`gpio_pull_up()`), so no external resistors are needed. Each input reads high when idle and low when pressed or switched on.

The joystick will run on 3.3V, not 5V. Its VRX and VRY outputs swing between 0V and whatever the module is powered from, and the Pico's ADC pins only tolerate 3.3V. The module's `+5V` pin therefore goes to the Pico's 3V3(OUT) pin, not to VBUS. The module works fine at 3.3V and the full ADC range is still used.

The switch is currently documented as a single on/off input. If it ends up cutting power to the whole device instead, it goes in series with the battery or USB supply and GP6 is free again.

#### Reserved

| Use | GPIO | Pin | Notes |
| --- | --- | --- | --- |
| Audio out | GP7 | 10 | PWM output to the PAM8302A amplifier (not wired yet) |

#### Pin summary

| GPIO | Use | | GPIO | Use |
| --- | --- | --- | --- | --- |
| GP0 | Free | | GP12 | SPI1 RX (MISO) |
| GP1 | Free | | GP13 | LCD_BL |
| GP2 | Button A | | GP14 | Joystick SW |
| GP3 | Button B | | GP15 | LCD_RST |
| GP4 | Menu button | | GP16 | TP_CS |
| GP5 | SDIO_CLK (SD slot) | | GP17 | TP_IRQ |
| GP6 | Switch | | GP18 - GP21 | SDIO (SD slot) |
| GP7 | Audio (reserved) | | GP22 | SD_CS |
| GP8 | LCD_DC | | GP26 | Joystick VRX (ADC0) |
| GP9 | LCD_CS | | GP27 | Joystick VRY (ADC1) |
| GP10 | SPI1 SCK | | GP28 | Free (ADC2) |
| GP11 | SPI1 TX (MOSI) | | | |

GP23, GP24, GP25 and GP29 are used internally by the Pico 2 W's wireless chip and are not available.

### 3.2 - Component Details

## 4 - Software Specifications

### 4.1 - Language, Interface and SDK used

### 4.2 - Program Structure

### 4.3 - Debug and Test

### 4.4 - Compatibility with alternative devices

## 5 - CAD Model and Chassis

### 5.1 - Design

## Notes to be reassigned later

- The inputs are polled (every 5 to 10 ms) rather than interrupt driven, because buttons can be held down for actions and an interrupt only reports a press or release, not that the button is still held.
