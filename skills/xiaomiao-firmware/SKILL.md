---
name: xiaomiao-firmware
description: >-
  Create ESP32 firmware (ROM) applications for the Xiaomiao (Xueersi) educational
  handheld console — an ESP32-WROVER-B with ST7735 TFT (160x128), MicroSD, 6-key
  keypad, buzzer, I2C MPU6050 gyro, and GD32 co-processor. Use this skill
  whenever the user is writing embedded firmware for this device, creating games
  or tools that target it, or mentioning any of: xiaomiao, xueersi, xueersi-idf,
  xiao miao, littlecat, 小猫, 猫猫掌机, or 学而思掌机. Also trigger when the
  user asks to build ESP32 firmware with LVGL UI on a small TFT display with
  physical buttons, or needs Loader/OTA partition management for a
  factory+ota_0 layout, even if they don't name the device explicitly.
license: MIT
compatibility: >-
  Requires ESP-IDF v5.5.4, xtensa-esp-elf toolchain, cmake, ninja. Python 3.12+.
  LVGL 9.5.0 via ESP-IDF component manager.
metadata:
  author: xueersi-idf
  version: "1.0"
  target: esp32-wrover-b
  idf_version: "5.5.4"
---

# Xiaomiao Console Firmware Development

Create new firmware applications for the Xiaomiao educational handheld:
**ESP32-WROVER-B** (4MB flash, 8MB PSRAM) + **GD32F350G8** co-processor.

## What This Skill Must Deliver

When triggered, produce firmware work that is immediately useful on Xiaomiao:

1. A Loader-compatible ESP-IDF ROM project, patch, or explanation.
2. Code that matches this board's fixed pins, memory limits, partition layout,
   and LVGL 9.5.0 patterns.
3. A short verification path: build command, flash path, or hardware check.
4. Clear handling for missing hardware, missing ESP-IDF, oversized binaries, or
   unavailable MicroSD.

Do not treat Xiaomiao as generic ESP32. Display, buttons, SPI bus, partition
layout, and return-to-loader behavior are device-specific.

## Operating Workflow

Use this workflow for firmware creation, review, or debugging:

| Step | Input | Action | Output |
|---|---|---|---|
| 1. Identify target | User request, existing files | Decide: new ROM, patch, or diagnosis | One-sentence target and constraints |
| 2. Lock board assumptions | Device summary below plus `references/hardware.md` | Apply the Xiaomiao pins, ST7735 display, SPI2 sharing, 4MB flash, 8MB PSRAM, and Loader partition layout | No generic ESP32 defaults leak into code |
| 3. Pick implementation base | Empty project, existing project, or scaffold script | Use `scripts/new-rom.sh` for new projects; use existing project style for patches | Minimal file set or focused diff |
| 4. Enforce Loader contract | `app_main()`, partition table, `return_to_loader.h` | Put `return_to_loader_setup()` as the first executable line in `app_main()` and use `assets/partitions.csv` | ROM returns to Loader after reset |
| 5. Add feature code | User's game/tool/peripheral request | Reuse templates and snippets in this skill; keep code small enough for ota_0 | Buildable firmware logic |
| 6. Verify | Local ESP-IDF if available, otherwise static checks | Run or describe `idf.py build`; check binary size and likely hardware conflicts | Build result or explicit dry-run notes |

### 🔴 CHECKPOINT: Before Changing an Existing Project

Pause and inspect before editing when any of these are true:

- The repository already has a custom partition table.
- `app_main()` already performs boot or OTA selection.
- LCD, SD, or I2C code already exists and may conflict with these templates.
- The requested change may exceed the 3.25MB ota_0 app limit.

At this checkpoint, state the detected constraint and then make the smallest
safe change. Do not rewrite the whole firmware unless the existing structure is
broken.

## Device Summary

| Component | Details |
|-----------|---------|
| MCU | ESP32-WROVER-B, 240MHz dual-core |
| Flash | 4MB QIO 80MHz |
| PSRAM | 8MB Quad SPI (VSPI) |
| Display | ST7735 SPI TFT, 160x128 (rotated 90), 60MHz SPI2 |
| Storage | MicroSD on shared SPI2, CS=GPIO22 |
| Input | 6 keys (UP/DOWN/LEFT/RIGHT/A/B), active-low |
| Audio | Passive buzzer on GPIO14 (LEDC) |
| I2C | GD32 @0x40 (LED/motor), MPU6050 @0x68 (gyro) |
| USB | GD32 USB-CDC bridge, auto-reset via DTR/RTS |
| UI Framework | LVGL 9.5.0 |

