# Hardware Reference — Xiaomiao Console

## Architecture

```
┌──────────┐   USB   ┌──────────┐   UART0   ┌──────────────┐
│   PC     │◄──────►│  GD32    │◄─────────►│    ESP32     │
│ esptool  │        │ F350G8   │  115200   │  WROVER-B    │
└──────────┘        │ USB-CDC  │           │  4MB Flash   │
                    │ bridge   │  I2C      │  8MB PSRAM   │
                    │ +LED     │◄─────────►│              │
                    │ +Motor   │           │  ST7735 LCD  │
                    │ +Reset   │           │  MicroSD     │
                    │  ctrl    │           │  6 Keys      │
                    └──────────┘           │  Buzzer      │
                        │                  │  MPU6050     │
                   ┌────┴────┐              └──────────────┘
                   │ LEDs    │
                   │ Motors  │
                   └─────────┘
```

## ESP32 Pin Assignment (Complete)

| GPIO | Function | Notes |
|------|----------|-------|
| 0 | **LCD Backlight** | **Active-low backlight control** (NEW) |
| 2 | Button UP | Active-low, internal pull-up |
| 4 | LCD DC | Data/Command select |
| 5 | LCD CS | SPI2 chip select |
| 12 | Button B (ESC) | Active-low, internal pull-up |
| 13 | Button DOWN | Active-low, internal pull-up |
| 14 | Buzzer | Passive, LEDC Timer0/Ch0 |
| 15 | I2C SCL | Shared with UART1 TX (SugarASR) |
| 18 | LCD SCLK | SPI2 clock |
| 19 | LCD MISO | SPI2 MISO (shared with SD) |
| 21 | I2C SDA | Shared with UART1 RX (SugarASR) |
| 22 | SD CS | SPI2 chip select for MicroSD |
| 23 | LCD MOSI | SPI2 MOSI (shared with SD) |
| 25 | Extension OUT1 | LEDC Timer1/Ch1 PWM output |
| 26 | Extension OUT2 | LEDC Timer1/Ch2 PWM output |
| 27 | Button LEFT | Active-low, internal pull-up |
| 32 | Extension IN1 | ADC1 Ch4, digital input |
| 33 | Extension IN2 | ADC1 Ch5, digital input |
| 34 | Button A (ENTER) / **Battery ADC** | Active-low, **input-only, no pull-up**; also ADC1 Ch6 via voltage divider (9.1k + 2.4k) |
| 35 | Button RIGHT | Active-low, **input-only, no pull-up** |
| 36 | Light sensor | ADC1 Ch0 |
| 39 | Temp sensor | ADC1 Ch3 |

**Note**: GPIO34/35 have no internal pull resistors. External pull-up required
on hardware. When configuring buttons, skip pull-up for these two pins.

## LCD — ST7735 SPI TFT

- **Controller**: ST7735 Black Tab
- **Native panel**: 128x160, rotated 90° to **160x128** (landscape)
- **SPI bus**: SPI2_HOST @ **60 MHz** (shared with SD card)
- **Color**: RGB565, byte-swapped (`LV_COLOR_FORMAT_RGB565_SWAPPED`)
- **MADCTL**: `MX | MV | RGB` (0x60) for 90° rotation

### Backlight Control (NEW)

- **GPIO0**: Active-low backlight control
- **Logic**: Low = ON, High = OFF
- **Initialization**: Set GPIO0 as output and drive LOW to enable backlight

```c
// Initialize backlight GPIO
gpio_config_t bl_cfg = {
    .pin_bit_mask = 1ULL << GPIO_NUM_0,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
gpio_config(&bl_cfg);
gpio_set_level(GPIO_NUM_0, 0);  // Turn on backlight (active-low)

// To turn off: gpio_set_level(GPIO_NUM_0, 1);
// To turn on:  gpio_set_level(GPIO_NUM_0, 0);
```

### SPI Bus Init

```c
spi_bus_config_t buscfg = {
    .sclk_io_num = 18, .mosi_io_num = 23, .miso_io_num = 19,
    .quadwp_io_num = -1, .quadhd_io_num = -1,
    .max_transfer_sz = 160 * 128 * 2,  // full-screen frame
};
spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

esp_lcd_panel_io_spi_config_t io_cfg = {
    .dc_gpio_num = 4, .cs_gpio_num = 5,
    .pclk_hz = 60 * 1000000,
    .lcd_cmd_bits = 8, .lcd_param_bits = 8,
    .spi_mode = 0, .trans_queue_depth = 10,
};
esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io);
```

### ST7735 Init Sequence

```c
// Full init: SWRESET → SLPOUT → FRMCTR → PWCTR → VMCTR →
//            MADCTL → COLMOD → CASET/RASET → GAMMA → NORON →
//            MADCTL(rot90) → clear(black)
```

