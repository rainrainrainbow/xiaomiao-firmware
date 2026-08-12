# Test ROM 构建说明

## 📦 项目结构

```
test-rom/
├── CMakeLists.txt              # 顶层 CMake 配置
├── sdkconfig.defaults          # SDK 默认配置
├── partitions.csv              # OTA 分区表
├── return_to_loader.h          # Loader 返回机制
├── .gitignore
└── main/
    ├── CMakeLists.txt          # main 组件配置
    ├── idf_component.yml       # LVGL 9.5.0 依赖
    └── main.c                  # 测试代码（背光+按键A ADC+电池监测）
```

## 🔧 GitHub Actions 自动编译

### 触发方式

1. **手动触发**: 在 GitHub Actions 页面点击 "Run workflow"
2. **推送触发**: 推送代码到 `main` 分支且修改了 `test-rom/**` 文件

### 编译流程

```yaml
1. Checkout 代码
2. 设置 ESP-IDF v5.5.4 环境
3. 执行 idf.py build
4. 生成合并固件 idf.py merge-bin
5. 上传构建产物为 Artifact
6. (可选) 创建 Release
```

### 获取固件

编译完成后，有两种方式获取固件：

#### 方式 1: 下载 Artifact
1. 进入 GitHub Actions → 最近的工作流运行
2. 找到 "Upload firmware artifact" 步骤
3. 下载 `test-rom-firmware.zip`
4. 解压后得到：
   - `test_rom.bin` - 标准固件
   - `test_rom-merged.bin` - 合并固件（用于 SD 卡）

#### 方式 2: 从 Release 下载
如果触发了 Release 创建：
1. 进入 Releases 页面
2. 找到最新的 "Test ROM Build #xxx"
3. 下载附件中的固件文件

## 🎯 测试功能

### 1. GPIO0 背光控制
- **初始化**: GPIO0 设为输出，低电平点亮
- **测试**: 按按键 A 切换背光开关
- **日志**: 查看串口输出 "Backlight ON/OFF"

### 2. 按键 A ADC 检测
- **通道**: ADC1_CH6 (GPIO34)
- **阈值**: ADC_raw < 200 = 按下
- **原理**: 按键按下时 GPIO34 接地，ADC 读数接近 0

### 3. 电池电压监测
- **分压**: 9.1kΩ + 2.4kΩ
- **公式**: `Vbat = ADC_raw × 3.3 / 4095 × 11.5 / 2.4`
- **显示**: LVGL UI 底部显示当前电池电压

### 4. LVGL UI
- **分辨率**: 160×128
- **显示内容**: 
  - 中央：测试说明和按键计数
  - 底部：电池电压

## 📝 使用步骤

### 本地编译（如果有 ESP-IDF 环境）

```bash
cd test-rom
. ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash
```

### 通过 GitHub Actions 编译

1. 推送代码到 GitHub
2. 等待 Actions 完成（约 5-10 分钟）
3. 下载固件文件
4. 将 `test_rom-merged.bin` 复制到 SD 卡的 `/roms/` 目录
5. 重启设备，在 Loader 中选择 "test_rom"

## ⚠️ 注意事项

1. **GPIO0 下载模式**: 烧录固件时确保 GPIO0 为高电平（背光关闭）
2. **按键 A 阈值**: 可能需要根据实际硬件调整 `BTN_A_THRESHOLD`
3. **电池校准**: 首次使用建议用万用表测量实际电压进行校准
4. **Flash 空间**: 固件大小应 ≤ 3.25MB（ota_0 分区限制）

## 🔍 调试

### 串口日志
```bash
idf.py -p /dev/ttyACM0 monitor
```

### 关键日志
- `Backlight initialized on GPIO0` - 背光初始化成功
- `ADC initialized on CH6 (GPIO34)` - ADC 初始化成功
- `Backlight ON/OFF` - 背光切换
- `UP/DOWN pressed` - 其他按键检测

## 📊 预期行为

1. **启动**: 屏幕显示 "Test ROM\nPress A to toggle\nbacklight"
2. **按按键 A**: 
   - 背光开关切换
   - 计数器 +1
   - 显示当前电池电压
3. **不按按键**: 底部持续更新电池电压
4. **按其他键**: 串口输出按键信息

---

**最后更新**: 2026-08-12  
**构建系统**: GitHub Actions (ESP-IDF v5.5.4)