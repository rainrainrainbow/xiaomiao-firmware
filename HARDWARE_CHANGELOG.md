# 硬件变更记录

**更新日期**: 2026-08-12  
**版本**: v1.1 (硬件修订)

## 📋 变更概述

本次硬件修订增加了两项重要功能：
1. **软件可控背光** - 通过 GPIO0 实现
2. **电池电压监测** - 通过 GPIO34/ADC1_CH6 实现

---

## 🔆 变更 1: GPIO0 - TFT 背光控制

### 之前
- 背光硬接 VCC，无法软件控制
- 无法实现省电模式或亮度调节

### 现在
- **GPIO**: GPIO0
- **控制逻辑**: 低电平点亮，高电平关闭
- **优势**: 
  - 支持软件开关背光
  - 可实现 PWM 调光（通过 LEDC）
  - 省电模式下可关闭背光

### 初始化代码
```c
// 背光 GPIO 初始化
gpio_config_t bl_cfg = {
    .pin_bit_mask = 1ULL << GPIO_NUM_0,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
gpio_config(&bl_cfg);
gpio_set_level(GPIO_NUM_0, 0);  // 点亮背光（active-low）
```

### 控制函数
```c
// 开启背光
void backlight_on(void) {
    gpio_set_level(GPIO_NUM_0, 0);
}

// 关闭背光
void backlight_off(void) {
    gpio_set_level(GPIO_NUM_0, 1);
}

// PWM 调光示例（0-255）
void backlight_set_brightness(uint8_t brightness) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, brightness);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
```

### ⚠️ 重要注意事项
1. **下载模式冲突**: GPIO0 也用于 ESP32 的下载模式
   - 烧录固件时，GPIO0 必须为**高电平**
   - 建议在初始化时将背光关闭，或在烧录前确保 GPIO0 为高
   
2. **启动顺序**: 在 `app_main()` 中尽早初始化背光 GPIO

---

## 🔋 变更 2: GPIO34 - 按键 A ADC 检测 + 电池电压监测

### 之前
- GPIO34 仅用于按键 A（数字 GPIO 读取）
- 无电池电量监测功能

### 现在
- **检测方式**: **直接使用 ADC 读取**（非数字 GPIO）
- **双重功能**: 按键 A 检测 + 电池电压监测
- **ADC 通道**: ADC1_CH6
- **分压电路**: 9.1kΩ (R1) + 2.4kΩ (R2)
- **分压比**: 2.4 / (9.1 + 2.4) = 2.4 / 11.5 ≈ 0.209

### 检测逻辑

**按键按下时**: GPIO34 → GND（通过分压电阻）
- ADC_raw ≈ 0（或非常低的值）

**按键释放时**: GPIO34 → Vbat（通过分压电阻）
- ADC_raw = (Vbat × 2.4/11.5) / 3.3 × 4095

**典型值** (Vbat = 3.7V):
- ADC_raw ≈ (3.7 × 0.209) / 3.3 × 4095 ≈ 963

**推荐阈值**: ADC_raw < 200 → 按键按下

### 初始化代码
```c
// ADC 初始化
adc_oneshot_unit_handle_t adc_handle;
adc_oneshot_unit_init_cfg_t init_cfg = {
    .unit_id = ADC_UNIT_1,
};
adc_oneshot_new_unit(&init_cfg, &adc_handle);

adc_oneshot_chan_cfg_t chan_cfg = {
    .atten = ADC_ATTEN_DB_12,  // 12dB 衰减，最大测量约 3.9V
    .bitwidth = ADC_BITWIDTH_12,
};
adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &chan_cfg);
```

### 按键检测与电池读取
```c
// 简单的按键检测
bool is_button_a_pressed(adc_oneshot_unit_handle_t adc_handle) {
    int raw;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &raw);
    if (ret != ESP_OK) {
        return false;
    }
    
    // 阈值判断：ADC 值很低表示按键按下
    return (raw < 200);
}

// 综合函数：同时处理按键和电池
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
    
    // 检查按键是否按下（低 ADC 值）
    if (raw < 200) {
        return BTN_A_PRESSED;
    }
    
    // 按键未按下，计算电池电压
    if (out_vbat) {
        *out_vbat = (raw / 4095.0) * 3.3 * (11.5 / 2.4);
    }
    
    return BTN_A_RELEASED;
}
```

### ⚠️ 重要注意事项

1. **单一通道双功能**: ADC1_CH6 同时处理按键检测和电池监测，无需切换
2. **阈值校准**: 阈值 200 可能需要根据实际硬件调整
3. **滞后效应**: 建议在阈值附近添加滞后，避免误触发
   ```c
   // 带滞后的按键检测
   #define BTN_PRESS_THRESHOLD   150
   #define BTN_RELEASE_THRESHOLD 250
   
   if (raw < BTN_PRESS_THRESHOLD) {
       // 确认按下
   } else if (raw > BTN_RELEASE_THRESHOLD) {
       // 确认释放，读取电池
   }
   // 中间区域保持上次状态
   ```
4. **采样频率**: 建议 10-20ms 采样一次，平衡响应速度和功耗
5. **低电量边界**: 当电池电压极低时（<2.5V），可能与按键按下混淆，需特殊处理

---

## 📊 影响评估

### 正面影响
- ✅ 新增背光控制，支持省电模式
- ✅ 新增电池监测，可显示电量百分比
- ✅ 提升用户体验和设备智能化程度

### 需要注意的问题
- ⚠️ GPIO0 在下载模式时的冲突
- ⚠️ GPIO34 的双重功能可能导致干扰
- ⚠️ 需要在代码中妥善处理引脚复用

### 兼容性
- 旧代码仍可使用，但建议更新以利用新功能
- 如果使用 GPIO0 或 GPIO34，需要重新评估引脚分配

---

## 📚 相关文档

- [hardware.md](skills/xiaomiao-firmware/references/hardware.md) - 完整硬件参考
- [README.md](README.md) - 项目说明
- [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md) - 项目概览

---

**维护者**: Operit AI Assistant  
**最后更新**: 2026-08-12
