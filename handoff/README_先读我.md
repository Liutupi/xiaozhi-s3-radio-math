# 小智 + 电台 + 网易云 MCP 播放交接入口

这个文件夹用于下次继续开发、编译、烧录或交给其他人/AI 接手。

## 先读这些

- `handoff/CURRENT_STATE.md`：当前项目状态、硬件、固件、烧录结果。
- `handoff/2026-07-07-v1.0.1-netease-mcp-music.md`：本次网易云 MCP 播放桥接交接。
- `handoff/2026-07-06-v1.0.0-xiaozhi-radio-release.md`：v1.0.0 小智 + 电台基线交接。
- `docs/BUILD_AND_FLASH.md`：构建、烧录、打包命令。
- `docs/TEST_CHECKLIST.md`：实机测试清单。

## 当前最新固件

- 版本：`v1.0.1`
- app 固件：`D:\xiaozhi-s3\build-v1\xiaozhi.bin`
- 发布包：`D:\xiaozhi-s3\releases\xiaozhi-s3-radio-math-v1.0.1-bread-compact-wifi-lcd-20260707-234553.zip`
- SHA256：`7e58272bbf5f504c7c5602bfceb5d0666349b2096f9318ad52e749d28a8801ea`

## 当前功能范围

- 小智主体语音对话
- 网络电台
- 设置界面
- 极简动态表情
- 网易云 MCP 搜歌后的设备侧 MP3 播放桥接

设备菜单：

```text
1  XIAOZHI
2  RADIO
3  SETTINGS
```

## 已烧录设备

- 串口：`COM5`
- 芯片：ESP32-S3 QFN56 revision v0.2
- MAC：`14:c1:9f:cb:53:60`
- 最近烧录：`v1.0.1` app-only，保留 NVS/Wi-Fi
- 烧录结果：成功，`Hash of data verified`

## 关键规则

- `GPIO40` 是 LCD DC，绝对不要改成按键。
- `GPIO0` 是 BOOT。
- `GPIO39` 是右侧下面按键。
- `ota_0` app 地址是 `0x100000`。
- app-only 烧录不会清除 NVS/Wi-Fi。