## Quick Start: New ROM Project

Run the scaffold script:

```bash
scripts/new-rom.sh my-game ~/projects/my-game
```

Or create manually. A ROM project needs:

```
my-game/
├── CMakeLists.txt          # Top-level ESP-IDF project
├── sdkconfig.defaults      # Board config (see assets/sdkconfig.defaults)
├── main/
│   ├── CMakeLists.txt      # Component registration
│   ├── main.c              # Firmware code
│   └── idf_component.yml   # Dependencies: lvgl/lvgl 9.5.0
└── return_to_loader.h      # Drop-in header for Loader integration
```

**Critical**: `app_main()` must call `return_to_loader_setup()` as the very
first line — this makes Reset/power-cycle return to the Loader instead of
re-entering your ROM.

If the scaffold script is unavailable, recreate the structure from `assets/`.
If ESP-IDF is missing, generate the project and mark verification as a dry run.

## Build & Flash

```bash
# Set up ESP-IDF environment
. ~/esp/esp-idf/export.sh

# Build
idf.py build

# Flash to device (via GD32 USB-UART bridge)
idf.py -p /dev/ttyACM0 flash

# Or flash the merged binary with esptool
esptool.py --chip esp32 -b 460800 write_flash 0x0 build/my-game-merged.bin
```

## Loader Integration

The device runs a **ROM Loader** in the factory partition. The Loader:
1. Scans `/sdcard/roms/*.bin` on the TF card
2. Shows a selection list on screen
3. Writes the selected ROM's app image to the ota_0 partition
4. Reboots into the ROM

Your ROM must:
- Include `return_to_loader.h` and call `return_to_loader_setup()` first
- Be compiled with the Loader's partition table (`assets/partitions.csv`)
- App image must fit in ota_0 (3.25MB max)

If the ROM is already in ota_0, the Loader **skips the flash write** and boots
directly — no unnecessary flash wear.

For full Loader architecture details, see [references/loader.md](references/loader.md).

### 🔴 CHECKPOINT: Before Advising Partition Changes

Changing `assets/partitions.csv` affects the Loader and every ROM. Only make
partition changes after checking:

- Current app size from `idf.py size` or `idf.py size-components`
- Whether large assets can move to MicroSD
- Whether unused LVGL fonts, examples, or debug options can be removed
- Whether the Loader itself must be re-flashed

Prefer shrinking the ROM first. Partition edits are the last resort.

## Hardware Reference

Detailed pin assignments, LCD init sequence, I2C protocols, ADC channels:
[references/hardware.md](references/hardware.md)

Key pins (memorize these):

```
LCD:  SCLK=18  MOSI=23  MISO=19  CS=5  DC=4   (SPI2 @ 60MHz)
SD:   CS=22    (shared SPI2)
Keys: UP=2  DOWN=13  LEFT=27  RIGHT=35  A=34  B=12  (active-low)
I2C:  SCL=15  SDA=21  (100kHz)
Buzz: GPIO14 (LEDC ch0)
```

GPIO34/35 are input-only (no pull-up). Other buttons need internal pull-up.

## LVGL UI Patterns

Display init uses triple full-screen DMA buffers at 60fps. Input is a 6-key
keypad. For specific code patterns (focus styling, screen switching, progress
bars): [references/lvgl-ui.md](references/lvgl-ui.md)

## Template Files

| File | Purpose |
|------|---------|
| `assets/main-template.c` | Complete main.c skeleton with all hardware init |
| `assets/CMakeLists.txt` | Top-level ESP-IDF project CMakeLists |
| `assets/main_CMakeLists.txt` | Component CMakeLists for main/ |
| `assets/sdkconfig.defaults` | Board configuration defaults |
| `assets/partitions.csv` | Loader-compatible partition table |
| `assets/return_to_loader.h` | Return-to-Loader drop-in header |

## Common Tasks

The template in `assets/main-template.c` includes LCD, buttons, and LVGL init.
The tasks below add peripherals that aren't in the template. Each is a complete
drop-in you can paste into `app_main()` after `return_to_loader_setup()`.

### Mount SD card (SDSPI on SPI2)

