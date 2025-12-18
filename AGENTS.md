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

## Development Environment

### Required Tools (macOS / Apple Silicon)

Assume developers use Homebrew and ESP-IDF’s standard tooling.
- `cmake`, `ninja`, `dfu-util`, `ccache`
- ESP-IDF toolchain for `esp32s3`
- Python env managed by ESP-IDF `install.sh`

Agents should avoid suggesting alternate build systems. This is a CMake/Ninja ESP-IDF project.

### Environment Setup

Agents should assume the developer has ESP-IDF installed under `~/esp/esp-idf` and uses:

```sh
. ~/esp/esp-idf/export.sh
```

Do not hardcode absolute paths in code or CMake. Use ESP-IDF’s standard variables and component patterns.

### Common Commands

All commands are run from the project root after sourcing `export.sh`.

* Build: `idf.py build`
* Flash + serial monitor: `idf.py flash monitor`
* Clean: `idf.py fullclean`
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

This project uses the ESP-IDF Board Support Package for M5Dial:
  * Dependency is managed via ESP-IDF Component Manager (`idf.py add-dependency ...`).
  * Use BSP helpers to initialize hardware instead of re-implementing low-level drivers.

Guidelines:
  * Prefer BSP APIs such as display init / LVGL start helpers and device IO wrappers.
  * Do not directly reconfigure pins or buses that the BSP already owns unless required.
  * If you must adjust a BSP configuration, do so via Kconfig options or documented hooks rather than editing vendored/managed component sources.

Managed components live under `managed_components/` and should not be modified directly.


## Coding Conventions

### Language / Style

* C or C++ is acceptable; match the existing code style in the repo.
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
* Do not call LVGL from multiple tasks unless the project has an established locking strategy.