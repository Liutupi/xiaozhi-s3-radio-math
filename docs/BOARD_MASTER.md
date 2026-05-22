# 🎮 Xiaozhi Racing Board — Master Reference

> **用途**: 任何 AI 或开发者读完本文档后，应能直接为此开发板编写代码、编译固件、烧录运行。
> **最后更新**: 2026-05-12

---

## 1. Hardware Identity

| 项目 | 值 |
|------|-----|
| **芯片** | ESP32-S3 QFN56, revision v0.2 |
| **PSRAM** | 8MB (AP_3v3 模式) |
| **Flash** | 16MB |
| **USB** | USB-Serial/JTAG |
| **MAC** | `14:c1:9f:cb:53:60` |
| **Windows 串口** | `COM4` |
| **开发板型号** | `bread-compact-wifi-lcd` |
| **项目路径** | `C:\espbuild\xz-sfx` |

---

## 2. Display (LCD)

| 配置项 | 值 |
|--------|-----|
| **驱动芯片** | ST7735 |
| **分辨率** | 128 × 160 |
| **色彩深度** | RGB565 (16-bit) |
| **SPI 模式** | Mode 0 |
| **Mirror X** | true |
| **Mirror Y** | true |
| **Swap XY** | false |
| **Invert** | false |
| **X offset** | 0 |
| **Y offset** | 0 |
| **LVGL 版本** | v9.x（通过 `lvgl__lvgl` 组件引入） |
| **LVGL 渲染** | 全屏 LVGL 对象，`lv_screen_active()` 为根 |

### LCD Pin Map

| 信号 | GPIO |
|------|------|
| MOSI | 47 |
| CLK | 21 |
| DC | 40 |
| RST | 45 |
| CS | 41 |
| **背光** | 42 |

> ⚠️ **GPIO40 严禁用作按键输入！** 它是 LCD DC 引脚，占用会导致屏幕不刷新。

---

## 3. Audio

| 配置项 | 值 |
|--------|-----|
| 输入采样率 | 16000 Hz |
| 输出采样率 | 24000 Hz |
| 接口 | 双工 I2S |

### I2S Pin Map

| 信号 | GPIO |
|------|------|
| WS | 4 |
| BCLK | 5 |
| DIN (MIC) | 6 |
| DOUT (SPK) | 7 |

### 音效架构

音效通过 `RacingSfx` 单例播放，基于方波合成（`AppendTone` 函数）：
- 在 `racing_sfx.h` 的 `RacingSfxEvent` 枚举中添加事件
- 在 `racing_sfx.cc` 的 `BuildSound` 中添加对应波形
- 调用 `RacingSfx::GetInstance().Play(RacingSfxEvent::kXxx)`

---

## 4. Physical Buttons

| 按键 | GPIO | 功能（赛车游戏） |
|------|------|-----------------|
| **BOOT** | 0 | 单击 = 左移 / 死亡后重开 / 长按进出游戏 |
| **右下方按键** | 39 | 单击 = 右移 / 死亡后重开 |
| **音量加** | NC | 未连接 |
| **LED** | 48 | 内置指示灯 |

> 所有按钮通过板级代码 `compact_wifi_board_lcd.cc` 统一管理，游戏模块通过 `HandleClick()` / `MoveLeft()` / `MoveRight()` 接口接收输入。

---

## 5. Build Environment

### ESP-IDF
- **版本**: v5.5-dirty
- **安装路径**: `C:\Users\Administrator\esp-idf`
- **Python 环境**: `C:\Users\Administrator\.espressif\python_env\idf5.5_py3.14_env`
- **工具链**: `xtensa-esp-elf` (esp-14.2.0)

### Build Commands

```powershell
# 1. 设置环境
$env:PYTHONUTF8 = '1'
$env:IDF_PATH   = 'C:\Users\Administrator\esp-idf'
$env:IDF_PYTHON_ENV_PATH = 'C:\Users\Administrator\.espressif\python_env\idf5.5_py3.14_env'

# 2. 构建 PATH（必须包含以下工具目录）
$tools = @(
    'C:\Users\Administrator\.espressif\tools\cmake\3.30.2\bin',
    'C:\Users\Administrator\.espressif\tools\ninja\1.12.1',
    'C:\Users\Administrator\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin',
    'C:\Users\Administrator\.espressif\tools\idf-exe\1.0.3',
    "$env:IDF_PYTHON_ENV_PATH\Scripts"
)
$env:PATH = ($tools -join ';') + ';' + $env:PATH

# 3. 编译
python C:\Users\Administrator\esp-idf\tools\idf.py -C C:\espbuild\xz-sfx -B build-racing build
```

### Flash Command（只写 APP 分区，保留 NVS/WiFi 配置）

```powershell
python -m esptool --chip esp32s3 -p COM4 -b 460800 `
  --before default_reset --after hard_reset `
  write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m `
  0x100000 C:\Users\Administrator\build-racing\xiaozhi.bin