```c
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
host.slot = SPI2_HOST;
host.max_freq_khz = 10000;

sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
slot.host_id = SPI2_HOST;
slot.gpio_cs = GPIO_NUM_22;
slot.wait_for_miso = 20;

esp_vfs_fat_mount_config_t mcfg = {
    .format_if_mount_failed = false,
    .max_files = 4,
    .allocation_unit_size = 16 * 1024,
};
sdmmc_card_t *card;
esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mcfg, &card);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(ret));
}

// Usage:
FILE *f = fopen("/sdcard/data.txt", "r");
char line[128];
while (fgets(line, sizeof(line), f)) { /* ... */ }
fclose(f);
```

### Buzzer (GPIO14, LEDC)

```c
#include "driver/ledc.h"

static void buzzer_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 440,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t chan = {
        .gpio_num = 14,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&chan);
}

// Play a tone at freq Hz for duration_ms. This blocks the current task; call it
// from a short-lived audio task or timer-driven state machine if UI must stay responsive.
static void buzzer_tone(uint32_t freq, uint32_t duration_ms) {
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
```

### Initialize I2C bus

Both MPU6050 and GD32 share I2C_NUM_0 (SCL=15, SDA=21, 100kHz). Init once:

```c
#include "driver/i2c_master.h"

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_mpu6050, s_gd32;

static void i2c_init(void) {
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_15,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };
    i2c_new_master_bus(&cfg, &s_i2c_bus);

    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,
    };
    dev.device_address = 0x68;
    i2c_master_bus_add_device(s_i2c_bus, &dev, &s_mpu6050);
    dev.device_address = 0x40;
    i2c_master_bus_add_device(s_i2c_bus, &dev, &s_gd32);
}
```

### Read MPU6050 accelerometer (0x68)

```c
static void mpu6050_init(void) {
    uint8_t buf[2];
    buf[0] = 0x6B; buf[1] = 0x00;  // wake up
    i2c_master_transmit(s_mpu6050, buf, 2, 30);
}

static void mpu6050_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t reg = 0x3B, data[6];
    i2c_master_transmit_receive(s_mpu6050, &reg, 1, data, 6, 30);
    *ax = (int16_t)(data[0] << 8 | data[1]);
    *ay = (int16_t)(data[2] << 8 | data[3]);
    *az = (int16_t)(data[4] << 8 | data[5]);
}
```

### Control GD32 LEDs and motors (0x40)

```c
static void gd32_led(bool on) {
    uint8_t cmd[] = {0xA0, on ? 1 : 0};  // LED1
    i2c_master_transmit(s_gd32, cmd, sizeof(cmd), 30);
    cmd[0] = 0xA1;                         // LED2
    i2c_master_transmit(s_gd32, cmd, sizeof(cmd), 30);
}

static void gd32_motor(uint8_t speed) {
    // speed: 0-15 (direction via bit 3 of shifted value)
    uint8_t cmd[] = {0x0E, (uint8_t)(speed << 4)};
    i2c_master_transmit(s_gd32, cmd, sizeof(cmd), 30);
}
```

### Save/load settings with NVS

```c
#include "nvs_flash.h"
#include "nvs.h"

// Init (in app_main):
nvs_flash_init();

// Save:
nvs_handle_t handle;
nvs_open("config", NVS_READWRITE, &handle);
nvs_set_i32(handle, "highscore", 9999);
nvs_commit(handle);
nvs_close(handle);

// Load:
int32_t score = 0;
nvs_open("config", NVS_READONLY, &handle);
nvs_get_i32(handle, "highscore", &score);
nvs_close(handle);
```

## Troubleshooting

Use this table before code changes. It separates hardware failures, config
mistakes, and build environment problems.

| Symptom | First fix | If still failing |
|---|---|---|
| Build cannot find LVGL | `main/idf_component.yml` contains `lvgl/lvgl: "9.5.0"` and component manager is enabled | Run `idf.py reconfigure`; if offline, pre-download components on a connected machine |
| Binary too large | `idf.py size-components` | Remove unused LVGL fonts/assets; move media to SD; only then discuss partition changes |
| Screen black | ST7735 init order, DISPON delay, CS=5, DC=4 | Verify SPI2 bus setup and MADCTL landscape config |
| Screen garbled | `LV_COLOR_FORMAT_RGB565_SWAPPED` | Recheck CASET/RASET dimensions and DMA buffer sizes |
| SD mount timeout | Keep SD and LCD on SPI2 and verify both CS lines idle high | Mount SD before LCD init, or lower SD clock to 10MHz |
| Buttons fail | Active-low logic and GPIO34/35 external pull-ups | Check LVGL input callback and keypad group |
| Reset returns to ROM | `return_to_loader_setup()` is missing or not first | Move it to the first executable line of `app_main()` |
| Flashing fails | `/dev/ttyACM0` exists and cable supports data | Lower baud to 115200 and check DTR/RTS auto-reset |

