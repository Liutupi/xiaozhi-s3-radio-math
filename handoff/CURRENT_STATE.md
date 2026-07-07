# 当前交接状态

更新时间：2026-07-07

## 当前目标

项目继续作为长期迭代固件维护。当前最新固件为 `v1.0.1`，在 `v1.0.0` 小智 + 网络电台基线之上，新增网易云 MCP 音乐播放桥接。

## 当前版本

- 项目版本：`v1.0.1`
- 主力固件：`bread-compact-wifi-lcd`
- 芯片：ESP32-S3
- LCD：ST7735 128x160
- app-only 烧录地址：`0x100000`

## 当前保留/新增功能

- 小智主体语音对话。
- 网络电台 `InternetRadio`。
- 主菜单入口：`XIAOZHI / RADIO / SETTINGS`。
- 极简动态表情：两个眼睛 + 一个嘴巴，待机眨眼/视线移动，说话时嘴巴动画。
- 设置界面：音量、亮度、返回。
- 设备侧 `MusicPlayer` IoT 工具：支持 `PlayUrl(url, title)` 播放 MCP 返回的直链 MP3。
- 数学游戏不参与当前固件主流程。

## 网易云 MCP 播放链路

NAS/小智后台负责“搜索和拿播放链接”，固件负责“播放链接”。

- NAS 容器：`xiaozhi-netease-duoduo`
- 容器状态：已克隆并运行
- 镜像：`jks0831/node18-alpine`
- 挂载：`/docker/xiaozhi-mcp-services/netease-music` -> `/app`
- 启动命令：`node ./xiaozhi-ws-mcp.js`
- 小智后台：多多已接入网易云 MCP
- 固件工具：`MusicPlayer.PlayUrl`

多多角色提示词建议补充：

```text
当网易云音乐 MCP 找到可播放的直链 MP3 URL 后，必须调用设备工具 MusicPlayer.PlayUrl(url, title) 在开发板扬声器播放，不要只回复“马上播放”。
```

## 已确认源码位置

- 小智主流程：`main/application.*`
- 网络电台/音乐 URL 播放器：`main/games/internet_radio.*`
- 板级入口：`main/boards/bread-compact-wifi-lcd/compact_wifi_board_lcd.cc`
- IoT 参数解析：`main/iot/thing.cc`
- LCD 动态表情：`main/display/lcd_display.*`
- 构建清单：`main/CMakeLists.txt`
- 分区表：`partitions.csv`

## 关键硬件规则

- `GPIO40` 是 LCD DC，不能改成按键。
- `GPIO0` 是 BOOT。
- `GPIO39` 是右侧下面按键。
- I2S 音频引脚沿用 `GPIO4/5/6/7`。
- app 分区 `ota_0` 从 `0x100000` 开始。

## 当前本机状态

- 仓库：`D:\xiaozhi-s3`
- ESP-IDF v5.5.0：`D:\3.5inch_ESP32-S3\Espressif\v5.5\esp-idf`
- Espressif 工具链：`C:\Espressif\tools`
- 构建目录：`D:\xiaozhi-s3\build-v1`
- 最新 app 固件：`D:\xiaozhi-s3\build-v1\xiaozhi.bin`
- 最新固件 SHA256：`7e58272bbf5f504c7c5602bfceb5d0666349b2096f9318ad52e749d28a8801ea`
- 最新发布包：`D:\xiaozhi-s3\releases\xiaozhi-s3-radio-math-v1.0.1-bread-compact-wifi-lcd-20260707-234553.zip`

## 最近烧录状态

- 串口：`COM5`
- 芯片：ESP32-S3 QFN56 revision v0.2
- MAC：`14:c1:9f:cb:53:60`
- 烧录方式：app-only，写入 `0x100000 build-v1\xiaozhi.bin`，保留 NVS/Wi-Fi 配置。
- 烧录结果：成功，`Hash of data verified`，最后 hard reset。

## 下次接手优先级

1. 实测：“多多，播放一首网易云音乐”，确认是否调用 `MusicPlayer.PlayUrl` 并出声。
2. 如果仍只回复“马上播放”，先检查多多角色提示词是否明确要求调用 `MusicPlayer.PlayUrl`。
3. 若 MCP 返回的不是 MP3 直链，固件会无法播放，需要在 NAS MCP 侧转成可直连 MP3 URL 或扩展固件解码格式。
4. 按 `docs/TEST_CHECKLIST.md` 补齐小智、电台、设置页、音乐播放的实机测试记录。
