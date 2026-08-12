# xiaomiao-firmware 项目框架

## 📁 项目结构

```
workspace/
├── README.md                          # 项目主说明文档
├── .operit/
│   └── work-log.md                    # 工作台账
└── skills/xiaomiao-firmware/
    ├── SKILL.md                       # ⭐ Skill 核心定义 (20.50 KB)
    ├── test-prompts.json              # 测试提示词
    ├── assets/                        # 📦 模板文件（用于生成新项目）
    │   ├── CMakeLists.txt             # 顶层 CMake 配置
    │   ├── main-template.c            # main.c 完整骨架代码
    │   ├── main_CMakeLists.txt        # main 组件 CMake
    │   ├── partitions.csv             # OTA 分区表
    │   ├── return_to_loader.h         # Loader 返回机制
    │   └── sdkconfig.defaults         # SDK 默认配置
    ├── references/                    # 📚 参考文档
    │   ├── hardware.md                # 硬件引脚与协议详解
    │   ├── loader.md                  # ROM Loader 架构说明
    │   └── lvgl-ui.md                 # LVGL UI 开发指南
    └── scripts/
        └── new-rom.sh                 # 🚀 项目脚手架脚本
```

## 🎯 项目用途

这是一个 **ESP32 固件开发 Skill**，专为 **小喵（学而思）教育掌机** 设计，提供：

1. **一键项目生成** - 使用 `new-rom.sh` 快速创建完整的 ESP-IDF 项目
2. **硬件抽象层** - 预配置的 LCD、SD卡、按键、I2C、蜂鸣器驱动
3. **LVGL 9.5 UI 框架** - 三缓冲渲染、60fps、6键导航支持
4. **Loader 集成** - Reset 自动返回 ROM 选择器
5. **完整文档** - 硬件引脚、通信协议、UI 设计规范

## 🔧 目标设备

**小喵教育掌机** 硬件规格：
- **主控**: ESP32-WROVER-B (4MB Flash + 8MB PSRAM)
- **协处理器**: GD32F350G8
- **显示**: ST7735 160×128 TFT (SPI)
- **存储**: MicroSD 卡槽
- **输入**: 6键手柄 (UP/DOWN/LEFT/RIGHT/A/B)
- **传感器**: MPU6050 陀螺仪 (I2C)
- **其他**: 蜂鸣器、LED、马达

## 📌 关键引脚映射

| 外设 | GPIO | 备注 |
|------|------|------|
| LCD BL | **0** | **背光控制（低电平点亮）** (NEW) |
| LCD SCLK | 18 | SPI2, 60MHz |
| LCD MOSI | 23 | SPI2 |
| LCD MISO | 19 | SPI2 |
| LCD CS | 5 | SPI2 |
| LCD DC | 4 | 数据/命令选择 |
| SD CS | 22 | 共享 SPI2 |
| KEY UP | 2 | 低电平有效，内部上拉 |
| KEY DOWN | 13 | 低电平有效，内部上拉 |
| KEY LEFT | 27 | 低电平有效，内部上拉 |
| KEY RIGHT | 35 | 仅输入，需外部上拉 |
| KEY A | **34** | **ADC 阈值检测**；**同时用于电池电压监测 (ADC1_CH6)** (NEW) |
| KEY B | 12 | 低电平有效，内部上拉 |
| I2C SCL | 15 | 100kHz |
| I2C SDA | 21 | 100kHz |
| BUZZER | 14 | LEDC Timer0/Ch0 |

## 🚀 快速开始

### 前置要求
- ESP-IDF v5.5.4
- Python 3.12+
- CMake + Ninja
- LVGL 9.5.0

### 使用流程

```bash
# 1. 生成新项目
./skills/xiaomiao-firmware/scripts/new-rom.sh my-game ~/projects/my-game

# 2. 进入项目目录
cd ~/projects/my-game

# 3. 设置 ESP-IDF 环境
. ~/esp/esp-idf/export.sh

# 4. 构建固件
idf.py build

# 5. 烧录到设备
idf.py -p /dev/ttyACM0 flash

# 6. (可选) 生成合并固件用于 TF 卡
idf.py merge-bin
cp build/my-game-merged.bin /sdcard/roms/my-game.bin
```

## ⚠️ 重要注意事项

1. **必须调用 Loader 初始化**
   ```c
   void app_main(void) {
       return_to_loader_setup();  // ← 第一行必须调用！
       // ... 你的代码
   }
   ```

2. **Flash 空间限制**: ROM 镜像 ≤ 3.25MB (ota_0 分区)

3. **SPI 总线竞争**: LCD 和 SD 卡共享 SPI2，需要交替访问

4. **GPIO34/35 限制**: 仅输入模式，无内部上拉，需外部电阻

5. **GPIO0 背光控制** (NEW): 
   - 低电平点亮背光，高电平关闭
   - **注意**: GPIO0 也用于下载模式，烧录固件时需确保 GPIO0 为高电平

6. **GPIO34/ADC 双重功能** (NEW):
   - 按键 A 通过 ADC 阈值检测（ADC_raw < 200 = 按下）
   - 同时用于电池电压监测（分压电阻: 9.1kΩ + 2.4kΩ）
   - 单一 ADC 通道实现两种功能，无冲突
   - 可能需要根据实际电池电压校准阈值

## 📖 文档导航

- **[SKILL.md](skills/xiaomiao-firmware/SKILL.md)** - Skill 完整定义和使用指南
- **[hardware.md](skills/xiaomiao-firmware/references/hardware.md)** - 硬件引脚、LCD初始化、I2C协议
- **[loader.md](skills/xiaomiao-firmware/references/loader.md)** - ROM Loader 架构、分区布局
- **[lvgl-ui.md](skills/xiaomiao-firmware/references/lvgl-ui.md)** - LVGL UI 开发规范

## 🙏 致谢

- 硬件知识来源: [ZyoungInc/xueersi-idf](https://github.com/ZyoungInc/xueersi-idf)
- 原理图绘制: 「我为电波狂」

## 📄 License

MIT License

---
*最后更新: 2026-08-12*