### Display stays black after init
- ST7735 needs a delay after DISPON (20ms minimum). The template handles this.
- Verify MADCTL: the second write (`MX | MV | RGB`) sets landscape rotation. If colors are inverted (red↔blue), swap `MADCTL_RGB` to `MADCTL_BGR`.
- Check that CS (GPIO5) and DC (GPIO4) aren't swapped. DC must be high for data, low for command.
- SPI bus init must happen before LCD init. The template does this correctly.

### SD card fails to mount
- **SPI bus contention**: LCD and SD share SPI2. The LCD init leaves CS high, but SDSPI driver expects exclusive bus access. If SD fails after LCD init, try mounting SD *before* `lcd_init()`. The template mounts SD last — it works in practice but if you see `ESP_ERR_TIMEOUT`, swap the order.
- Verify CS=GPIO22 isn't being driven by anything else.
- If no card is inserted, `esp_vfs_fat_sdspi_mount` returns an error — handle it gracefully instead of aborting.

### Build fails: "lvgl not found" or component manager errors
- Make sure `main/idf_component.yml` has `lvgl/lvgl: "9.5.0"`.
- The ESP-IDF component manager downloads LVGL on first build. Internet connection required. If offline, pre-download: `idf.py reconfigure` on a connected machine first.
- Some ESP-IDF SDK configs disable the component manager. Check `sdkconfig.defaults` doesn't have `CONFIG_IDF_COMPONENT_MANAGER=n`.

### ROM binary exceeds ota_0 partition (3.25MB)
- Check what's taking space: `idf.py size-components`
- Common culprits: LVGL fonts (only enable needed sizes), embedded assets (store on SD), debug symbols (use release build).
- If you need more space, reduce ota_0 offset (must stay 64KB-aligned) or shrink factory partition. Both require updating `partitions.csv` and re-flashing the loader.

### Buttons not responding
- GPIO34 (A) and GPIO35 (RIGHT) are input-only with no internal pull-up. The hardware has external pull-ups. If these buttons don't work, check the external resistors.
- All buttons are active-low. `gpio_get_level()` returns 0 when pressed.
- If all buttons are unresponsive, check `lv_indev_set_read_cb` was called and the keypad group was set as default.
- Software debounce is 25ms — if buttons feel laggy, reduce `BUTTON_DEBOUNCE_MS`.

### `idf.py flash` fails or device not found
- The GD32 acts as a USB-UART bridge. Check `ls /dev/ttyACM*` — it should appear as `/dev/ttyACM0`.
- If no device appears: check USB cable (some are power-only), try a different port.
- Some Linux distros require dialout group: `sudo usermod -aG dialout $USER`.
- The GD32 auto-resets ESP32 via DTR/RTS. If flashing starts but fails mid-way, try lowering baud rate: `idf.py -p /dev/ttyACM0 flash -b 115200`.

### LVGL display is garbled or shifted
- Check `LV_COLOR_FORMAT_RGB565_SWAPPED` — ST7735 expects byte-swapped RGB565.
- If the image is offset or clipped, verify CASET/RASET in `flush_cb` match the display dimensions (160x128 landscape).
- If only part of the screen updates, check DMA buffer sizes: all three buffers must be `stride * LCD_V_RES` bytes.

## Pitfalls — Device-Specific Constraints

These are the things generic ESP32 knowledge will get wrong. Double-check
against this list when writing firmware.

