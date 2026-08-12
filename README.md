# Xiaomiao Console Firmware Skill

为 [小喵（学而思）教育掌机](https://www.xueersi.com) 开发 ESP32 固件（ROM）的 opencode skill。

> **设备参数**：ESP32-WROVER-B（4MB Flash / 8MB PSRAM）+ GD32F350G8 协处理器
> ST7735 160×128 TFT、MicroSD、6 键手柄、MPU6050 陀螺仪、蜂鸣器、LED/马达

## 功能

- **一键脚手架** — `new-rom.sh` 生成完整的 ESP-IDF 项目骨架
- **硬件初始化模板** — LCD、SD 卡、按键、I2C、蜂鸣器全部配置就绪
- **LVGL 9.5 UI** — 三缓冲 60fps 渲染、6 键导航、原始主题配色
- **Loader 集成** — `return_to_loader.h` 实现 Reset 即返回 ROM 列表
- **硬件参考文档** — 完整引脚定义、I2C 协议、LCD 初始化序列、ADC 通道
- **兼容分区表** — 与原厂 Loader 的 OTA 分区布局完全匹配

## 安装

使用 [skills CLI](https://skills.sh)（`npx skills`）安装，支持 OpenCode、Claude Code、Codex 等多种 Agent：

```bash
# 安装到当前项目（.agents/skills/）
npx skills add jsfaint/xiaomiao-firmware -a opencode

# 全局安装（~/.config/opencode/skills/）
npx skills add jsfaint/xiaomiao-firmware -a opencode -g
```

其他常用命令：

```bash
npx skills list                 # 查看已安装的 skills
npx skills update xiaomiao-firmware   # 更新到最新版本
npx skills remove xiaomiao-firmware   # 卸载
```

## 前置环境

| 工具 | 版本 |
|------|------|
| ESP-IDF | v5.5.4 |
| xtensa-esp-elf | 工具链（随 IDF 安装） |
| CMake + Ninja | 构建系统 |
| Python | 3.12+ |
| LVGL | 9.5.0（通过 IDF Component Manager 拉取） |

## 快速开始

```bash
# 1. 生成新 ROM 项目
scripts/new-rom.sh my-game ~/projects/my-game

# 2. 配置 ESP-IDF 环境
. ~/esp/esp-idf/export.sh

# 3. 构建 & 烧录（通过 GD32 USB-UART 桥）
cd ~/projects/my-game
idf.py build
idf.py -p /dev/ttyACM0 flash

# 4. 生成合并固件并拷入 TF 卡（可选）
idf.py merge-bin
cp build/my-game-merged.bin /sdcard/roms/my-game.bin
```

生成的项目结构：

```
my-game/
├── CMakeLists.txt          # 顶层 ESP-IDF 项目
├── sdkconfig.defaults      # 板级配置
├── partitions.csv          # Loader 兼容分区表
├── return_to_loader.h      # 返回 Loader 集成头文件
├── .gitignore
└── main/
    ├── CMakeLists.txt
    ├── main.c              # 完整硬件初始化骨架
    └── idf_component.yml   # lvgl/lvgl 9.5.0
```

> **关键**：`app_main()` 的第一行必须调用 `return_to_loader_setup()`，确保 Reset/断电重启后返回 Loader 而非重复进入当前 ROM。

## 项目结构

```
xiaomiao-firmware/
└── skills/xiaomiao-firmware/
    ├── SKILL.md                     # Skill 定义与设备总览
    ├── assets/                      # 可复制的模板文件
    │   ├── main-template.c          #   main.c 骨架（含全部硬件初始化）
    │   ├── CMakeLists.txt           #   顶层 CMake 模板
    │   ├── main_CMakeLists.txt      #   main/ 组件 CMake 模板
    │   ├── sdkconfig.defaults       #   板级 Kconfig 默认值
    │   ├── partitions.csv           #   Loader 分区表
    │   └── return_to_loader.h       #   返回 Loader 的内联实现
    ├── references/                  # 详细参考文档
    │   ├── hardware.md              #   引脚定义、LCD/SD/I2C/ADC/蜂鸣器
    │   ├── loader.md                #   ROM Loader 架构与分区布局
    │   └── lvgl-ui.md               #   LVGL 9.5 UI 模式与配色
    └── scripts/
        └── new-rom.sh               # 项目脚手架脚本
```

## 核心引脚速查

```
LCD:  BL=0   SCLK=18  MOSI=23  MISO=19  CS=5  DC=4   (SPI2 @ 60MHz)
SD:   CS=22    (共享 SPI2)
Keys: UP=2  DOWN=13  LEFT=27  RIGHT=35  A=34(ADC)  B=12  (低电平有效)
I2C:  SCL=15  SDA=21  (100kHz)
Buzz: GPIO14 (LEDC Timer0/Ch0)
ADC:  CH0=36(光敏)  CH3=39(温度)  CH4=32  CH5=33  CH6=34(按键A+电池)
```

> - GPIO34/35 仅输入、无内部上拉，需外部上拉电阻。其余按键启用内部上拉。
> - **GPIO0**: 背光控制（低电平点亮）
> - **GPIO34/ADC_CH6**: 按键 A 通过 ADC 阈值检测（<200 为按下），同时监测电池电压

## 文档导航

| 文档 | 内容 |
|------|------|
| [SKILL.md](skills/xiaomiao-firmware/SKILL.md) | Skill 定义、设备总览、常用任务 |
| [references/hardware.md](skills/xiaomiao-firmware/references/hardware.md) | 完整引脚表、LCD 初始化序列、I2C 协议（GD32/MPU6050）、ADC、蜂鸣器 |
| [references/loader.md](skills/xiaomiao-firmware/references/loader.md) | 分区布局、启动流程、ROM 文件格式、Skip-Write 优化 |
| [references/lvgl-ui.md](skills/xiaomiao-firmware/references/lvgl-ui.md) | 三缓冲显示、按键输入、配色方案、屏幕布局、控件样式 |

## 硬件限制

- **4MB Flash** — ROM 镜像须 ≤ 3.25MB（ota_0 分区）
- **无防撕裂** — TE 引脚未连接 MCU，无法硬件 vsync
- **背光控制** — GPIO0 控制背光（低电平点亮），支持软件调光
- **SPI 总线竞争** — LCD 与 SD 共享 SPI2，需交替访问
- **PSRAM** — 可用于数据存储，ESP32 不支持从 PSRAM 执行代码
- **GPIO34 双重功能** — 按键 A 与电池电压检测共用，读取 ADC 时可能影响按键状态
- **GPIO0 下载模式** — 烧录固件时需确保 GPIO0 为高电平（背光应关闭）

## 鸣谢

本 skill 的全部硬件知识——引脚定义、I2C 协议、LCD 初始化序列、分区布局等——均源自 [ZyoungInc/xueersi-idf](https://github.com/ZyoungInc/xueersi-idf) 项目的实测与逆向成果。感谢作者 ZYoungInc 无偿开源固件与硬件资料。

原理图由「我为电波狂」测量并绘制。

## License

MIT
