# 小智赛车编译交接包

这个文件夹用于下次继续编译、烧录或交给其他大模型接手这个 ESP32-S3 小智赛车固件。

## 文件说明

- `docs\BUILD_HANDOFF_XIAOZHI_RACING.md`：最重要，完整硬件、GPIO、ESP-IDF、编译、烧录、避坑说明。
- `firmware\xiaozhi-racing-app-20260511.bin`：本次成功编译出的 app 固件。
- `firmware\xiaozhi-racing-better-cars-app-20260511-223917.bin`：桌面备份版 app 固件，内容与本次成功版本对应。
- `memory-note\2026-05-11-xiaozhi-racing-build-handoff.md`：给 Codex/其他模型用的精简记忆条目。
- `xiaozhi-racing-firmware-package-20260511-172351.zip`：原始固件工程包。

## 关键路径

- 当前可成功编译的工程路径：`C:\espbuild\xz-sfx`
- ESP-IDF 路径：`C:\Users\Administrator\esp-idf`
- 最终 app 固件路径：`C:\espbuild\xz-sfx\build-racing\xiaozhi.bin`

不要从中文桌面路径直接编译，之前 ESP-IDF 在该路径下遇到过编码和 ccache/filesystem 问题。

## 最短编译命令

```powershell
Set-Location 'C:\espbuild\xz-sfx'
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-racing build
```

## 最短烧录命令

只烧录 app 分区，保留 NVS/Wi-Fi 配置：

```powershell
Set-Location 'C:\espbuild\xz-sfx'
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1' | Out-Null
python -m esptool --chip esp32s3 -p COM4 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x100000 build-racing\xiaozhi.bin
```

## 核心硬件信息

- 芯片：ESP32-S3
- 串口：`COM4`
- 板型：`CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD=y`
- 屏幕：`CONFIG_LCD_ST7735_128X160=y`
- BOOT 键：`GPIO0`，赛车里向左/重启
- 右侧下面的键：`GPIO39`，赛车里向右
- `GPIO40` 是 LCD DC，绝对不要改成按钮

详细内容看 `docs\BUILD_HANDOFF_XIAOZHI_RACING.md`。
