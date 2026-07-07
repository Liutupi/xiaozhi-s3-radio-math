# Changelog

## v1.0.1 - 2026-07-07

### 新增

- 新增设备侧 `MusicPlayer` IoT 工具，支持小智调用 `PlayUrl(url, title)` 播放 MCP 返回的直链 MP3。
- `InternetRadio` 复用为通用 MP3 URL 播放器，支持网易云 MCP 搜歌后的设备侧播放桥接。
- 新增设置页入口，支持音量、亮度调节。
- 更新 LCD 主页面为极简动态表情：待机眨眼/视线移动，说话时嘴巴动画。

### 修复

- 修复 IoT 可选参数未传时仍读取导致崩溃的问题。
- 播放新电台/音乐前会先停止旧音频流，避免“找到但不播放”。

### 发布状态

- 已在本机使用 ESP-IDF v5.5.0 构建。
- 已通过 `COM5` app-only 烧录到 ESP32-S3 开发板。
- 固件 SHA256：`7e58272bbf5f504c7c5602bfceb5d0666349b2096f9318ad52e749d28a8801ea`。
- 发布包：`releases/xiaozhi-s3-radio-math-v1.0.1-bread-compact-wifi-lcd-20260707-234553.zip`。

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
- 发布包：`releases/xiaozhi-s3-radio-math-v1.0.0-bread-compact-wifi-lcd-20260706-231936.zip`。