The complete register sequence is in `assets/main-template.c` function
`st7735_init_black_tab_rot90()`. Copy it verbatim — it matches the
MicroPython `init(2)` sequence from the original firmware.

### Flush Callback

```c
static void lvgl_flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px) {
    esp_lcd_panel_io_handle_t io = lv_display_get_user_data(d);
    // Send CASET, RASET, then RAMWR with pixel data
}
```

## MicroSD Card

- **Bus**: SDSPI on shared SPI2_HOST
- **CS**: GPIO22
- **Max freq**: 10 MHz (`SD_SPI_MAX_FREQ_KHZ = 10000`)
- **Mount point**: `/sdcard`
- **VFS**: FAT filesystem, up to 4 open files

```c
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
host.slot = SPI2_HOST;
host.max_freq_khz = 10000;
sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
slot.host_id = SPI2_HOST;
slot.gpio_cs = 22;
slot.wait_for_miso = 20;
esp_vfs_fat_mount_config_t mcfg = VFS_FAT_MOUNT_DEFAULT_CONFIG();
mcfg.format_if_mount_failed = false;
mcfg.max_files = 4;
esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mcfg, &card);
```

## 6-Key Keypad

| Key | GPIO | LVGL Key | Detection Method | Notes |
|-----|------|----------|------------------|-------|
| UP | 2 | LV_KEY_UP | Digital GPIO | Active-low, internal pull-up |
| DOWN | 13 | LV_KEY_DOWN | Digital GPIO | Active-low, internal pull-up |
| LEFT | 27 | LV_KEY_LEFT | Digital GPIO | Active-low, internal pull-up |
| RIGHT | 35 | LV_KEY_RIGHT | Digital GPIO | **Input-only pin**, no pull-up |
| **A** | **34** | **LV_KEY_ENTER** | **ADC (CH6)** | **Via voltage divider + battery monitor** (NEW) |
| B | 12 | LV_KEY_ESC | Digital GPIO | Active-low, internal pull-up |

All keys are **active-low**. Software debounce: 25ms.

### Button A - ADC Detection (NEW)

Button A uses **ADC1 Channel 6** for detection instead of digital GPIO:

**Circuit**: Battery voltage divider (9.1kΩ + 2.4kΩ) shares GPIO34 with Button A

**Detection Logic**:
- **Button pressed**: ADC reads lower voltage (button pulls to GND through divider)
- **Button released**: ADC reads battery voltage (through divider)

**Threshold Calculation**:
```
When button pressed: GPIO34 → GND
  ADC_raw ≈ 0 (or very low value)

When button released: GPIO34 → Vbat through divider
  ADC_raw = (Vbat × 2.4/11.5) / 3.3 × 4095
  
For Vbat = 3.7V (typical):
  ADC_raw ≈ (3.7 × 0.209) / 3.3 × 4095 ≈ 963
```

**Recommended threshold**: ADC_raw < 200 → Button pressed

```c
// Button A detection via ADC
bool is_button_a_pressed(adc_oneshot_unit_handle_t adc_handle) {
    int raw;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &raw);
    if (ret != ESP_OK) {
        return false;
    }
    
    // Threshold: if ADC value is very low, button is pressed
    return (raw < 200);
}

// Combined function: read battery OR detect button
typedef enum {
    BTN_A_RELEASED,
    BTN_A_PRESSED,
    ADC_READ_ERROR
} btn_a_state_t;

btn_a_state_t read_button_a_and_battery(adc_oneshot_unit_handle_t adc_handle, 
                                         float *out_vbat) {
    int raw;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &raw);
    if (ret != ESP_OK) {
        return ADC_READ_ERROR;
    }
    
    // Check if button is pressed (low ADC value)
    if (raw < 200) {
        return BTN_A_PRESSED;
    }
    
    // Button not pressed, calculate battery voltage
    if (out_vbat) {
        *out_vbat = (raw / 4095.0) * 3.3 * (11.5 / 2.4);
    }
    
    return BTN_A_RELEASED;
}
```

**Important Notes**:
1. Button A detection and battery monitoring share the same ADC channel
2. Cannot distinguish between "button pressed" and "very low battery" without additional logic
3. Recommended: Sample periodically and track trends for better accuracy
4. Consider adding hysteresis to avoid false triggers near threshold

## I2C Bus (I2C_NUM_0)

- **SCL**: GPIO15, **SDA**: GPIO21
- **Freq**: 100 kHz
- **Timeout**: 30ms

### Devices

| Address | Device | Purpose |
|---------|--------|---------|
| 0x40 | GD32F350 | LED control, motor PWM |
| 0x68 | MPU6050 | 3-axis accelerometer + gyroscope |

