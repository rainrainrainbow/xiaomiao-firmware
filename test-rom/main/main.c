/*
 * Test ROM for Xiaomiao Console
 * Tests: Backlight control (GPIO0), Button A ADC detection, Battery monitoring
 * Full LCD/ST7735 initialization + LVGL display
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_lcd_panel_io.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"

#include "return_to_loader.h"

/* ── Hardware Constants ────────────────────────────────────────────────── */
#define LCD_HOST            SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ  (60 * 1000 * 1000)
#define LCD_NATIVE_H_RES    128
#define LCD_NATIVE_V_RES    160
#define LCD_H_RES           160
#define LCD_V_RES           128
#define LCD_DRAW_BUF_LINES  LCD_V_RES
#define LCD_DPI             60

#define PIN_LCD_SCLK   GPIO_NUM_18
#define PIN_LCD_MOSI   GPIO_NUM_23
#define PIN_LCD_MISO   GPIO_NUM_19
#define PIN_LCD_CS     GPIO_NUM_5
#define PIN_LCD_DC     GPIO_NUM_4

#define GPIO_BACKLIGHT      GPIO_NUM_0
#define GPIO_BTN_UP         GPIO_NUM_2
#define GPIO_BTN_DOWN       GPIO_NUM_13
#define GPIO_BTN_LEFT       GPIO_NUM_27
#define GPIO_BTN_RIGHT      GPIO_NUM_35
#define GPIO_BTN_A          GPIO_NUM_34  /* ADC channel 6 */
#define GPIO_BTN_B          GPIO_NUM_12

#define ADC_CHANNEL         ADC_CHANNEL_6
#define ADC_ATTEN           ADC_ATTEN_DB_12
#define BTN_A_THRESHOLD     200

#define LVGL_TICK_PERIOD_MS  1
#define LVGL_TASK_STACK      (10 * 1024)
#define LVGL_TASK_PRIORITY   5

#define UI_YELLOW  0xF6D34A
#define UI_BLACK   0x1B1713
#define UI_BROWN   0x5C4220
#define UI_RED     0xE64B3C
#define UI_CREAM   0xFFF3B0

/* ST7735 registers */
#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_NORON    0x13
#define ST7735_INVOFF   0x20
#define ST7735_DISPOFF  0x28
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_MADCTL   0x36
#define ST7735_COLMOD   0x3A
#define ST7735_FRMCTR1  0xB1
#define ST7735_FRMCTR2  0xB2
#define ST7735_FRMCTR3  0xB3
#define ST7735_INVCTR   0xB4
#define ST7735_PWCTR1   0xC0
#define ST7735_PWCTR2   0xC1
#define ST7735_PWCTR3   0xC2
#define ST7735_PWCTR4   0xC3
#define ST7735_PWCTR5   0xC4
#define ST7735_VMCTR1   0xC5
#define ST7735_GMCTRP1  0xE0
#define ST7735_GMCTRN1  0xE1

#define MADCTL_MX       0x40
#define MADCTL_MY       0x80
#define MADCTL_MV       0x20
#define MADCTL_RGB      0x00

static const char *TAG = "test_rom";

/* ── Globals ───────────────────────────────────────────────────────────── */
static lv_draw_buf_t          s_draw_buf3;
static esp_lcd_panel_io_handle_t s_lcd_io;
static volatile bool          s_first_flush;
static adc_oneshot_unit_handle_t adc_handle = NULL;

/* ── Backlight Control ─────────────────────────────────────────────────── */

static void backlight_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << GPIO_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(GPIO_BACKLIGHT, 0);  /* Turn on (active-low) */
    ESP_LOGI(TAG, "Backlight initialized on GPIO%d", GPIO_BACKLIGHT);
}

static void backlight_toggle(void)
{
    static bool on = true;
    on = !on;
    gpio_set_level(GPIO_BACKLIGHT, on ? 0 : 1);
    ESP_LOGI(TAG, "Backlight %s", on ? "ON" : "OFF");
}

/* ── Button A ADC Detection ────────────────────────────────────────────── */

static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg));
    ESP_LOGI(TAG, "ADC initialized on CH%d (GPIO%d)", ADC_CHANNEL, GPIO_BTN_A);
}

static bool button_a_is_pressed(void)
{
    int raw;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
    if (ret != ESP_OK) {
        return false;
    }
    return (raw < BTN_A_THRESHOLD);
}

static float read_battery_voltage(void)
{
    int raw;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
    if (ret != ESP_OK) {
        return -1.0f;
    }
    /* Vbat = ADC_raw * (3.3 / 4095) * (11.5 / 2.4) */
    return (raw / 4095.0f) * 3.3f * (11.5f / 2.4f);
}

