# v1.0.0 测试清单

## 构建检查

- [ ] `PROJECT_VER` 是 `1.0.0`。
- [ ] `main/CMakeLists.txt` 包含 `games/internet_radio.cc`。
- [ ] `main/CMakeLists.txt` 不包含 `games/math_game.cc`。
- [ ] 构建成功生成 `xiaozhi.bin`。
- [ ] 记录固件 SHA256。

## 烧录检查

- [ ] 使用 app-only 烧录。
- [ ] 烧录地址是 `0x100000`。
- [ ] 未执行全量擦除。
- [ ] 烧录后设备能正常重启。
- [ ] 原有 Wi-Fi/NVS 配置未丢失。

## 硬件检查

- [ ] LCD 正常点亮。
- [ ] LCD 方向正确。
- [ ] BOOT / GPIO0 按键正常。
- [ ] GPIO39 按键正常。
- [ ] `GPIO40` 未被配置成按键。
- [ ] 音频输入/输出正常。

## 功能检查

- [ ] 小智主体可进入。
- [ ] 小智语音对话可用。
- [ ] 网络电台可进入。
- [ ] 网络电台可播放。
- [ ] 网络电台可停止或退出回主流程。
- [ ] 菜单中只能看到 `XIAOZHI / RADIO`。
- [ ] 菜单中看不到 `MATH`。

## 发布检查

- [ ] zip 包包含 `firmware/xiaozhi.bin`。
- [ ] zip 包包含 `manifest.json`。
- [ ] zip 包包含 `SHA256SUMS.txt`。
- [ ] zip 包包含 `flash_app_only.ps1`。
- [ ] zip 包包含 `README_刷机说明.md`。
- [ ] GitHub Release 附件上传的是本次新构建的 zip，不是历史旧 bin。
