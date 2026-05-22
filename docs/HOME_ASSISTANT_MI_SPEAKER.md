# Home Assistant Mi Speaker Integration

This firmware exposes a `HomeAssistant` IoT thing so the LLM can control a Xiaomi Mi AI Speaker through Home Assistant.

## Supported Commands

The `HomeAssistant` thing currently provides these methods:

- `ExecuteTextCommand`: send a natural-language command to the Mi AI Speaker, for example "打开客厅灯".
- `PlayText`: make the Mi AI Speaker read a text aloud.
- `WakeUpSpeaker`: wake the Mi AI Speaker.
- `PlayMusic`: ask the Mi AI Speaker to play music.
- `StopAlarm`: stop the Mi AI Speaker alarm.
- `SetSleepMode`: turn Mi AI Speaker sleep mode on or off.
- `SetSpeakerVolume`: set Mi AI Speaker volume from 0 to 100.

## Home Assistant Defaults

The current integration expects Home Assistant at:

```text
http://192.168.3.200:8123
```

Default Xiaomi speaker entities:

```text
media_player.xiaomi_s12_8133_play_control
button.xiaomi_s12_8133_play_music
text.xiaomi_s12_8133_play_text
button.xiaomi_s12_8133_wake_up
switch.xiaomi_s12_8133_sleep_mode
button.xiaomi_s12_8133_stop_alarm
text.xiaomi_s12_8133_execute_text_directive
```

## Private Token Setup

Do not commit a Home Assistant long-lived access token to git.

Copy:

```text
main/iot/things/home_assistant_config.example.h
```

to:

```text
main/iot/things/home_assistant_config.local.h
```

Then fill in:

```c
#define HOME_ASSISTANT_TOKEN "paste-your-home-assistant-long-lived-access-token-here"
```

`home_assistant_config.local.h` is ignored by git.

## Build

From the ASCII build path:

```powershell
Set-Location 'C:\espbuild\xz-sfx'
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-racing build
```

## App-Only Flash

```powershell
Set-Location 'C:\espbuild\xz-sfx'
$env:PYTHONUTF8='1'
& 'C:\Users\Administrator\esp-idf\export.ps1' | Out-Null
python -m esptool --chip esp32s3 -p COM4 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x100000 build-racing\xiaozhi.bin
```

This preserves NVS and Wi-Fi configuration.
