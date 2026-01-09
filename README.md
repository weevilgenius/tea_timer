# Tea Timer

A kitchen/tea timer application for the [M5Stack Dial v1.1](https://shop.m5stack.com/products/m5stack-dial-v1-1),
built with ESP-IDF and LVGL.

## Hardware

- **M5Stack Dial v1.1** (ESP32-S3FN8)
- USB-C cable for flashing and debugging

## Prerequisites

### ESP-IDF

This project requires **ESP-IDF v5.3 or later**. Follow the official installation guide:

https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/

### macOS (Homebrew)

```sh
xcode-select --install
brew install cmake ninja dfu-util ccache
```

### ESP-IDF Setup

After installing ESP-IDF, install the ESP32-S3 toolchain:

```sh
cd ~/esp/esp-idf
./install.sh esp32s3
```

## Configure

There are some features such as screen rotation and disabling buzzer output that can
be tweaked by editing the top of `main/tea_timer.c`.

## Building

1. Clone this repository

2. Source the ESP-IDF environment (required in each terminal session):
   ```sh
   . ~/esp/esp-idf/export.sh
   ```

3. Build the project:
   ```sh
   idf.py build
   ```

   The first build will automatically download managed components (M5Dial BSP and dependencies).

## Flashing

Connect your M5Stack Dial via USB-C, then:

```sh
idf.py flash monitor
```

Press `Ctrl+]` to exit the serial monitor.

## Debugging

The M5Stack Dial supports USB Serial/JTAG debugging. In separate terminals:

```sh
# Terminal 1: Start OpenOCD
idf.py openocd

# Terminal 2: Start GDB
idf.py gdbtui
```

Or combined:
```sh
idf.py openocd gdbtui monitor
```

## Project Structure

```
├── main/
│   ├── tea_timer.c    # Application entry point and event loop
│   ├── view.c/h       # LVGL UI rendering
│   ├── logic.c/h      # Timer state machine
│   └── buzzer.c/h     # Buzzer driver
├── components/        # Local components
└── sdkconfig.defaults # Build configuration
```

## Links

- [M5Stack Dial v1.1 Documentation](https://docs.m5stack.com/en/core/M5Dial%20V1.1)
- [espressif/m5dial BSP](https://components.espressif.com/components/espressif/m5dial/versions/3.0.1/readme?language=en)
- [LVGL Documentation](https://docs.lvgl.io/master/)
