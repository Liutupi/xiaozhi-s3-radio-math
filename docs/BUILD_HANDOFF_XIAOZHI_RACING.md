# Xiaozhi Racing Firmware Build Handoff

This document is the authoritative build handoff for this ESP32-S3 Xiaozhi racing firmware project. It records the exact hardware profile, source paths, build commands, flash commands, and known pitfalls from the successful 2026-05-11 compile and flash session.

## Quick Summary

- Build from the short ASCII path: `C:\espbuild\xz-sfx`
- Do not build from the original Chinese desktop path; ESP-IDF tools previously failed there with Python GBK decode and ccache/filesystem errors.
- ESP-IDF root: `C:\Users\Administrator\esp-idf`
- Target chip: ESP32-S3
- Board config: `CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_LCD=y`
- LCD config: `CONFIG_LCD_ST7735_128X160=y`
- Working serial port: `COM4`
- Flash only the app partition at `0x100000` to preserve NVS and existing Wi-Fi/device config.
- Final app binary: `C:\espbuild\xz-sfx\build-racing\xiaozhi.bin`
- Final binary SHA256: `3598EA8B4C4D28DF8F1A3088CDD4EFAE18E47782A86B16D7916AFC304BE2D4A4`

## Source Locations

- Original package: `C:\Users\Administrator\Desktop\xiaozhi-racing-firmware-package-20260511-172351.zip`
- Active working project: `C:\espbuild\xz-sfx`
- Earlier extracted source: under the desktop workspace `_temp\firmware-src\xiaozhi-racing-firmware-package-20260511-172351\xiaozhi-esp32-v146`; avoid compiling from that Chinese-path location.
- Use `C:\espbuild\xz-sfx` for all future builds unless the path issue is intentionally retested and fixed.

## ESP-IDF Setup

ESP-IDF is installed at:

```powershell
C:\Users\Administrator\esp-idf
```

If the ESP-IDF Python virtual environment is missing or `idf.py` dependencies fail, reinstall the toolchain for ESP32-S3:

```powershell
& 'C:\Users\Administrator\esp-idf\install.ps1' esp32s3
```

Before every build or flash command, export the ESP-IDF environment and force UTF-8 mode:

```powershell
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1'
```

The successful build logs reported ESP-IDF 5.5.0 and `IDF_VER` as `v5.5-dirty`. The exported `idf.py --version` output may print `v1.0.3` in this environment, but the project build itself uses ESP-IDF 5.5.

## Build Command

Run from `C:\espbuild\xz-sfx`:

```powershell
Set-Location 'C:\espbuild\xz-sfx'
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-racing build
```

The successful final build produced:

- Binary: `C:\espbuild\xz-sfx\build-racing\xiaozhi.bin`
- Size on disk: 3,567,216 bytes
- Build-reported app size: `0x366e70`
- Partition headroom: about 43% free in the 6M OTA partition

## Flash Command

Use app-only flashing so NVS, Wi-Fi credentials, and other device data are preserved:

```powershell
Set-Location 'C:\espbuild\xz-sfx'
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1' | Out-Null
python -m esptool --chip esp32s3 -p COM4 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x100000 build-racing\xiaozhi.bin
```

A successful flash ends with:

