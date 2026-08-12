# LVGL 9.5 UI Patterns for Xiaomiao Console

## Display Configuration

Triple full-screen DMA buffers for tear-free 60fps:

```c
lv_display_t *disp = lv_display_create(160, 128);
lv_color_format_t cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
uint32_t stride = lv_draw_buf_width_to_stride(160, cf);
size_t buf_sz = stride * 128;

void *b1 = spi_bus_dma_memory_alloc(SPI2_HOST, buf_sz, 0);
void *b2 = spi_bus_dma_memory_alloc(SPI2_HOST, buf_sz, 0);
void *b3 = spi_bus_dma_memory_alloc(SPI2_HOST, buf_sz, 0);

lv_display_set_color_format(disp, cf);
lv_display_set_buffers(disp, b1, b2, buf_sz, LV_DISPLAY_RENDER_MODE_FULL);

lv_draw_buf_t draw_buf3;
lv_draw_buf_init(&draw_buf3, 160, 128, cf, stride, b3, buf_sz);
lv_display_set_3rd_draw_buffer(disp, &draw_buf3);
lv_display_set_user_data(disp, io_handle);
lv_display_set_flush_cb(disp, lvgl_flush_cb);
```

**Why three buffers**: Two for LVGL double-buffering + one for the DMA transfer
currently in flight. Prevents blocking on SPI bus.

## Keypad Input

```c
lv_group_t *group = lv_group_create();
lv_group_set_default(group);

lv_indev_t *indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
lv_indev_set_display(indev, disp);
lv_indev_set_group(indev, group);
lv_indev_set_read_cb(indev, keypad_read_cb);
lv_indev_set_long_press_time(indev, 360);
lv_indev_set_long_press_repeat_time(indev, 130);
```

The keypad callback scans 6 GPIOs and maps to LVGL keys:

```c
static void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    // Scan GPIOs 2,13,27,35,34,12 (active-low)
    // 25ms software debounce
    // Map to LV_KEY_UP/DOWN/LEFT/RIGHT/ENTER/ESC
}
```

## Color Palette (Original Theme)

```c
#define UI_YELLOW  0xF6D34A   // main background
#define UI_BLACK   0x1B1713   // dark text
#define UI_BROWN   0x5C4220   // title bars, button focus bg
#define UI_RED     0xE64B3C   // warnings
#define UI_CREAM   0xFFF3B0   // text on dark backgrounds
#define UI_GREEN   0x2DD466   // success/progress
```

## Screen Layout Pattern

```
160px wide, 128px tall
┌───────────────────────────┐
│ Title bar (12px, brown)   │  flex column
├───────────────────────────┤
│ Status (10px, brown)      │
├───────────────────────────┤
│                           │
│ Content area (flex grow)  │  scrollable
│                           │
├───────────────────────────┤
│ Hint bar (10px, brown)    │
└───────────────────────────┘
```

```c
lv_obj_t *scr = lv_obj_create(NULL);
lv_obj_set_style_bg_color(scr, lv_color_hex(UI_YELLOW), 0);
lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
lv_obj_set_style_pad_all(scr, 0, 0);
lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
lv_obj_set_style_pad_row(scr, 0, 0);
```

## Button Focus Styling

In LVGL 9.5, buttons get `LV_STATE_FOCUSED` when selected via keypad. Set
both bg and text color on the button — child labels inherit text_color:

```c
lv_obj_t *btn = lv_button_create(parent);
lv_obj_set_width(btn, lv_pct(100));
lv_obj_set_style_bg_color(btn, lv_color_hex(UI_CREAM), 0);
lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

// Focused state
lv_obj_set_style_bg_color(btn, lv_color_hex(UI_BROWN), LV_STATE_FOCUSED);
lv_obj_set_style_text_color(btn, lv_color_hex(UI_BROWN), 0);
lv_obj_set_style_text_color(btn, lv_color_hex(UI_CREAM), LV_STATE_FOCUSED);

// Label inherits text_color from button — no need to style separately
lv_obj_t *lbl = lv_label_create(btn);
lv_label_set_text(lbl, "Item");
lv_obj_center(lbl);

lv_group_add_obj(group, btn);
lv_obj_add_event_cb(btn, on_click, LV_EVENT_SHORT_CLICKED, user_data);
lv_obj_add_event_cb(btn, on_key, LV_EVENT_KEY, user_data);
```

## Key Event Handling

Each button needs its own `LV_EVENT_KEY` handler (events don't bubble
from buttons by default in LVGL 9):

```c
static void on_key(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ESC) {
        // Handle B button
    }
}

static void on_click(lv_event_t *e) {
    // Handle A button (ENTER)
    // LV_EVENT_SHORT_CLICKED fires on ENTER release
}
```

## Screen Switching

Use separate screen objects with `lv_screen_load()`:

```c
static lv_obj_t *main_screen;
static lv_obj_t *about_screen;

// Create screens once, switch between them
void show_about(void) {
    lv_screen_load(about_screen);
    lv_group_focus_obj(about_area);
}

void back_to_main(void) {
    lv_screen_load(main_screen);
    lv_group_focus_obj(first_button);
}
```

## Progress Bar

```c
lv_obj_t *bar = lv_bar_create(parent);
lv_obj_set_width(bar, 140);
lv_obj_set_height(bar, 14);
lv_obj_set_style_bg_color(bar, lv_color_hex(UI_CREAM), 0);
lv_obj_set_style_bg_color(bar, lv_color_hex(UI_GREEN), LV_PART_INDICATOR);
lv_bar_set_range(bar, 0, 100);
lv_bar_set_value(bar, 0, LV_ANIM_OFF);

// Update:
lv_bar_set_value(bar, percent, LV_ANIM_OFF);
```

## Keeping UI Alive During Long Operations

When running a blocking loop (e.g. OTA write), call LVGL manually between
chunks:

```c
while (working) {
    // ... do chunk of work ...
    lv_tick_inc(16);     // simulate tick
    lv_timer_handler();  // process LVGL
}
```

## Tick Timer

```c
static void lvgl_tick_cb(void *arg) { lv_tick_inc(1); }

// In app_main:
esp_timer_create_args_t args = { .callback = lvgl_tick_cb, .name = "lv" };
esp_timer_handle_t timer;
esp_timer_create(&args, &timer);
esp_timer_start_periodic(timer, 1000);  // 1ms
```

## LVGL Task

```c
static void lvgl_task(void *arg) {
    // Build UI, wait for first flush, turn on display
    ui_create();
    lv_refr_now(NULL);
    lcd_display_on();

    while (true) {
        uint32_t delay = lv_timer_handler();
        delay = MAX(delay, 1);
        delay = MIN(delay, 16);
        usleep(delay * 1000);
    }
}
```

## Required sdkconfig

```
CONFIG_LV_CONF_SKIP=y
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_DEF_REFR_PERIOD=16
CONFIG_LV_USE_DRAW_SW=y
CONFIG_LV_DRAW_SW_SUPPORT_RGB565=y
CONFIG_LV_DRAW_SW_SUPPORT_RGB565_SWAPPED=y
CONFIG_LV_FONT_MONTSERRAT_10=y
CONFIG_LV_FONT_MONTSERRAT_12=y
CONFIG_LV_FONT_MONTSERRAT_14=y
```
