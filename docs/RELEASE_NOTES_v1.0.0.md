# xiaozhi-s3-radio-math v1.0.0

第一版长期迭代基线固件。

## 保留功能

- 小智主体语音对话。
- 网络电台。
- `bread-compact-wifi-lcd` 菜单入口：`XIAOZHI / RADIO`。

## 支持硬件

- ESP32-S3
- 16MB Flash
- 8MB PSRAM
- ST7735 128x160 LCD
- `bread-compact-wifi-lcd`

## 烧录方式

推荐 app-only 烧录，保留 NVS/Wi-Fi 配置。

```powershell
.\flash_app_only.ps1 -Port COM4
```

关键参数：

- app offset：`0x100000`
- flash size：`16MB`
- flash mode：`dio`
- flash freq：`80m`

## 发布前必测

请按 `docs/TEST_CHECKLIST.md` 完整测试后再正式发布。

## 当前状态

源码、文档、交接本和打包脚本已准备好。

已构建并烧录：

- 构建目录：`build-v1`
- app 固件：`build-v1/xiaozhi.bin`
- SHA256：`ce36eff695443cde5b85f809e683e0808d0a398939e253904db38422cb0a737d`
- 发布包：`releases/xiaozhi-s3-radio-math-v1.0.0-bread-compact-wifi-lcd-20260706-231632.zip`
- 烧录端口：`COM5`