| Pitfall | Wrong Assumption | Correct |
|---------|-----------------|---------|
| Display | ILI9341 / ILI9488 / generic TFT | **ST7735 Black Tab** only. Init sequence is copy-paste from template. |
| LCD SPI bus | SPI3_HOST or VSPI | **SPI2_HOST** (shared with SD card). SPI3 doesn't exist in this config. |
| SD card + LCD sharing | Separate SPI buses | Both on **SPI2**. CS pins (GPIO5 for LCD, GPIO22 for SD) prevent conflict. SD max 10MHz. |
| Buzzer pin | GPIO25 / GPIO26 / random LEDC | **GPIO14** only. LEDC timer 0, channel 0. |
| I2C pins | GPIO21/22 (default ESP32 I2C) | **SCL=15, SDA=21**. GPIO21 is correct for SDA but SCL is 15, not 22. |
| Button pull-ups | All buttons need pull-up | GPIO34/35 are **input-only, no internal pull-up**. Must skip pullup for these two. |
| Partition table | Standard ESP-IDF partitions | Must use Loader-compatible: `factory(0x10000) + ota_0(0xC0000)`. ROM app goes in ota_0. |
| Return to Loader | Firmware boots directly on reset | Every ROM must call `return_to_loader_setup()` **as the first line** of app_main(). Otherwise reset drops back into the same ROM, not the Loader. |
| LVGL color format | Standard RGB565 | **RGB565_SWAPPED** — ST7735 expects byte-swapped color. Wrong format = garbled or inverted colors. |
| Display dimensions | 128x160 (portrait) | **160x128** (landscape, rotated 90° via MADCTL `MX\|MV\|RGB`). |
| Flash/PSRAM config | Default ESP32 settings | Must enable: QIO 80MHz flash, **8MB Quad PSRAM** on VSPI, 240MHz CPU. |
| LVGL buffer strategy | Single or double buffer | **Triple** DMA buffers (two for LVGL + one for SPI DMA in-flight). Single buffer causes tearing. |

### Most Common Root Cause

When things don't work on actual hardware, 90% of the time it's one of:
1. Wrong display controller (not ST7735)
2. Wrong buzzer pin (not GPIO14)
3. Missing `return_to_loader_setup()` as first line
4. Wrong partition table (missing factory+ota_0)
5. GPIO34/35 configured with pull-up

## Do Not Do These

These are hard blacklists for this device:

| Anti-pattern | Why it breaks | Correct action |
|---|---|---|
| Generate firmware for ILI9341, ILI9488, SSD1306, or generic TFT | Xiaomiao uses ST7735 Black Tab at 160x128 landscape | Use the LCD init sequence and flush pattern from `assets/main-template.c` |
| Put SD on another SPI bus | LCD and SD share SPI2 on this board | Use SPI2, LCD CS=5, SD CS=22 |
| Configure GPIO34/35 with internal pull-ups | ESP32 GPIO34/35 are input-only and have no internal pull-ups | Skip pull-up config for those pins and rely on hardware pull-ups |
| Skip `return_to_loader_setup()` | Reset will re-enter the ROM instead of going back to Loader | Call it first in `app_main()` |
| Use a standard ESP-IDF partition table | Loader expects factory + ota_0 layout | Copy `assets/partitions.csv` |
| Store large media in flash by default | ota_0 has only 3.25MB | Put media on `/sdcard` and stream or load on demand |
| Block the LVGL task with long delays | UI input and rendering will stall | Use short delays, timers, or background tasks |
| Abort when SD is missing | Not every ROM requires inserted storage | Handle mount failure gracefully unless storage is the core feature |
| Invent unverified GD32 commands | GD32 handles LEDs, motor, bridge behavior | Use only documented command bytes from this skill or references |

## Constraints

- Flash: 4MB total. App must fit in ota_0 (3.25MB).
- PSRAM: Available for data (heap_caps_malloc with MALLOC_CAP_SPIRAM). Cannot execute code from PSRAM on ESP32.
- TE (tearing) pin not connected to MCU — no vsync.
- Backlight hardwired to VCC — no brightness control.
- No OTA data partition in ROM's own view — use factory partition for persistent data via NVS.

## Output Style

For code-generation requests:

- Provide the exact files to create or modify.
- Keep code compatible with ESP-IDF v5.5.4 and LVGL 9.5.0.
- Include build and flash commands.
- Mention any dry-run limitation if hardware or ESP-IDF is unavailable.

For debugging requests:

- Start with the most likely root cause.
- Separate diagnosis from the fix.
- Give one hardware check and one software check when the symptom could be
  caused by either.

For reviews:

- Flag generic ESP32 assumptions first.
- Check Loader contract, partition table, display config, input pins, and binary
  size before style concerns.
