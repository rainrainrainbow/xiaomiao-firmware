# 🚀 推送代码到 GitHub 并触发编译 - 操作指南

## ⚠️ 问题说明

当前 Git token 没有权限推送到 `jsfaint/xiaomiao-firmware` 仓库。

## ✅ 解决方案（3 选 1）

### 方案 1：使用 GitHub 网页上传（最简单，推荐）

#### 步骤：

1. **访问仓库**
   ```
   https://github.com/jsfaint/xiaomiao-firmware
   ```

2. **上传 test-rom 目录**
   - 点击 **"Add file"** → **"Upload files"**
   - 拖拽或选择以下文件：
     ```
     test-rom/CMakeLists.txt
     test-rom/sdkconfig.defaults
     test-rom/partitions.csv
     test-rom/return_to_loader.h
     test-rom/.gitignore
     test-rom/main/CMakeLists.txt
     test-rom/main/idf_component.yml
     test-rom/main/main.c
     ```
   - Commit message: `Add test ROM with backlight and ADC button detection`
   - 点击 **"Commit changes"**

3. **上传 GitHub Actions 工作流**
   - 再次点击 **"Add file"** → **"Upload files"**
   - 创建目录结构：在文件名中输入 `.github/workflows/build-test-rom.yml`
   - 复制 `.github/workflows/build-test-rom.yml` 的内容粘贴进去
   - Commit message: `Add GitHub Actions workflow for test ROM`
   - 点击 **"Commit changes"**

4. **触发编译**
   - 进入 **"Actions"** 标签页
   - 找到 **"Build Test ROM"** 工作流
   - 点击 **"Run workflow"** → **"Run workflow"**
   - 等待 5-10 分钟

5. **下载固件**
   - 工作流完成后，点击最新的运行记录
   - 在 **"Artifacts"** 部分下载 `test-rom-firmware.zip`
   - 解压得到 `test_rom.bin` 和 `test_rom-merged.bin`

---

### 方案 2：创建新仓库（如果你有 GitHub 账号）

#### 步骤：

1. **创建新仓库**
   - 访问 https://github.com/new
   - 仓库名：`xiaomiao-test-rom`
   - 公开（Public）
   - 不要初始化 README
   - 点击 **"Create repository"**

2. **获取你的 Personal Access Token**
   - Settings → Developer settings → Personal access tokens
   - 生成新 token（需要 repo 权限）

3. **推送代码**
   ```bash
   cd /data/user/0/com.ai.assistance.operit/files/workspace/048cffc1-79e7-4894-9879-88383911322b
   
   # 修改远程地址（替换 YOUR_USERNAME 和 YOUR_TOKEN）
   git remote set-url origin https://YOUR_TOKEN@github.com/YOUR_USERNAME/xiaomiao-test-rom.git
   
   # 推送
   git push -u origin main
   ```

4. **触发编译**
   - 推送后 GitHub Actions 会自动触发
   - 或手动进入 Actions 标签页运行

---

### 方案 3：使用提供的压缩包手动上传

我已为你创建了压缩包：
- `test-rom-release.zip` - test-rom 目录
- `github-workflow.zip` - GitHub Actions 配置

#### 步骤：

1. **解压文件**
   ```bash
   # 在电脑上解压这两个文件
   unzip test-rom-release.zip
   unzip github-workflow.zip
   ```

2. **按照方案 1 的步骤上传到 GitHub**

---

## 📦 需要的文件清单

确保上传以下文件到仓库：

```
.github/
└── workflows/
    └── build-test-rom.yml      # ⭐ 必须

test-rom/
├── CMakeLists.txt              # ⭐ 必须
├── sdkconfig.defaults          # ⭐ 必须
├── partitions.csv              # ⭐ 必须
├── return_to_loader.h          # ⭐ 必须
├── .gitignore
└── main/
    ├── CMakeLists.txt          # ⭐ 必须
    ├── idf_component.yml       # ⭐ 必须
    └── main.c                  # ⭐ 必须（测试代码）
```

---

## 🔍 验证编译成功

编译成功后，你应该能下载到一个 ZIP 文件，包含：
- `test_rom.bin` - 标准固件
- `test_rom-merged.bin` - 合并固件（用于 SD 卡）

固件大小预期：
- `test_rom.bin`: ~500KB - 1MB
- `test_rom-merged.bin`: ~2MB - 3MB

---

## 📱 使用固件

1. 将 `test_rom-merged.bin` 重命名为 `test_rom.bin`
2. 复制到 SD 卡的 `/roms/` 目录
3. 重启小喵掌机
4. 在 Loader 中选择 "test_rom"
5. 观察屏幕显示和按键响应

---

## ❓ 常见问题

**Q: GitHub Actions 没有自动触发？**
A: 手动进入 Actions 标签页，点击 "Run workflow"

**Q: 编译失败？**
A: 检查是否所有必需文件都已上传，特别是 `.github/workflows/build-test-rom.yml`

**Q: 下载不到 Artifact？**
A: 等待工作流完全完成（绿色对勾），然后点击运行记录下载

---

**最后更新**: 2026-08-12  
**推荐方案**: 方案 1（GitHub 网页上传）