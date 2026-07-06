# 小智 + 网络电台 v1.0 交接入口

这个文件夹用于下次继续开发、编译、烧录或交给其他人/AI 接手。

## 当前最新状态

先读：

- `handoff/CURRENT_STATE.md`：当前项目状态、硬件、固件、烧录结果。
- `handoff/2026-07-06-v1.0.0-xiaozhi-radio-release.md`：本次 v1.0.0 构建与烧录交接。
- `docs/BUILD_AND_FLASH.md`：构建、烧录、打包命令。
- `docs/TEST_CHECKLIST.md`：实机测试清单。

## v1.0.0 范围

只保留：

- 小智主体
- 网络电台

不包含：

- 数学游戏菜单入口
- 赛车游戏菜单入口

设备菜单应只显示：

```text
1  XIAOZHI
2  RADIO
```

## 最终固件

- app 固件：`D:\xiaozhi-s3\build-v1\xiaozhi.bin`
- 发布包：`D:\xiaozhi-s3\releases\xiaozhi-s3-radio-math-v1.0.0-bread-compact-wifi-lcd-20260706-231632.zip`
- SHA256：`ce36eff695443cde5b85f809e683e0808d0a398939e253904db38422cb0a737d`

## 已烧录设备

- 串口：`COM5`
- 芯片：ESP32-S3 QFN56 revision v0.2
- MAC：`14:c1:9f:cb:53:60`
- 烧录结果：成功，所有写入段均 `Hash of data verified`

## 关键规则

- `GPIO40` 是 LCD DC，绝对不要改成按键。
- `GPIO0` 是 BOOT。
- `GPIO39` 是右侧下面按键。
- `ota_0` app 地址是 `0x100000`。
- 本次烧录写入 bootloader、partition table、ota data、srmodels、app，没有擦除 NVS。
