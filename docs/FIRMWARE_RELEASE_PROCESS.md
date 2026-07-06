# 固件发布流程

## v1.0.0 发布目标

第一版发布包只承诺支持：

- `bread-compact-wifi-lcd`
- ESP32-S3
- 16MB Flash
- ST7735 128x160 LCD
- 小智主体
- 网络电台

## 每次发布前

1. 更新 `CMakeLists.txt` 里的 `PROJECT_VER`。
2. 更新 `CHANGELOG.md`。
3. 更新 `handoff/CURRENT_STATE.md`。
4. 确认 `main/CMakeLists.txt` 仍包含网络电台，且第一版不包含数学游戏：

```text
games/internet_radio.cc
```

5. 确认 `compact_wifi_board_lcd.cc` 仍有：

```text
XIAOZHI / RADIO
```

## 构建

```powershell
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-v1 build
```

## 测试

按 `docs/TEST_CHECKLIST.md` 逐项检查。

## 打包

```powershell
.\scripts\package_firmware.ps1 -AppBin .\build-v1\xiaozhi.bin -Version 1.0.0 -Board bread-compact-wifi-lcd
```

## GitHub Release 建议

Tag：

```text
v1.0.0
```

标题：

```text
xiaozhi-s3-radio-math v1.0.0
```

Release 说明：

```text
第一版长期迭代基线固件。

保留：
- 小智主体
- 网络电台
- bread-compact-wifi-lcd 菜单入口：XIAOZHI / RADIO

烧录：
- 推荐 app-only 烧录
- 地址：0x100000
- 默认保留 NVS/Wi-Fi 配置

请下载 zip 包后阅读 README_刷机说明.md。
```