```

> ⚠️ 不要用中文路径或长路径编译 ESP-IDF。使用 `C:\espbuild\xz-sfx`。
> ⚠️ 不要用 `idf.py flash` 全量烧录，会擦除 NVS 分区（WiFi 配置会丢失）。

---

## 6. Partition Table

| 分区名 | 类型 | 子类型 | 起始地址 | 大小 |
|--------|------|--------|----------|------|
| nvs | data | nvs | `0x9000` | 16KB |
| otadata | data | ota | `0xd000` | 8KB |
| phy_init | data | phy | `0xf000` | 4KB |
| model | data | spiffs | `0x10000` | 960KB |
| **ota_0** | app | ota_0 | **`0x100000`** | **6MB** |
| ota_1 | app | ota_1 | `0x700000` | 6MB |

> APP 烧录地址 = `0x100000`。当前固件 3.5MB，剩余 2.5MB 空间。

---

## 7. Project Architecture

### 目录结构

```
C:\espbuild\xz-sfx\
├── main/
│   ├── CMakeLists.txt          ← 注册新源文件在这里
│   ├── main.cc                 ← 入口
│   ├── application.cc/h        ← 应用层
│   ├── display/                ← 显示驱动
│   ├── audio_codecs/           ← 音频编解码
│   ├── boards/                 ← 板级配置
│   │   ├── common/             ← 通用板级代码
│   │   └── bread-compact-wifi-lcd/  ← 本板的 config.h 和 .cc
│   ├── games/                  ← ★ 游戏模块放这里
│   │   ├── racing_game.h/cc    ← 赛车游戏
│   │   └── racing_sfx.h/cc     ← 赛车音效
│   ├── protocols/              ← MQTT/WebSocket
│   └── iot/                    ← IoT 物模型
├── docs/
│   ├── BOARD_MASTER.md         ← ★ 本文档
│   └── BUILD_HANDOFF_XIAOZHI_RACING.md
├── partitions.csv
└── CMakeLists.txt
```

### 如何添加新游戏

1. 在 `main/games/` 下创建 `xxx_game.h` 和 `xxx_game.cc`
2. 类结构参考 `RacingGame`：

```cpp
class MyGame {
public:
    void Start(Display* display);   // 初始化，创建 LVGL UI
    void Stop();                     // 清理资源
    bool HandleClick();              // BOOT 按钮单击
    bool HandleDoubleClick();        // BOOT 双击
    bool MoveRight();               // GPIO39 按下
    bool MoveLeft();                // BOOT 按下（和 HandleClick 不同）
    bool IsRunning() const;
private:
    void Tick();                     // 每帧逻辑（用 esp_timer 驱动）
    void CreateUi();                 // 创建 LVGL 对象
    // ...
};
```

3. 在 `main/CMakeLists.txt` 中添加新 `.cc` 文件
4. 在板级代码 `boards/bread-compact-wifi-lcd/compact_wifi_board_lcd.cc` 中注册按键处理
5. 编译烧录

### LVGL 渲染模式

- 使用 `lv_obj_create()` 创建对象，`lv_obj_set_pos/size()` 定位
- 颜色用 `lv_color_hex(0xRRGGBB)`，格式为 RGB565
- 常用辅助函数（已存在于 `racing_game.cc` 中，可复用）:
  - `AddBlock(parent, x, y, w, h, color, radius)` — 创建纯色色块
  - `StylePlain(obj, color)` — 设纯色背景无边框
  - `StyleTransparent(obj)` — 透明无边框

### NVS (非易失存储)

项目已初始化 NVS，可直接读写：
```cpp
#include <nvs.h>

nvs_handle_t handle;
nvs_open("mynamespace", NVS_READWRITE, &handle);
nvs_set_i32(handle, "key", value);
nvs_commit(handle);
nvs_close(handle);
```

---

## 8. Display Constraints Quick Reference

| 参数 | 值 |
|------|-----|
| 屏幕宽 | 128px |
| 屏幕高 | 160px |
| 当前道路起点 Y | 36px |
| 车道宽 | ~28px (road_w / 3) |
| 画面刷新 | LVGL 自动，游戏用 50ms 定时器 (20fps) |
| 可用内存 | PSRAM 8MB，游戏只用几十 KB |

---

## 9. Button Input Flow

```
硬件按键 → 板级 debounce → compact_wifi_board_lcd.cc
  → HandleClick()     // BOOT 单击
  → HandleDoubleClick() // BOOT 双击
  → MoveLeft()         // BOOT 按下(与 click 共用)
  → MoveRight()        // GPIO39
```

---

## 10. One-Liner for AI

> 我是 ESP32-S3 `bread-compact-wifi-lcd` 开发板，128×160 ST7735 LCD，GPIO0/39 两个按键，I2S 音频在 GPIO4-7，LCD 引脚 GPIO21/40/41/42/45/47。用 ESP-IDF 5.5 在 `C:\espbuild\xz-sfx` 编译，`COM4` 串口烧录 APP 到 `0x100000`。完整文档在 `docs/BOARD_MASTER.md`。
