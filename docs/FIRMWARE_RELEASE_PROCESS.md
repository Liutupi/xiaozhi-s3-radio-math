# 固件发布流程

## 当前发布

最新版本：`v1.0.1`

目标硬件：

- `bread-compact-wifi-lcd`
- ESP32-S3
- 16MB Flash
- ST7735 128x160 LCD

当前功能：

- 小智主体
- 网络电台
- 设置页
- 极简动态表情
- 网易云 MCP 直链 MP3 播放桥接

## 每次发布前

1. 更新 `CMakeLists.txt` 里的 `PROJECT_VER`。
2. 更新 `CHANGELOG.md`。
3. 更新 `handoff/CURRENT_STATE.md`。
4. 新增一份 `handoff/YYYY-MM-DD-vX.Y.Z-*.md` 交接单。
5. 确认 `main/CMakeLists.txt` 包含：

```text
games/internet_radio.cc
```

6. 确认目标板级文件为：

```text
main/boards/bread-compact-wifi-lcd/compact_wifi_board_lcd.cc
```

## 构建

本机已验证命令：

```powershell
cmd.exe /c "set IDF_PATH=D:\3.5inch_ESP32-S3\Espressif\v5.5\esp-idf&& set IDF_TOOLS_PATH=C:\Espressif\tools&& set IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v5.5\venv&& set PYTHONUTF8=1&& set PATH=C:\Espressif\tools\ccache\4.10.2\ccache-4.10.2-windows-x86_64;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Espressif\tools\python\v5.5\venv\Scripts;C:\Espressif\tools\git\cmd;C:\Espressif\tools\git\bin;%PATH%&& C:\Espressif\tools\python\v5.5\venv\Scripts\python.exe D:\3.5inch_ESP32-S3\Espressif\v5.5\esp-idf\tools\idf.py -B build-v1 build"
```

## 测试

按 `docs/TEST_CHECKLIST.md` 逐项检查。

v1.0.1 额外测试：

- “多多，播放一首网易云音乐”
- 屏幕是否显示 `MUSIC / MCP Music`
- 是否有声音
- 如果没声音，检查多多是否调用 `MusicPlayer.PlayUrl`
- 如果已调用但无声，检查 MCP 返回链接是否是直链 MP3

## 打包

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_firmware.ps1 -AppBin .\build-v1\xiaozhi.bin -Version 1.0.1 -Board bread-compact-wifi-lcd
```

## app-only 烧录

保留 NVS/Wi-Fi 配置：

```powershell
python -m esptool --chip esp32s3 -p COM5 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x100000 build-v1\xiaozhi.bin
```

如果串口不是 `COM5`，用设备管理器里的实际串口替换。

## GitHub Release 建议

Tag：

```text
v1.0.1
```

标题：

```text
xiaozhi-s3-radio-math v1.0.1
```

Release 说明：

```text
v1.0.1 新增网易云 MCP 音乐播放桥接。

新增：
- MusicPlayer.PlayUrl(url, title)
- 复用 InternetRadio MP3 解码输出链路播放 MCP 返回的直链 MP3
- 设置页：音量、亮度
- 极简动态表情

烧录：
- 推荐 app-only 烧录
- 地址：0x100000
- 默认保留 NVS/Wi-Fi 配置

请下载 zip 包后阅读 README_FLASH.md。
```
