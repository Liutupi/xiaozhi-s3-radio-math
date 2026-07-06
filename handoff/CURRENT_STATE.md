# 当前交接状态

更新时间：2026-07-06

## 当前目标

把项目整理为第一版长期迭代基线：`v1.0.0`。

## 当前版本

- 项目版本：`1.0.0`
- 主力硬件：`bread-compact-wifi-lcd`
- 芯片：ESP32-S3
- LCD：ST7735 128x160
- app-only 烧录地址：`0x100000`

## 第一版保留功能

- 小智主体语音对话。
- 网络电台 `InternetRadio`。
- 主菜单入口：`XIAOZHI / RADIO`。
- 数学游戏不参与 v1.0.0 构建。

## 已确认源码位置

- 小智主流程：`main/application.*`
- 网络电台：`main/games/internet_radio.*`
- 板级入口：`main/boards/bread-compact-wifi-lcd/compact_wifi_board_lcd.cc`
- 构建清单：`main/CMakeLists.txt`
- 分区表：`partitions.csv`

## 关键硬件规则

- `GPIO40` 是 LCD DC，不能改成按键。
- `GPIO0` 是 BOOT。
- `GPIO39` 是右侧下面按键。
- I2S 音频引脚沿用 `GPIO4/5/6/7`。
- app 分区 `ota_0` 从 `0x100000` 开始。

## 当前本机状态

- 仓库已检出到 `D:\xiaozhi-s3`。
- ESP-IDF v5.5.0 位于 `D:\3.5inch_ESP32-S3\Espressif\v5.5\esp-idf`。
- Espressif 工具链位于 `C:\Espressif\tools`。
- 本次构建目录：`D:\xiaozhi-s3\build-v1`。
- 本次 app 固件：`D:\xiaozhi-s3\build-v1\xiaozhi.bin`。
- 固件 SHA256：`ce36eff695443cde5b85f809e683e0808d0a398939e253904db38422cb0a737d`。
- 发布包：`D:\xiaozhi-s3\releases\xiaozhi-s3-radio-math-v1.0.0-bread-compact-wifi-lcd-20260706-231632.zip`。

## 本次烧录状态

- 开发板串口：`COM5`。
- 芯片：ESP32-S3 QFN56 revision v0.2。
- MAC：`14:c1:9f:cb:53:60`。
- 烧录方式：写入 bootloader、partition table、ota data、srmodels、app，不擦除 NVS。
- 烧录结果：成功，所有段均 `Hash of data verified`，最后 hard reset。

## 下次接手优先级

1. 观察设备启动后 LCD、按键、语音、小智、网络电台是否正常。
2. 按 `docs/TEST_CHECKLIST.md` 补齐人工实机测试勾选。
3. 创建 GitHub Release `v1.0.0` 并上传 zip。
4. 如需重新构建，沿用 `docs/BUILD_AND_FLASH.md`，必要时参考本文件里的 ESP-IDF 路径。
