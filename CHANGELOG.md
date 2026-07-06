# Changelog

## v1.0.0 - 2026-07-06

第一版长期迭代基线版本。

### 保留

- 保留小智主体语音对话能力。
- 保留网络电台 `InternetRadio` 模块。
- 移除第一版菜单里的数学游戏入口。
- 保留 `bread-compact-wifi-lcd` 板级菜单入口：`XIAOZHI / RADIO`。
- 保留 app-only 烧录策略，默认写入 `ota_0` 的 `0x100000` 地址，避免清空 NVS/Wi-Fi 配置。

### 工程化

- 项目版本号调整为 `1.0.0`。
- 新增项目说明、构建烧录说明、固件发布流程、测试清单。
- 新增当前交接本和发布交接模板。
- 新增本地固件打包脚本 `scripts/package_firmware.ps1`。

### 发布状态

- 已在本机使用 ESP-IDF v5.5.0 构建 v1.0.0 固件。
- 已通过 `COM5` 烧录到 ESP32-S3 开发板。
- 固件 SHA256：`ce36eff695443cde5b85f809e683e0808d0a398939e253904db38422cb0a737d`。
- 发布包：`releases/xiaozhi-s3-radio-math-v1.0.0-bread-compact-wifi-lcd-20260706-231632.zip`。
