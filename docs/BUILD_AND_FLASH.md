# 构建与烧录

## 推荐环境

- Windows PowerShell
- ESP-IDF 5.3 或以上，优先沿用已验证过的 ESP-IDF 5.5 环境
- 工程路径建议使用短 ASCII 路径，例如：

```powershell
C:\espbuild\xz-sfx
```

不要从中文桌面路径直接编译。此前 ESP-IDF 在中文/长路径下出现过 Python 编码、ccache 和文件系统问题。

## 目标硬件

- 芯片：ESP32-S3
- Flash：16MB
- PSRAM：8MB
- 板型：`CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD=y`
- LCD：`CONFIG_LCD_ST7735_128X160=y`
- app 分区：`ota_0`
- app 烧录地址：`0x100000`

## 构建

本机已验证的 ESP-IDF 路径：

```text
D:\3.5inch_ESP32-S3\Espressif\v5.5\esp-idf
```

本机已验证的构建命令：

```powershell
cmd.exe /c "set IDF_PATH=D:\3.5inch_ESP32-S3\Espressif\v5.5\esp-idf&& set IDF_TOOLS_PATH=C:\Espressif\tools&& set IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v5.5\venv&& set PYTHONUTF8=1&& set PATH=C:\Espressif\tools\ccache\4.10.2\ccache-4.10.2-windows-x86_64;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;C:\Espressif\tools\python\v5.5\venv\Scripts;C:\Espressif\tools\git\cmd;C:\Espressif\tools\git\bin;%PATH%&& C:\Espressif\tools\python\v5.5\venv\Scripts\python.exe D:\3.5inch_ESP32-S3\Espressif\v5.5\esp-idf\tools\idf.py -B build-v1 build"
```

如果 ESP-IDF 安装在别的位置，把上面的路径替换为本机实际路径。

## 只烧录 app 分区

这个方式会保留 NVS、Wi-Fi、设备配置。

```powershell
Set-Location 'C:\espbuild\xz-sfx'
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1' | Out-Null
python -m esptool --chip esp32s3 -p COM4 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x100000 build-v1\xiaozhi.bin
```

如果 Windows 重新分配了串口，把 `COM4` 替换为设备管理器里显示的 ESP32-S3 串口。

## 打包发布固件

构建成功后执行：

```powershell
.\scripts\package_firmware.ps1 -AppBin .\build-v1\xiaozhi.bin -Version 1.0.0 -Board bread-compact-wifi-lcd
```

脚本会在 `releases\` 下生成 zip 包，并写入：

- `firmware\xiaozhi.bin`
- `manifest.json`
- `SHA256SUMS.txt`
- `flash_app_only.ps1`
- `README_FLASH.md`