### GD32 Protocol (0x40)

**LED control**:
- Register 0xA0: LED1 on/off (write 1 byte: 0 or 1)
- Register 0xA1: LED2 on/off

```c
uint8_t cmd[] = {0xA0, 1};  // LED1 on
i2c_master_transmit(dev, cmd, sizeof(cmd), 30);
```

**Motor control**:
- Register 0x0E: Motor 1 speed (write 1 byte: `speed << 4`, 0-15 range)
- Register 0x06: Motor 2 speed
- Direction: bit 3 of the shifted value

### MPU6050 (0x68)

- WHO_AM_I register 0x75 → value 0x68
- PWR_MGMT_1 register 0x6B → write 0 to wake up
- Accel data: register 0x3B, 6 bytes, big-endian int16 (X/Y/Z)
- Gyro data: register 0x43, 6 bytes, big-endian int16 (X/Y/Z)

## ADC (ADC_UNIT_1)

| Channel | GPIO | Function |
|---------|------|----------|
| 0 | 36 | Light sensor |
| 3 | 39 | Temperature sensor |
| 4 | 32 | Extension IN1 |
| 5 | 33 | Extension IN2 |
| **6** | **34** | **Battery voltage (via divider)** (NEW) |

12-bit resolution, 12dB attenuation. Raw range 0-4095.

### Battery Voltage & Button A Detection (NEW)

GPIO34/ADC1_CH6 serves dual purpose: **Button A detection** and **battery voltage monitoring**

- **Voltage divider**: 9.1kΩ + 2.4kΩ (R1 + R2)
- **Button detection**: ADC reads low value when pressed, battery voltage when released
- **Battery formula**: `Vbat = ADC_raw * (3.3V / 4095) * ((9.1 + 2.4) / 2.4)`
- **Multiplier**: `(11.5 / 2.4) ≈ 4.79`
- **Button threshold**: ADC_raw < 200 → Button pressed

```c
// Configure ADC for button A and battery monitoring
adc_oneshot_unit_handle_t adc_handle;
adc_oneshot_unit_init_cfg_t init_cfg = {
    .unit_id = ADC_UNIT_1,
};
adc_oneshot_new_unit(&init_cfg, &adc_handle);

adc_oneshot_chan_cfg_t chan_cfg = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
};
adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &chan_cfg);

// Read and interpret ADC value
int raw;
adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &raw);

if (raw < 200) {
    // Button A is pressed
    handle_button_a_press();
} else {
    // Button A is released, read battery voltage
    float vbat = (raw / 4095.0) * 3.3 * (11.5 / 2.4);
    update_battery_indicator(vbat);
}
```

**Important Notes**:
1. Single ADC channel handles both functions - no conflict
2. Low ADC value = button pressed; high ADC value = battery voltage
3. Threshold may need calibration based on actual hardware
4. Add hysteresis to avoid false triggers near threshold boundary

## Buzzer (GPIO14)

```c
ledc_timer_config_t timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = freq,        // e.g. 988 for B5
    .duty_resolution = LEDC_TIMER_8_BIT,
    .clk_cfg = LEDC_AUTO_CLK,
};
ledc_timer_config(&timer);
ledc_channel_config_t chan = {
    .gpio_num = 14,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0,
    .duty = 128,            // 50% duty
    .hpoint = 0,
};
ledc_channel_config(&chan);
// Stop: ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
```

## GD32 USB-UART Bridge Reset Control

The GD32 controls ESP32's EN (PA4) and IO0 (PA0) pins:

| DTR | RTS | Action |
|-----|-----|--------|
| 1 | 0 | Enter download mode (IO0 low, EN pulse) |
| 0 | 1 | Hardware reset (EN low) |
| 0 | 0 | Normal release |

This is transparent to firmware — esptool handles it automatically via the
USB-CDC interface.

## Extension IO

- **GPIO25/26**: PWM output via LEDC Timer1, channels 1/2. 1000Hz, 8-bit duty.
- **GPIO32/33**: Digital input or ADC (channels 4/5).

## Hardware Limitations

1. **No anti-tearing**: TE pin not connected to ESP32
2. **Backlight control via GPIO0**: Now supports software brightness control (active-low)
3. **SPI bus contention**: LCD and SD share SPI2 — interleaved access only
4. **GPIO34/35**: Input-only, no internal pull resistors
5. **UART1 conflict**: GPIO15/21 shared between I2C and SugarASR voice module
6. **Button A via ADC**: Button A detection uses ADC threshold; may need calibration for different battery levels
7. **GPIO0 download mode conflict**: GPIO0 used for backlight; ensure it's HIGH during firmware flashing (download mode requires IO0 low)
