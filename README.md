# ESP32 Touchscreen PZEM-004T Dashboard

A high-performance, modular metrics dashboard for ESP32 designed to display real-time sensor information (Voltage, Current, Energy, Power) retrieved from local HTTP Prometheus endpoints (such as ESPHome or Home Assistant devices).

The display driver is powered by **LovyanGFX** interfacing with an **ST7789** controller over a high-speed parallel bus, with **LVGL v8.3.11** serving as the graphical user interface framework.

---

## Key Features

* **Layered Modular Architecture**: Decoupled design separating the UI rendering loops, background networking threads, and low-level driver implementations.
* **Core-0 Asynchronous Metrics Fetch**: Metrics collection is offloaded to a background FreeRTOS task pinned to ESP32 **Core 0**, using thread-safe Mutex synchronization. This leaves **Core 1** dedicated exclusively to the UI, guaranteeing a smooth GUI refresh rate with zero network-induced lag.
* **3-Point Affine Touch Calibration**: Integrates a visual 4-point calibration utility on boot that solves for the 6 coefficients of an affine transformation matrix. It handles rotated, swapped, or mirrored axes automatically.
* **NVS Persistent Calibration**: Solved calibration coefficients are saved persistently to ESP32 Non-Volatile Storage (NVS) using the `Preferences` library. Calibration is only required on the very first boot.
* **Flicker-Free Display Tuning**: Customized LVGL refresh periods and a reduced parallel bus frequency (20 MHz) optimized for signal integrity on prototype wiring.
* **Prometheus Streaming Optimization**: A fast HTTP parsing routine utilizing `getString()` to cleanly detect packet completion, preventing Keep-Alive socket delays.

---

## Architecture Overview

```mermaid
graph TD
    A[main.cpp Coordinator] --> B[Display_LGFX Driver]
    A --> C[Touch_Calibrator]
    A --> D[Network_Manager]
    A --> E[Dashboard_UI]
    D --> F[Remote_Metrics Parser]
    C --> G[CST820 Touch Library]
```

### Component Structure
* **`src/main.cpp`**: Main entry point coordinating hardware initialization, loading/running calibration, and driving the LVGL main handler loop.
* **`include/Display_LGFX.h` / `src/Display_LGFX.cpp`**: LovyanGFX display configuration for the ST7789 panel running on an 8-bit parallel bus.
* **`include/Touch_Calibrator.h` / `src/Touch_Calibrator.cpp`**: 4-corner calibration UI and NVS storage manager.
* **`include/Network_Manager.h` / `src/Network_Manager.cpp`**: WiFi auto-reconnect coordinator and asynchronous FreeRTOS network task.
* **`include/Remote_Metrics.h` / `src/Remote_Metrics.cpp`**: Text-based Prometheus HTTP client parser.
* **`include/Dashboard_UI.h` / `src/Dashboard_UI.cpp`**: LVGL UI layout creating the 2x2 grid card interfaces and fullscreen zoom cards.
* **`lib/CST820/`**: Statically isolated private PlatformIO driver library for the CST820 touchpad controller.

---

## Hardware Configuration & Pin Map

### ST7789 MCU8080 8-Bit Parallel Display
| Signal | ESP32 GPIO | Description |
|--------|------------|-------------|
| `WR`   | GPIO 4     | Write Clock |
| `RD`   | GPIO 2     | Read Clock |
| `RS`   | GPIO 16    | Register Select (DC) |
| `CS`   | GPIO 17    | Chip Select |
| `RST`  | Unused     | Hardware Reset |
| `D0`   | GPIO 15    | Data Bit 0 |
| `D1`   | GPIO 13    | Data Bit 1 |
| `D2`   | GPIO 12    | Data Bit 2 |
| `D3`   | GPIO 14    | Data Bit 3 |
| `D4`   | GPIO 27    | Data Bit 4 |
| `D5`   | GPIO 25    | Data Bit 5 |
| `D6`   | GPIO 33    | Data Bit 6 |
| `D7`   | GPIO 32    | Data Bit 7 |

### CST820 Capacitive Touch Controller (I2C)
| Signal | ESP32 GPIO | Description |
|--------|------------|-------------|
| `SDA`  | GPIO 21    | I2C Data Line |
| `SCL`  | GPIO 22    | I2C Clock Line |
| `RST`  | Unused     | Reset Pin |
| `INT`  | Unused     | Interrupt Pin |

---

## Installation & Setup

1. **Prerequisites**: Install [VSCode](https://code.visualstudio.com/) and the [PlatformIO IDE](https://platformio.org/) extension.
2. **WiFi & URL Configuration**: Update the credentials and target Prometheus endpoint in [src/main.cpp](file:///c:/Users/marco/Documentos/PlatformIO/Projects/esp32_touchscreen/src/main.cpp#L14-L17):
   ```cpp
   #define WIFI_SSID "YOUR_SSID"
   #define WIFI_PASS "YOUR_PASSWORD"
   #define METRICS_URL "http://YOUR_DEVICE_IP/metrics"
   ```
3. **Partition Table**: Since the project integrates LVGL fonts, the binary size exceeds the standard 1.2MB partition. The [platformio.ini](file:///c:/Users/marco/Documentos/PlatformIO/Projects/esp32_touchscreen/platformio.ini) is configured with `board_build.partitions = huge_app.csv` to allocate **3.0 MB** for the program, ensuring compilation succeeds out-of-the-box.

---

## Compilation & Flashing

Use PlatformIO shortcut commands in VSCode to build and deploy:
* **Build**: `ctrl` + `alt` + `b`
* **Upload**: `ctrl` + `alt` + `u`
* **Monitor**: `ctrl` + `alt` + `s` (Set to `115200` baud)
