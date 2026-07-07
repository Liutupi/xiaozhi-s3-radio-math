#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "games/internet_radio.h"
#include "iot/thing.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>
#include <functional>
#include <string>

#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif
 
#define TAG "CompactWifiBoardLCD"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

namespace {
class InternetRadioThing : public iot::Thing {
public:
    InternetRadioThing(std::function<void()> play,
                       std::function<void(const std::string&)> play_station,
                       std::function<std::string()> station_catalog,
                       std::function<void()> stop,
                       std::function<void()> next,
                       std::function<bool()> is_running)
        : Thing("InternetRadio", "Internet radio player. It can play, stop, and switch stations."),
          play_(std::move(play)),
          play_station_(std::move(play_station)),
          station_catalog_(std::move(station_catalog)),
          stop_(std::move(stop)),
          next_(std::move(next)),
          is_running_(std::move(is_running)) {
        properties_.AddBooleanProperty("playing", "Whether internet radio is playing", [this]() -> bool {
            return is_running_();
        });
        properties_.AddStringProperty("station_catalog", "Available station names. User can request any station in this catalog by name.", [this]() -> std::string {
            return station_catalog_();
        });

        methods_.AddMethod("Play", "Play internet radio when user says play radio or open radio without a specific station name", iot::ParameterList(), [this](const iot::ParameterList&) {
            play_();
        });

        methods_.AddMethod("PlayStation", "Play any specific radio station by station_name. Use this whenever the user says a station name; pass the requested name exactly as heard. If no station name is provided, use Play instead", iot::ParameterList({
            iot::Parameter("station_name", "Radio station name requested by user", iot::kValueTypeString, true)
        }), [this](const iot::ParameterList& parameters) {
            play_station_(parameters["station_name"].string());
        });

        methods_.AddMethod("Stop", "Stop internet radio", iot::ParameterList(), [this](const iot::ParameterList&) {
            stop_();
        });

        methods_.AddMethod("Next", "Switch to the next radio station", iot::ParameterList(), [this](const iot::ParameterList&) {
            next_();
        });
    }

private:
    std::function<void()> play_;
    std::function<void(const std::string&)> play_station_;
    std::function<std::string()> station_catalog_;
    std::function<void()> stop_;
    std::function<void()> next_;
    std::function<bool()> is_running_;
};

class MusicPlayerThing : public iot::Thing {
public:
    MusicPlayerThing(std::function<void(const std::string&, const std::string&)> play_url,
                     std::function<void()> stop,
                     std::function<bool()> is_running)
        : Thing("MusicPlayer",
                "Device music player. When a music MCP, NetEase Cloud Music MCP, or any tool returns a playable direct MP3 URL, call PlayUrl to play it on this device speaker. Do not only say it will play."),
          play_url_(std::move(play_url)),
          stop_(std::move(stop)),
          is_running_(std::move(is_running)) {
        properties_.AddBooleanProperty("playing", "Whether device music playback is currently running", [this]() -> bool {
            return is_running_();
        });

        methods_.AddMethod("PlayUrl",
            "Play a direct HTTP or HTTPS MP3 audio URL on the device speaker. Use this after NetEase Cloud Music MCP finds a playable URL.",
            iot::ParameterList({
                iot::Parameter("url", "Direct playable MP3 audio URL returned by the music MCP", iot::kValueTypeString, true),
                iot::Parameter("title", "Song title or display name", iot::kValueTypeString, false),
            }),
            [this](const iot::ParameterList& parameters) {
                play_url_(parameters["url"].string(), parameters["title"].string());
            });

        methods_.AddMethod("Stop", "Stop device music playback", iot::ParameterList(), [this](const iot::ParameterList&) {
            stop_();
        });
    }

private:
    std::function<void(const std::string&, const std::string&)> play_url_;
    std::function<void()> stop_;
    std::function<bool()> is_running_;
};
}  // namespace

class CompactWifiBoardLCD : public WifiBoard {
private:
 