```text
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

## Hardware Profile

- Chip: ESP32-S3
- Package/revision: QFN56, revision v0.2
- USB mode: USB-Serial/JTAG
- Flash size used by flash command: 16MB
- PSRAM: 8MB reported by esptool
- MAC address: `14:c1:9f:cb:53:60`
- Working Windows port: `COM4`
- Windows device label: USB Serial Device on `COM4` (shown as a Chinese localized label in Device Manager)
- USB ID observed: `USB\VID_303A&PID_1001&MI_00\6&2C8464CC&0&0000`
- Non-working/irrelevant ports observed: `COM5`, `COM6`, and `COM7` are Bluetooth; `COM3`/`COM9` were unavailable, busy, or missing.

## Board And Pin Configuration

Board config file:

```text
C:\espbuild\xz-sfx\main\boards\bread-compact-wifi-lcd\config.h
```

Important audio settings:

```c
#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_I2S_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7
```

The active audio path is the default duplex I2S path. `AUDIO_I2S_METHOD_SIMPLEX` is commented out. Simplex pins are present in the file but inactive unless that macro is enabled.

Button pins:

```c
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_39
```

Display pins:

```c
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_42
#define DISPLAY_MOSI_PIN GPIO_NUM_47
#define DISPLAY_CLK_PIN GPIO_NUM_21
#define DISPLAY_DC_PIN GPIO_NUM_40
#define DISPLAY_RST_PIN GPIO_NUM_45
#define DISPLAY_CS_PIN GPIO_NUM_41
```

Other notable pin:

```c
#define BUILTIN_LED_GPIO GPIO_NUM_48
```

Critical GPIO warning: do not use `GPIO40` as a button. It is the LCD DC pin, and assigning it as an input breaks display refresh.

## LCD Configuration

Selected SDK config:

```text
CONFIG_LCD_ST7735_128X160=y
```

Effective ST7735 behavior:

- Resolution: 128 x 160
- Mirror X: true
- Mirror Y: true
- Swap XY: false
- Invert color: false
- RGB element order: RGB
- X offset: 0
- Y offset: 0
- SPI mode: 0

## Partition Table

The active partition file is:

```text
C:\espbuild\xz-sfx\partitions.csv
```

Partition layout:

| Name | Type | SubType | Offset | Size |
| --- | --- | --- | --- | --- |
| `nvs` | data | nvs | `0x9000` | `0x4000` |
| `otadata` | data | ota | `0xd000` | `0x2000` |
| `phy_init` | data | phy | `0xf000` | `0x1000` |
| `model` | data | spiffs | `0x10000` | `0xF0000` |
| `ota_0` | app | ota_0 | `0x100000` | `6M` |
| `ota_1` | app | ota_1 | `0x700000` | `6M` |

Because `ota_0` starts at `0x100000`, app-only flashing writes `build-racing\xiaozhi.bin` to `0x100000`.

## Current Firmware Features

Racing game files:

- `C:\espbuild\xz-sfx\main\games\racing_game.h`
- `C:\espbuild\xz-sfx\main\games\racing_game.cc`
- `C:\espbuild\xz-sfx\main\games\racing_sfx.h`
- `C:\espbuild\xz-sfx\main\games\racing_sfx.cc`

Build registration:

- `C:\espbuild\xz-sfx\main\CMakeLists.txt`

Board integration:

- `C:\espbuild\xz-sfx\main\boards\bread-compact-wifi-lcd\compact_wifi_board_lcd.cc`

Robot dynamic UI:

- `C:\espbuild\xz-sfx\main\display\lcd_display.h`
- `C:\espbuild\xz-sfx\main\display\lcd_display.cc`

Implemented behavior:

- Long press BOOT (`GPIO0`) enters or exits the racing game.
- BOOT click while racing moves left.
- BOOT click while crashed/game over restarts the game.
- BOOT double-click also moves left.
- Right-side lower key (`GPIO39`) moves right.
- Racing game has sound effects for start, move, score, crash, and restart.
- Racing game uses LVGL-rendered car sprites instead of plain square blocks.
- Track has neon edge lines and colored lane marks.
- Xiaozhi UI includes a dynamic robot avatar inspired by the user-provided robot image, with animated face/screen/ears/energy bar and emotion mapping.

## Backup Artifact

The last known successful app-only binary backup placed on the desktop was:

```text
C:\Users\Administrator\Desktop\xiaozhi-racing-better-cars-app-20260511-223917.bin
```

## Common Pitfalls

- Avoid Chinese or long desktop paths for ESP-IDF builds. Use `C:\espbuild\xz-sfx`.
- Always set `$env:PYTHONUTF8='1'` before ESP-IDF commands in PowerShell.
- Always run `export.ps1` in the current shell before `idf.py`.
- Use `COM4` for this connected board unless Windows re-enumerates the device.
- Do not use Bluetooth COM ports for flashing.
- Do not use `GPIO40` as an input or gameplay button.
- Prefer app-only flashing at `0x100000` unless a full erase/repartition is intentionally required.
- If a model or tool suggests `idf.py flash`, make sure it will not erase NVS or write the wrong partition. The verified command above is safer for continuing this device.

## Recommended Future Workflow

1. Edit source under `C:\espbuild\xz-sfx`.
2. Build with `idf.py -B build-racing build`.
3. Flash app-only to `COM4` at `0x100000`.
4. If controls are changed, verify that BOOT remains left/restart and `GPIO39` remains right.
5. If display code is changed, verify that `GPIO40` is still only LCD DC.
6. If audio code is changed, keep I2S pins `4/5/6/7` unless the physical board wiring is intentionally changed.