/* ── Other Buttons (Digital GPIO) ──────────────────────────────────────── */

static void buttons_init(void)
{
    uint64_t mask = (1ULL << GPIO_BTN_UP) | (1ULL << GPIO_BTN_DOWN) |
                    (1ULL << GPIO_BTN_LEFT) | (1ULL << GPIO_BTN_RIGHT) |
                    (1ULL << GPIO_BTN_B);

    uint64_t pullup = mask;
    pullup &= ~(1ULL << GPIO_BTN_RIGHT);  /* GPIO35 is input-only */

    gpio_config_t io = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    if (pullup) {
        gpio_config_t pu = {
            .pin_bit_mask = pullup,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&pu);
    }
}

/* ── LCD / ST7735 ──────────────────────────────────────────────────────── */

static void st7735_tx(esp_lcd_panel_io_handle_t io, int cmd,
                      const void *param, size_t len)
{
    esp_lcd_panel_io_tx_param(io, cmd, param, len);
}

static void st7735_delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static void st7735_init(esp_lcd_panel_io_handle_t io)
{
    const uint8_t frmctr[]  = {0x01,0x2C,0x2D};
    const uint8_t frmctr3[] = {0x01,0x2C,0x2D,0x01,0x2C,0x2D};
    const uint8_t pwctr1[]  = {0xA2,0x02,0x84};
    const uint8_t pwctr2[]  = {0xC5};
    const uint8_t pwctr3[]  = {0x0A,0x00};
    const uint8_t pwctr4[]  = {0x8A,0x2A};
    const uint8_t pwctr5[]  = {0x8A,0xEE};
    const uint8_t madctl_d[] = {MADCTL_MX | MADCTL_MY | MADCTL_RGB};
    const uint8_t madctl_r[] = {MADCTL_MX | MADCTL_MV | MADCTL_RGB};
    const uint8_t colmod[] = {0x05};
    const uint8_t gp[] = {0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,
                          0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10};
    const uint8_t gn[] = {0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,
                          0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10};

    st7735_tx(io, ST7735_DISPOFF, NULL, 0);
    st7735_tx(io, ST7735_SWRESET, NULL, 0);
    st7735_delay(150);
    st7735_tx(io, ST7735_SLPOUT, NULL, 0);
    st7735_delay(500);
    st7735_tx(io, ST7735_FRMCTR1, frmctr, sizeof(frmctr));
    st7735_tx(io, ST7735_FRMCTR2, frmctr, sizeof(frmctr));
    st7735_tx(io, ST7735_FRMCTR3, frmctr3, sizeof(frmctr3));
    st7735_tx(io, ST7735_INVCTR, (uint8_t[]){0x07}, 1);
    st7735_tx(io, ST7735_PWCTR1, pwctr1, sizeof(pwctr1));
    st7735_tx(io, ST7735_PWCTR2, pwctr2, sizeof(pwctr2));
    st7735_tx(io, ST7735_PWCTR3, pwctr3, sizeof(pwctr3));
    st7735_tx(io, ST7735_PWCTR4, pwctr4, sizeof(pwctr4));
    st7735_tx(io, ST7735_PWCTR5, pwctr5, sizeof(pwctr5));
    st7735_tx(io, ST7735_VMCTR1, (uint8_t[]){0x0E}, 1);
    st7735_tx(io, ST7735_INVOFF, NULL, 0);
    st7735_tx(io, ST7735_MADCTL, madctl_d, sizeof(madctl_d));
    st7735_tx(io, ST7735_COLMOD, colmod, sizeof(colmod));
    st7735_tx(io, ST7735_CASET,
              (uint8_t[]){0,0,0,LCD_NATIVE_H_RES-1}, 4);
    st7735_tx(io, ST7735_RASET,
              (uint8_t[]){0,0,0,LCD_NATIVE_V_RES-1}, 4);
    st7735_tx(io, ST7735_GMCTRP1, gp, sizeof(gp));
    st7735_tx(io, ST7735_GMCTRN1, gn, sizeof(gn));
    st7735_tx(io, ST7735_NORON, NULL, 0);
    st7735_delay(10);
    st7735_tx(io, ST7735_MADCTL, madctl_r, sizeof(madctl_r));
}

static esp_lcd_panel_io_handle_t lcd_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t cfg = {
        .dc_gpio_num = PIN_LCD_DC, .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .spi_mode = 0, .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_HOST, &cfg, &io));

    s_lcd_io = io;
    st7735_init(io);
    return io;
}

/* ── LVGL ──────────────────────────────────────────────────────────────── */

static bool flush_ready(esp_lcd_panel_io_handle_t io,
                        esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    s_first_flush = true;
    lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}

