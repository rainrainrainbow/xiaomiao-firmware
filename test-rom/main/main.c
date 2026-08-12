/*
 * Test ROM for Xiaomiao Console
 * Tests: Backlight control (GPIO0), Button A ADC detection, Battery monitoring
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "lvgl.h"

#include "return_to_loader.h"

static const char *TAG = "test_rom";

/* GPIO definitions */
#define GPIO_BACKLIGHT      GPIO_NUM_0
#define GPIO_BTN_UP         GPIO_NUM_2
#define GPIO_BTN_DOWN       GPIO_NUM_13
#define GPIO_BTN_LEFT       GPIO_NUM_27
#define GPIO_BTN_RIGHT      GPIO_NUM_35
#define GPIO_BTN_A          GPIO_NUM_34  /* ADC channel 6 */
#define GPIO_BTN_B          GPIO_NUM_12

/* ADC configuration */
#define ADC_CHANNEL         ADC_CHANNEL_6
#define ADC_ATTEN           ADC_ATTEN_DB_12
#define BTN_A_THRESHOLD     200  /* Below this = pressed */

/* LVGL display buffer */
#define LVGL_BUF_LINES      40
static lv_color_t lvgl_buf[160 * LVGL_BUF_LINES];

/* Global handles */
static adc_oneshot_unit_handle_t adc_handle = NULL;
static lv_display_t *lvgl_disp = NULL;

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
    
    uint64_t pullup = mask;  /* All have internal pull-up except GPIO34/35 */
    /* GPIO35 is input-only, no pull-up */
    pullup &= ~(1ULL << GPIO_BTN_RIGHT);

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

static bool read_button(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == 0;  /* Active-low */
}

/* ── LVGL Display ──────────────────────────────────────────────────────── */

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* Simplified: just acknowledge flush */
    lv_display_flush_ready(disp);
}

static void lvgl_init(void)
{
    lv_init();

    lvgl_disp = lv_display_create(160, 128);
    lv_display_set_buffers(lvgl_disp, lvgl_buf, NULL, 
                          sizeof(lvgl_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvgl_disp, lvgl_flush_cb);

    ESP_LOGI(TAG, "LVGL initialized");
}

/* ── UI Task ───────────────────────────────────────────────────────────── */

static void ui_task(void *arg)
{
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Test ROM\nPress A to toggle\nbacklight");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *bat_label = lv_label_create(lv_screen_active());
    lv_obj_align(bat_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    int btn_a_count = 0;
    bool last_btn_a = false;

    while (1) {
        /* Read Button A via ADC */
        bool btn_a = button_a_is_pressed();
        
        /* Detect press event */
        if (btn_a && !last_btn_a) {
            btn_a_count++;
            backlight_toggle();
            lv_label_set_text_fmt(label, "Test ROM\nA pressed: %d times\nBat: %.2fV", 
                                 btn_a_count, read_battery_voltage());
        }
        last_btn_a = btn_a;

        /* Update battery voltage display periodically */
        if (!btn_a) {
            float vbat = read_battery_voltage();
            if (vbat > 0) {
                lv_label_set_text_fmt(bat_label, "Battery: %.2fV", vbat);
            }
        }

        /* Read other buttons */
        if (read_button(GPIO_BTN_UP)) {
            ESP_LOGI(TAG, "UP pressed");
        }
        if (read_button(GPIO_BTN_DOWN)) {
            ESP_LOGI(TAG, "DOWN pressed");
        }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ── Main ──────────────────────────────────────────────────────────────── */

void app_main(void)
{
    /* MUST call first: setup return-to-loader */
    return_to_loader_setup();

    ESP_LOGI(TAG, "=== Test ROM Starting ===");

    /* Initialize hardware */
    backlight_init();
    adc_init();
    buttons_init();
    lvgl_init();

    /* Create UI task */
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Test ROM running. Press Button A to toggle backlight.");
}