    Button boot_button_;
    Button volume_down_button_;
    LcdDisplay* display_;
    InternetRadio internet_radio_;
    enum class LauncherMode : uint8_t {
        kMenu,
        kXiaozhi,
        kRadio,
        kSettings,
    };
    LauncherMode launcher_mode_ = LauncherMode::kMenu;
    int launcher_index_ = 0;
    lv_obj_t* launcher_layer_ = nullptr;
    static constexpr int kLauncherItemCount = 3;
    lv_obj_t* launcher_items_[kLauncherItemCount] = {};
    lv_obj_t* launcher_labels_[kLauncherItemCount] = {};
    static constexpr int kSettingsItemCount = 3;
    int settings_index_ = 0;
    lv_obj_t* settings_layer_ = nullptr;
    lv_obj_t* settings_items_[kSettingsItemCount] = {};
    lv_obj_t* settings_labels_[kSettingsItemCount] = {};
    lv_obj_t* settings_value_labels_[kSettingsItemCount] = {};

    void StyleLauncherBox(lv_obj_t* obj, uint32_t bg, uint32_t border, int border_width, int radius) {
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(obj, lv_color_hex(border), 0);
        lv_obj_set_style_border_width(obj, border_width, 0);
        lv_obj_set_style_radius(obj, radius, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
    }

    lv_obj_t* AddLauncherLabel(lv_obj_t* parent, int x, int y, int w, const char* text, uint32_t color,
                               lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
        lv_obj_t* label = lv_label_create(parent);
        lv_obj_set_pos(label, x, y);
        lv_obj_set_width(label, w);
        lv_obj_set_style_text_font(label, &font_puhui_16_4, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        lv_obj_set_style_text_align(label, align, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_label_set_text(label, text);
        return label;
    }

    void DrawLauncher() {
        static const char* kItems[] = {
            "1  XIAOZHI", "2  RADIO", "3  SETTINGS"
        };
        static const uint32_t kItemColors[] = {
            0x1d4ed8, 0xd97706, 0x0f766e
        };
        for (int i = 0; i < kLauncherItemCount; ++i) {
            const bool active = i == launcher_index_;
            StyleLauncherBox(launcher_items_[i], active ? 0xfff1c2 : kItemColors[i],
                             active ? 0xffd166 : 0x334155, active ? 2 : 1, 5);
            lv_obj_set_style_text_color(launcher_labels_[i], lv_color_hex(active ? 0x111827 : 0xf8fafc), 0);
            lv_label_set_text(launcher_labels_[i], kItems[i]);
        }
    }

    void ShowLauncher() {
        if (display_ == nullptr) {
            return;
        }

        if (internet_radio_.IsRunning()) {
            internet_radio_.Stop();
        }

        DisplayLockGuard lock(display_);
        launcher_mode_ = LauncherMode::kMenu;
        HideSettings();
        if (launcher_layer_ == nullptr) {
            launcher_layer_ = lv_obj_create(lv_screen_active());
            lv_obj_set_size(launcher_layer_, display_->width(), display_->height());
            lv_obj_set_pos(launcher_layer_, 0, 0);
            StyleLauncherBox(launcher_layer_, 0x07111f, 0x07111f, 0, 0);

            AddLauncherLabel(launcher_layer_, 7, 8, display_->width() - 14, "SELECT MODE", 0xffd166, LV_TEXT_ALIGN_CENTER);
            AddLauncherLabel(launcher_layer_, 7, 26, display_->width() - 14, "GPIO39 move", 0x94a3b8, LV_TEXT_ALIGN_CENTER);

            const int item_h = display_->height() >= 220 ? 28 : 17;
            const int item_gap = display_->height() >= 220 ? 10 : 2;
            const int start_y = display_->height() >= 220 ? 56 : 31;
            for (int i = 0; i < kLauncherItemCount; ++i) {
                launcher_items_[i] = lv_obj_create(launcher_layer_);
                lv_obj_set_size(launcher_items_[i], display_->width() - 18, item_h);
                lv_obj_set_pos(launcher_items_[i], 9, start_y + i * (item_h + item_gap));
                StyleLauncherBox(launcher_items_[i], 0x16345f, 0x334155, 1, 5);
                launcher_labels_[i] = AddLauncherLabel(launcher_items_[i], 8, (item_h - 16) / 2, display_->width() - 34, "", 0xf8fafc);
            }

        }

        lv_obj_clear_flag(launcher_layer_, LV_OBJ_FLAG_HIDDEN);
        DrawLauncher();
        lv_obj_move_foreground(launcher_layer_);
    }

    void HideLauncher() {
        if (launcher_layer_ != nullptr) {
            lv_obj_add_flag(launcher_layer_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    int ClampPercent(int value) {
        if (value < 0) {
            return 0;
        }
        if (value > 100) {
            return 100;
        }
        return value;
    }

    int CurrentBrightness() {
        auto backlight = GetBacklight();
        return backlight == nullptr ? 0 : backlight->brightness();
    }

    void DrawSettings() {
        if (settings_layer_ == nullptr) {
            return;
        }

        auto codec = GetAudioCodec();
        const int volume = codec == nullptr ? 0 : codec->output_volume();
        const int brightness = CurrentBrightness();
        char volume_text[16];
        char brightness_text[16];
        snprintf(volume_text, sizeof(volume_text), "%d%%", volume);
        snprintf(brightness_text, sizeof(brightness_text), "%d%%", brightness);

        static const char* kLabels[] = {"Volume", "Brightness", "Back"};
        const char* values[] = {volume_text, brightness_text, ""};
        for (int i = 0; i < kSettingsItemCount; ++i) {
            const bool active = i == settings_index_;
            if (!active) {
                lv_obj_add_flag(settings_items_[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }

            const int item_h = display_->height() >= 220 ? 96 : 64;
            const int item_y = display_->height() >= 220 ? 86 : 54;
            lv_obj_clear_flag(settings_items_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(settings_items_[i], display_->width() - 24, item_h);
            lv_obj_set_pos(settings_items_[i], 12, item_y);
            StyleLauncherBox(settings_items_[i], active ? 0xd1fae5 : 0x102033,
                             active ? 0x5eead4 : 0x334155, active ? 2 : 1, 6);
            lv_obj_set_style_text_color(settings_labels_[i], lv_color_hex(active ? 0x102033 : 0xe2e8f0), 0);
            lv_obj_set_style_text_color(settings_value_labels_[i], lv_color_hex(active ? 0x102033 : 0x94a3b8), 0);
            lv_obj_set_pos(settings_labels_[i], 8, item_h >= 90 ? 20 : 10);
            lv_obj_set_width(settings_labels_[i], display_->width() - 40);
            lv_obj_set_style_text_align(settings_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(settings_value_labels_[i], 8, item_h >= 90 ? 54 : 36);
            lv_obj_set_width(settings_value_labels_[i], display_->width() - 40);
            lv_obj_set_style_text_align(settings_value_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(settings_labels_[i], kLabels[i]);
            lv_label_set_text(settings_value_labels_[i], i == 2 ? "Click to return" : values[i]);
        }
    }

    void ShowSettings() {
        if (display_ == nullptr) {
            return;
        }
        if (internet_radio_.IsRunning()) {
            internet_radio_.Stop();
        }

        DisplayLockGuard lock(display_);
        HideLauncher();
        launcher_mode_ = LauncherMode::kSettings;

        if (settings_layer_ == nullptr) {
            settings_layer_ = lv_obj_create(lv_screen_active());
            lv_obj_set_size(settings_layer_, display_->width(), display_->height());
            lv_obj_set_pos(settings_layer_, 0, 0);
            StyleLauncherBox(settings_layer_, 0x07111f, 0x07111f, 0, 0);

            AddLauncherLabel(settings_layer_, 7, 10, display_->width() - 14, "SETTINGS", 0x5eead4, LV_TEXT_ALIGN_CENTER);
            AddLauncherLabel(settings_layer_, 7, 32, display_->width() - 14, "BOOT +  GPIO39 -", 0x94a3b8, LV_TEXT_ALIGN_CENTER);

            const int item_h = display_->height() >= 220 ? 96 : 64;
            for (int i = 0; i < kSettingsItemCount; ++i) {
                settings_items_[i] = lv_obj_create(settings_layer_);
                lv_obj_set_size(settings_items_[i], display_->width() - 24, item_h);
                lv_obj_set_pos(settings_items_[i], 12, display_->height() >= 220 ? 86 : 54);
                StyleLauncherBox(settings_items_[i], 0x102033, 0x334155, 1, 6);
                settings_labels_[i] = AddLauncherLabel(settings_items_[i], 8, item_h >= 90 ? 20 : 10,
                                                       display_->width() - 40, "", 0xe2e8f0, LV_TEXT_ALIGN_CENTER);
                settings_value_labels_[i] = AddLauncherLabel(settings_items_[i], 8, item_h >= 90 ? 54 : 36,
                                                             display_->width() - 40, "", 0x94a3b8, LV_TEXT_ALIGN_CENTER);
            }
        }

        lv_obj_clear_flag(settings_layer_, LV_OBJ_FLAG_HIDDEN);
        DrawSettings();
        lv_obj_move_foreground(settings_layer_);
    }

    void HideSettings() {
        if (settings_layer_ != nullptr) {
            lv_obj_add_flag(settings_layer_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void MoveLauncherSelection(int delta) {
        if (launcher_mode_ != LauncherMode::kMenu) {
            return;
        }
        DisplayLockGuard lock(display_);
        launcher_index_ = (launcher_index_ + delta + kLauncherItemCount) % kLauncherItemCount;
        DrawLauncher();
    }

    void MoveSettingsSelection(int delta) {
        if (launcher_mode_ != LauncherMode::kSettings) {
            return;
        }
        DisplayLockGuard lock(display_);
        settings_index_ = (settings_index_ + delta + kSettingsItemCount) % kSettingsItemCount;
        DrawSettings();
    }

    void AdjustSettingsValue(int delta) {
        if (launcher_mode_ != LauncherMode::kSettings) {
            return;
        }

        auto codec = GetAudioCodec();
        if (settings_index_ == 0 && codec != nullptr) {
            codec->SetOutputVolume(ClampPercent(codec->output_volume() + delta));
        } else if (settings_index_ == 1) {
            auto backlight = GetBacklight();
            if (backlight != nullptr) {
                backlight->SetBrightness(static_cast<uint8_t>(ClampPercent(backlight->brightness() + delta)), true);
            }
        } else if (settings_index_ == 2) {
            ShowLauncher();
            return;
        }

        DisplayLockGuard lock(display_);
        DrawSettings();
    }

    void EnterLauncherSelection() {
        if (launcher_mode_ != LauncherMode::kMenu || display_ == nullptr) {
            return;
        }

        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateUpgrading || app.GetDeviceState() == kDeviceStateFatalError) {
            display_->ShowNotification("Mode unavailable now");
            return;
        }

        {
            DisplayLockGuard lock(display_);
            HideLauncher();
        }

        if (launcher_index_ == 0) {
            launcher_mode_ = LauncherMode::kXiaozhi;
            app.DismissAlert();
        } else if (launcher_index_ == 1) {
            launcher_mode_ = LauncherMode::kRadio;
            internet_radio_.Start(display_, GetAudioCodec());
        } else {
            ShowSettings();
        }
    }

    void RunRadioActionAsync(std::function<void()> action) {
        auto* heap_action = new std::function<void()>(std::move(action));
        if (xTaskCreate([](void* arg) {
                auto* action = static_cast<std::function<void()>*>(arg);
                vTaskDelay(pdMS_TO_TICKS(1000));
                (*action)();
                delete action;
                vTaskDelete(nullptr);
            }, "radio_voice", 12288, heap_action, 3, nullptr) != pdPASS) {
            delete heap_action;
            if (display_ != nullptr) {
                display_->ShowNotification("Radio command failed");
            }
        }
    }

    void PrepareVoiceSessionForRadio() {
        auto& app = Application::GetInstance();
        auto state = app.GetDeviceState();
        if (state == kDeviceStateSpeaking) {
            app.AbortSpeaking(kAbortReasonNone);
        }

        for (int i = 0; i < 20; ++i) {
            state = app.GetDeviceState();
            if (state == kDeviceStateIdle || state == kDeviceStateStarting) {
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        state = app.GetDeviceState();
        if (state == kDeviceStateSpeaking || state == kDeviceStateListening ||
            state == kDeviceStateConnecting || state == kDeviceStateActivating) {
            app.SetDeviceState(kDeviceStateIdle);
        }
    }

    void StartInternetRadioNow() {
        if (display_ == nullptr) {
            return;
        }

        PrepareVoiceSessionForRadio();

        if (internet_radio_.IsRunning()) {
            internet_radio_.Stop();
        }

        {
            DisplayLockGuard lock(display_);
            HideLauncher();
            HideSettings();
        }

        launcher_mode_ = LauncherMode::kRadio;
        internet_radio_.Start(display_, GetAudioCodec());
    }

    void StartInternetRadioNow(const std::string& station_name) {
        if (display_ == nullptr) {
            return;
        }

        PrepareVoiceSessionForRadio();

        if (internet_radio_.IsRunning()) {
            internet_radio_.Stop();
        }

        {
            DisplayLockGuard lock(display_);
            HideLauncher();
            HideSettings();
        }

        launcher_mode_ = LauncherMode::kRadio;
        internet_radio_.Start(display_, GetAudioCodec(), station_name);
    }

    void StartMusicUrlNow(const std::string& url, const std::string& title) {
        if (display_ == nullptr || url.empty()) {
            return;
        }

        PrepareVoiceSessionForRadio();

        if (internet_radio_.IsRunning()) {
            internet_radio_.Stop();
        }

        {
            DisplayLockGuard lock(display_);
            HideLauncher();
            HideSettings();
        }

        launcher_mode_ = LauncherMode::kRadio;
        internet_radio_.StartUrl(display_, GetAudioCodec(), url, title);
    }

    void StopInternetRadioNow() {
        if (internet_radio_.IsRunning()) {
            internet_radio_.Stop();
        }
        launcher_mode_ = LauncherMode::kXiaozhi;
    }

    void NextInternetRadioNow() {
        if (!internet_radio_.IsRunning()) {
            StartInternetRadioNow();
            return;
        }
        internet_radio_.MoveRight();
    }

    void StartInternetRadioByVoice() {
        RunRadioActionAsync([this]() { StartInternetRadioNow(); });
    }

    void StartInternetRadioByVoice(const std::string& station_name) {
        RunRadioActionAsync([this, station_name]() { StartInternetRadioNow(station_name); });
    }
    void StartMusicUrlByVoice(const std::string& url, const std::string& title) {
        RunRadioActionAsync([this, url, title]() { StartMusicUrlNow(url, title); });
    }
    void StopInternetRadioByVoice() {
        RunRadioActionAsync([this]() { StopInternetRadioNow(); });
    }

    void NextInternetRadioByVoice() {
        RunRadioActionAsync([this]() { NextInternetRadioNow(); });
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始�?
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };        
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif
        
        esp_lcd_panel_reset(panel);
 

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
#ifdef  LCD_TYPE_GC9A01_SERIAL
        panel_config.vendor_config = &gc9107_vendor_config;
#endif
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,
                                        .icon_font = &font_awesome_16_4,
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
                                    });
    }


 
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (launcher_mode_ == LauncherMode::kMenu) {
                EnterLauncherSelection();
                return;
            }
            if (launcher_mode_ == LauncherMode::kSettings) {
                AdjustSettingsValue(10);
                return;
            }
            if (internet_radio_.HandleClick()) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        boot_button_.OnDoubleClick([this]() {
            if (launcher_mode_ == LauncherMode::kMenu) {
                MoveLauncherSelection(-1);
                return;
            }
            if (launcher_mode_ == LauncherMode::kSettings) {
                AdjustSettingsValue(-10);
                return;
            }
            if (internet_radio_.HandleDoubleClick()) {
                return;
            }
        });

        boot_button_.OnLongPress([this]() {
            if (launcher_mode_ != LauncherMode::kMenu) {
                ShowLauncher();
                return;
            }
            MoveLauncherSelection(1);
        });

        volume_down_button_.OnClick([this]() {
            if (launcher_mode_ == LauncherMode::kMenu) {
                MoveLauncherSelection(1);
                return;
            }
            if (launcher_mode_ == LauncherMode::kSettings) {
                AdjustSettingsValue(-10);
                return;
            }
            if (internet_radio_.MoveRight()) {
                return;
            }
        });

        volume_down_button_.OnLongPress([this]() {
            if (launcher_mode_ == LauncherMode::kSettings) {
                MoveSettingsSelection(1);
                return;
            }
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
        thing_manager.AddThing(new InternetRadioThing(
            [this]() { StartInternetRadioByVoice(); },
            [this](const std::string& station_name) { StartInternetRadioByVoice(station_name); },
            [this]() -> std::string { return internet_radio_.GetStationCatalog(); },
            [this]() { StopInternetRadioByVoice(); },
            [this]() { NextInternetRadioByVoice(); },
            [this]() -> bool { return internet_radio_.IsRunning(); }));
        thing_manager.AddThing(new MusicPlayerThing(
            [this](const std::string& url, const std::string& title) { StartMusicUrlByVoice(url, title); },
            [this]() { StopInternetRadioByVoice(); },
            [this]() -> bool { return internet_radio_.IsRunning(); }));
        thing_manager.AddThing(iot::CreateThing("HomeAssistant"));
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            thing_manager.AddThing(iot::CreateThing("Backlight"));
        }
    }

public:
    CompactWifiBoardLCD() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeSpi();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->SetBrightness(100);
        }
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeIot();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
        ShowLauncher();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(CompactWifiBoardLCD);