static void flush_cb(lv_display_t *d, const lv_area_t *area, uint8_t *px)
{
    esp_lcd_panel_io_handle_t io = lv_display_get_user_data(d);
    uint16_t x1 = area->x1, x2 = area->x2, y1 = area->y1, y2 = area->y2;
    esp_lcd_panel_io_tx_param(io, ST7735_CASET,
        (uint8_t[]){x1>>8,x1&0xFF,x2>>8,x2&0xFF}, 4);
    esp_lcd_panel_io_tx_param(io, ST7735_RASET,
        (uint8_t[]){y1>>8,y1&0xFF,y2>>8,y2&0xFF}, 4);
    int sz = (x2-x1+1)*(y2-y1+1)*2;
    esp_lcd_panel_io_tx_color(io, ST7735_RAMWR, px, sz);
}

static void tick_cb(void *arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); }

static lv_display_t *display_init(esp_lcd_panel_io_handle_t io)
{
    lv_display_t *d = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_color_format_t cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
    uint32_t stride = lv_draw_buf_width_to_stride(LCD_H_RES, cf);
    size_t sz = stride * LCD_V_RES;
    void *b1 = spi_bus_dma_memory_alloc(LCD_HOST, sz, 0);
    void *b2 = spi_bus_dma_memory_alloc(LCD_HOST, sz, 0);
    void *b3 = spi_bus_dma_memory_alloc(LCD_HOST, sz, 0);
    assert(b1 && b2 && b3);
    lv_display_set_color_format(d, cf);
    lv_display_set_buffers(d, b1, b2, sz, LV_DISPLAY_RENDER_MODE_FULL);
    lv_draw_buf_init(&s_draw_buf3, LCD_H_RES, LCD_V_RES, cf, stride, b3, sz);
    lv_display_set_3rd_draw_buffer(d, &s_draw_buf3);
    lv_display_set_user_data(d, io);
    lv_display_set_flush_cb(d, flush_cb);
    return d;
}

/* ── UI ────────────────────────────────────────────────────────────────── */

static void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_YELLOW), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Test ROM\nPress A to toggle\nbacklight");
    lv_obj_set_style_text_color(lbl, lv_color_hex(UI_BROWN), 0);
    lv_obj_center(lbl);

    lv_obj_t *bat_label = lv_label_create(scr);
    lv_label_set_text(bat_label, "Battery: --");
    lv_obj_set_style_text_color(bat_label, lv_color_hex(UI_BROWN), 0);
    lv_obj_align(bat_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/* ── Main ──────────────────────────────────────────────────────────────── */

void app_main(void)
{
    /* Return-to-Loader: must be first line */
    return_to_loader_setup();

    ESP_LOGI(TAG, "=== Test ROM Starting ===");

    /* Initialize hardware */
    backlight_init();
    adc_init();
    buttons_init();

    /* Initialize LCD + LVGL */
    esp_lcd_panel_io_handle_t io = lcd_init();
    lv_init();
    lv_display_t *disp = display_init(io);

    /* Register flush callback */
    esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = flush_ready };
    esp_lcd_panel_io_register_event_callbacks(io, &cbs, disp);

    /* LVGL tick timer */
    esp_timer_create_args_t ta = { .callback = tick_cb, .name = "lv" };
    esp_timer_handle_t tt;
    esp_timer_create(&ta, &tt);
    esp_timer_start_periodic(tt, LVGL_TICK_PERIOD_MS * 1000);

    /* Create UI */
    ui_create();

    /* Wait for first flush then turn on display */
    s_first_flush = false;
    lv_refr_now(NULL);
    for (int i = 0; i < 100 && !s_first_flush; i++)
        vTaskDelay(pdMS_TO_TICKS(1));
    st7735_tx(s_lcd_io, ST7735_DISPON, NULL, 0);
    st7735_delay(20);

    ESP_LOGI(TAG, "Test ROM running. Press Button A to toggle backlight.");

    /* Main loop: handle Button A + battery monitoring */
    int btn_a_count = 0;
    bool last_btn_a = false;

    while (true) {
        /* Read Button A via ADC */
        bool btn_a = button_a_is_pressed();

        /* Detect press event */
        if (btn_a && !last_btn_a) {
            btn_a_count++;
            backlight_toggle();
            ESP_LOGI(TAG, "Button A pressed! Count: %d", btn_a_count);
        }
        last_btn_a = btn_a;

        /* Update battery voltage periodically */
        if (!btn_a) {
            float vbat = read_battery_voltage();
            if (vbat > 0) {
                ESP_LOGI(TAG, "Battery: %.2fV", vbat);
            }
        }

        /* Handle LVGL */
        uint32_t delay = lv_timer_handler();
        usleep(MAX(MIN(delay, 16), 1) * 1000);
    }
}
