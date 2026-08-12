# 🚀 快速开始 - Test ROM

## ⚠️ GitHub Token 权限问题

当前提供的 token 没有推送到 `jsfaint/xiaomiao-firmware` 仓库的权限。

### 解决方案

**选项 1: 使用你自己的 GitHub 仓库**
```bash
# 1. 在 GitHub 上创建新仓库（例如：yourname/xiaomiao-test-rom）
# 2. 修改远程仓库地址
git remote set-url origin https://YOUR_TOKEN@github.com/YOUR_USERNAME/xiaomiao-test-rom.git

# 3. 推送代码
git push -u origin main
```

**选项 2: 手动上传到 jsfaint/xiaomiao-firmware**
1. 下载工作区文件
2. 手动推送到该仓库

**选项 3: 直接在 GitHub 网页上创建文件**
1. 访问 https://github.com/jsfaint/xiaomiao-firmware
2. 点击 "Add file" → "Upload files"
3. 上传 test-rom 目录的所有文件
4. GitHub Actions 会自动触发编译

## 📦 项目已准备就绪

所有文件已在工作区创建完成：

```
test-rom/                    # 测试 ROM 项目
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── return_to_loader.h
├── .gitignore
├── BUILD_INSTRUCTIONS.md    # 详细构建说明
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml
    └── main.c               # 测试代码

.github/workflows/
└── build-test-rom.yml       # GitHub Actions 配置
```

## 🔧 下一步操作

### 方法 A: 通过 GitHub Actions 自动编译（推荐）

1. **将代码推送到 GitHub**（使用你有权限的仓库）
2. **进入 Actions 标签页**
3. **点击 "Run workflow"** 手动触发编译
4. **等待 5-10 分钟**
5. **下载固件**:
   - 方式 1: Artifacts → `test-rom-firmware.zip`
   - 方式 2: Releases → 最新版本的附件

### 方法 B: 本地编译（如果你有 ESP-IDF 环境）

```bash
cd test-rom
. ~/esp/esp-idf/export.sh
idf.py build
idf.py merge-bin
# 固件位置: build/test_rom-merged.bin
```

## 🎯 测试功能

编译后的固件将测试：
- ✅ **GPIO0 背光控制**（低电平点亮）
- ✅ **按键 A ADC 检测**（阈值 < 200）
- ✅ **电池电压监测**（ADC CH6，分压 9.1k+2.4k）
- ✅ **LVGL UI 显示**

## 📖 详细文档

查看 [`test-rom/BUILD_INSTRUCTIONS.md`](test-rom/BUILD_INSTRUCTIONS.md) 获取完整的构建和使用说明。

---

**状态**: ✅ 代码已准备，等待推送到 GitHub 触发自动编译  
**最后更新**: 2026-08-12