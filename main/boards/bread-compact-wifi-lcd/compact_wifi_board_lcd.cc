#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "games/internet_radio.h"
#include "games/math_game.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>

#include <cstdint>

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

class CompactWifiBoardLCD : public WifiBoard {
private:
 
    Button boot_button_;
    Button volume_down_button_;
    LcdDisplay* display_;
    MathGame math_game_;
    InternetRadio internet_radio_;
    enum class LauncherMode : uint8_t {
        kMenu,
        kXiaozhi,
        kMath,
        kRadio,
    };
    LauncherMode launcher_mode_ = LauncherMode::kMenu;
    int launcher_index_ = 0;
    lv_obj_t* launcher_layer_ = nullptr;
    static constexpr int kLauncherItemCount = 3;
    lv_obj_t* launcher_items_[kLauncherItemCount] = {};
    lv_obj_t* launcher_labels_[kLauncherItemCount] = {};

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
            "1  XIAOZHI", "2  MATH", "3  RADIO"
        };
        static const uint32_t kItemColors[] = {
            0x1d4ed8, 0x047857, 0xd97706
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

        if (math_game_.IsRunning()) {
            math_game_.Stop();
        }
        if (internet_radio_.IsRunning()) {
            internet_radio_.Stop();
        }

        DisplayLockGuard lock(display_);
        launcher_mode_ = LauncherMode::kMenu;
        if (launcher_layer_ == nullptr) {
            launcher_layer_ = lv_obj_create(lv_screen_active());
            lv_obj_set_size(launcher_layer_, display_->width(), display_->height());
            lv_obj_set_pos(launcher_layer_, 0, 0);
            StyleLauncherBox(launcher_layer_, 0x07111f, 0x07111f, 0, 0);

            AddLauncherLabel(launcher_layer_, 7, 8, display_->width() - 14, "SELECT MODE", 0xffd166, LV_TEXT_ALIGN_CENTER);
            AddLauncherLabel(launcher_layer_, 7, 26, display_->width() - 14, "GPIO39 move", 0x94a3b8, LV_TEXT_ALIGN_CENTER);

            for (int i = 0; i < kLauncherItemCount; ++i) {
                launcher_items_[i] = lv_obj_create(launcher_layer_);
                lv_obj_set_size(launcher_items_[i], display_->width() - 18, 17);
                lv_obj_set_pos(launcher_items_[i], 9, 31 + i * 19);
                StyleLauncherBox(launcher_items_[i], 0x16345f, 0x334155, 1, 5);
                launcher_labels_[i] = AddLauncherLabel(launcher_items_[i], 8, 0, display_->width() - 34, "", 0xf8fafc);
            }

            AddLauncherLabel(launcher_layer_, 7, display_->height() - 18, display_->width() - 14,
                             "BOOT enter", 0xffd166, LV_TEXT_ALIGN_CENTER);
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

    void MoveLauncherSelection(int delta) {
        if (launcher_mode_ != LauncherMode::kMenu) {
            return;
        }
        DisplayLockGuard lock(display_);
        launcher_index_ = (launcher_index_ + delta + kLauncherItemCount) % kLauncherItemCount;
        DrawLauncher();
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
            launcher_mode_ = LauncherMode::kMath;
            math_game_.Start(display_);
        } else {
            launcher_mode_ = LauncherMode::kRadio;
            internet_radio_.Start(display_, GetAudioCodec());
        }
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
        // 液晶屏控制IO初始化
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
            if (math_game_.HandleClick()) {
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
            if (math_game_.HandleDoubleClick()) {
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
            if (math_game_.MoveRight()) {
                return;
            }
            if (internet_radio_.MoveRight()) {
                return;
            }
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
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
