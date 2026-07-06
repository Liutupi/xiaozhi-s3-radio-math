# 项目说明：xiaozhi-s3-radio-math

## 项目定位

`xiaozhi-s3-radio-math` 是基于小智 ESP32 项目的 ESP32-S3 派生固件，第一版目标是保留小智语音对话能力，同时稳定提供网络电台入口。

## v1.0.0 范围

第一版只做稳定基线，不扩大硬件支持面。

- 小智主体：保留原有语音对话、显示、网络连接和配置能力。
- 网络电台：保留 `main/games/internet_radio.*`，频道列表默认来自 `stations.json`。
- 数学游戏：第一版不进入菜单、不参与构建，后续版本再决定是否恢复。
- 主力硬件：`bread-compact-wifi-lcd`。
- 主力屏幕：ST7735 128x160 LCD。
- 主力芯片：ESP32-S3，16MB Flash，8MB PSRAM。

## 当前入口

板级代码位于：

```text
main/boards/bread-compact-wifi-lcd/compact_wifi_board_lcd.cc
```

当前菜单入口：

```text
1  XIAOZHI
2  RADIO
```

## 关键约束

- `GPIO40` 是 LCD DC，不要改成按键输入。
- app-only 烧录地址是 `0x100000`。
- 默认保留 NVS/Wi-Fi 配置，不使用会清空设备数据的全量擦除流程。
- ESP-IDF 建议使用短 ASCII 路径，例如 `C:\espbuild\xz-sfx`。

## 后续方向

- 网络电台：频道管理、播放状态、断线恢复。
- 小智主体：保持和上游能力同步，避免为了游戏破坏语音主流程。
