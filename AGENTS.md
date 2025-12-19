# AGENTS.md

This file provides guidance to AI coding agents when working with code in this repository.

## Project Overview

This repository is a firmware application for the **M5Stack Dial v1.1** (ESP32-S3 /
Stamp S3A). It is built with **ESP-IDF** (C/C++) and uses the **`espressif/m5dial` BSP**
for device bring-up (display/touch/encoder/backlight/etc). The application is a
**kitchen/tea timer** UI.

Primary goals:
- Deterministic, reproducible builds
- Hardware debugging via **USB Serial/JTAG** (OpenOCD + GDB)
- Minimal board-specific boilerplate by using the BSP

## Ground Rules

- Do not use git commands unless explicitly asked.
- Prefer small, reviewable changes. Keep diffs scoped to the task.
- Do not add new dependencies unless necessary; if adding one, explain why and pin it.
- Keep the project buildable at each step; run the verification commands listed below.

## Hardware

The physical device is a M5Stack Dial v1.1 based on the ESP32-S3FN8 Dual-core Xtensa LX7
SoC. It has a touch screen, rotary encoder, and many other features documented
[here](https://docs.m5stack.com/en/core/M5Dial%20V1.1). Many of the GPIO pins are defined as
macros in bsp/m5dial.h (located in managed_components/espressif__m5dial/include/bsp/m5dial.h).

Screen Driver: GC9A01-SPI
GPIO4 - LCD_RS
GPIO5 - LCD_MOSI
GPIO6 - LCD_SCK
GPIO7 - LCD_CS
GPIO8 - LCD_RESET
GPIO9 - LCD_BL

Touch Driver: FT3267
GPIO11 - TP_SDA
GPIO12 - TP_SCL
GPIO14 - TP_INT

I2C: RTC8563
GPIO12 - SCL
GPIO11 - SDA

RFID: WS1850S
GPIO12 - SCL
GPIO11 - SDA
GPIO8 - RST
GPIO10 - IRQ

Rotary Encoder:
GPIO40 - Encoder B
GPIO41 - Encoder A

Other:
GPIO3 - Buzzer

## Development Environment

### Required Tools (macOS / Apple Silicon)

Assume developers use Homebrew and ESP-IDF’s standard tooling.
- `cmake`, `ninja`, `dfu-util`, `ccache`
- ESP-IDF toolchain for `esp32s3`
- Python env managed by ESP-IDF `install.sh`

Agents should avoid suggesting alternate build systems. This is a CMake/Ninja ESP-IDF project.

### Environment Setup

Agents should assume the developer has ESP-IDF installed under `~/esp/esp-idf` and has exported
IDF variables in the local terminal using `. ~/esp/esp-idf/export.sh`

Do not hardcode absolute paths in code or CMake. Use ESP-IDF’s standard variables and component patterns.

### Common Commands

All commands are run from the project root after sourcing `export.sh`.

* Build: `idf.py build`
* Flash + serial monitor: `idf.py flash monitor`
* Clean: `idf.py clean`
* Configure: `idf.py menuconfig`


#### USB Serial/JTAG debugging

In separate terminals:

OpenOCD:
```sh
idf.py openocd
```

GDB TUI:
```sh
idf.py gdbtui
```

Optional combined workflow:
```sh
idf.py openocd gdbtui monitor
```

If debugging fails because OpenOCD scripts are missing, verify the ESP-IDF environment
is exported (OPENOCD_SCRIPTS should be set by `export.sh`).

### BSP / Hardware Abstraction

This project uses the ESP-IDF Board Support Package (BSP) for M5Dial:
  * Dependency is managed via ESP-IDF Component Manager (`idf.py add-dependency ...`).
  * Use BSP helpers to initialize hardware instead of re-implementing low-level drivers.

Guidelines:
  * Prefer BSP APIs such as display init / LVGL start helpers and device IO wrappers.
  * Do not directly reconfigure pins or buses that the BSP already owns unless required.
  * If you must adjust a BSP configuration, do so via Kconfig options or documented hooks rather than editing vendored/managed component sources.

Managed components live under `managed_components/` and should not be modified directly.

### LVGL

LVGL is available. `bsp_display_start()` initializes display, touch and LVGL.
BSP requires that all LVGL calls must be protected using lock/unlock:

```c
/* Wait until other tasks finish screen operations */
bsp_display_lock(0);
/* safe to perform LVGL operations */
lv_obj_t *screen = lv_disp_get_scr_act(disp_handle);
lv_obj_t *label = lv_label_create(screen);
/* Unlock after screen operations are done */
bsp_display_unlock();
```

Because of this, using LVGL should either be done via an event queue or a timer that updates
based on global UI state.

## Coding Conventions

### Language / Style

* C is preferred over C++; match the existing code style in the repo.
* Prefer clear, explicit types and error handling.
* Avoid dynamic allocation in hot paths; keep memory usage predictable.

### Naming

* Functions: `snake_case` (C-style) or `camelCase` (C++) — follow existing files.
* Constants/macros: `UPPER_SNAKE_CASE`.
* File names: `snake_case` for C modules; follow existing conventions.

### Logging / Errors

* Use ESP-IDF logging (ESP_LOGI/W/E) with a consistent module tag.
* Check return values for ESP-IDF APIs; propagate or log failures.
* Avoid silent fallback behavior.

### Concurrency

* If using tasks, define clear ownership of shared state.
* Protect shared mutable state with a mutex or message passing; prefer queues/events when appropriate.
