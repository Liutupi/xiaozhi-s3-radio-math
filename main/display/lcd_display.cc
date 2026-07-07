#include "lcd_display.h"

#include <algorithm>
#include <vector>
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <string_view>
#include "assets/lang_config.h"

#include "board.h"

#define TAG "LcdDisplay"

LV_FONT_DECLARE(font_awesome_30_4);

namespace {
void FillPanel(esp_lcd_panel_handle_t panel, int width, int height, uint16_t color) {
    std::vector<uint16_t> buffer(width, color);
    for (int y = 0; y < height; y++) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, width, y + 1, buffer.data());
    }
}

void StyleBox(lv_obj_t* obj, lv_color_t bg, lv_color_t border, int border_width, int radius) {
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, border, 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void StyleNoBg(lv_obj_t* obj) {
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

}  // namespace

SpiLcdDisplay::SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));
    FillPanel(panel_, width_, height_, 0x0000);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        FillPanel(panel_, width_, height_, 0xF81F);
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();
}

// RGB LCD实现
RgbLcdDisplay::RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;
    
    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = true,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .swap_bytes = 0,
            .full_refresh = 1,
            .direct_mode = 1,
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        }
    };
    
    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add RGB display");
        return;
    }
    
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    SetupUI();
}

LcdDisplay::~LcdDisplay() {
    if (robot_timer_ != nullptr) {
        esp_timer_stop(robot_timer_);
        esp_timer_delete(robot_timer_);
        robot_timer_ = nullptr;
    }

    // 然后再清�?LVGL 对象
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
}

bool LcdDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplay::Unlock() {
    lvgl_port_unlock();
}

void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xd8f3ff), 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x08111f), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0x08111f), 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x101729), 0);
    lv_obj_set_style_text_color(status_bar_, lv_color_hex(0xd8f3ff), 0);
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_bg_color(content_, lv_color_hex(0x08111f), 0);
    lv_obj_set_style_border_width(content_, 0, 0);

    lv_obj_set_layout(content_, LV_LAYOUT_NONE);

    CreateRobotAvatar();

    emotion_label_ = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, "");
    lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0xb9f7ff), 0);
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.92);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模�?
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐
    lv_obj_set_style_bg_color(chat_message_label_, lv_color_hex(0x08111f), 0);
    lv_obj_set_style_bg_opa(chat_message_label_, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(chat_message_label_, 2, 0);
    lv_obj_align(chat_message_label_, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    lv_obj_t* low_battery_label = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);
    lv_obj_center(low_battery_label);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void LcdDisplay::CreateRobotAvatar() {
    robot_avatar_ = lv_obj_create(content_);
    const int avatar_width = std::clamp(width_ * 92 / 100, 150, 300);
    const int available_height = height_ - static_cast<int>(fonts_.text_font->line_height);
    const int avatar_height = std::clamp(available_height * 62 / 100, 120, 250);
    lv_obj_set_size(robot_avatar_, avatar_width, avatar_height);
    lv_obj_align(robot_avatar_, LV_ALIGN_CENTER, 0, -10);
    StyleNoBg(robot_avatar_);

    robot_left_eye_ = lv_obj_create(robot_avatar_);
    StyleBox(robot_left_eye_, lv_color_hex(0x55efff), lv_color_hex(0x55efff), 0, 24);

    robot_right_eye_ = lv_obj_create(robot_avatar_);
    StyleBox(robot_right_eye_, lv_color_hex(0x55efff), lv_color_hex(0x55efff), 0, 24);

    robot_mouth_ = lv_obj_create(robot_avatar_);
    StyleBox(robot_mouth_, lv_color_hex(0x55efff), lv_color_hex(0x55efff), 0, 16);

    ApplyRobotEmotion("neutral");

    const esp_timer_create_args_t robot_timer_args = {
        .callback = [](void* arg) {
            auto display = static_cast<LcdDisplay*>(arg);
            if (display->Lock(0)) {
                display->UpdateRobotAvatar();
                display->Unlock();
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "robot_face",
        .skip_unhandled_events = true,
    };

    if (esp_timer_create(&robot_timer_args, &robot_timer_) == ESP_OK) {
        esp_timer_start_periodic(robot_timer_, 180000);
    }
}

void LcdDisplay::ApplyRobotEmotion(const char* emotion) {
    if (robot_avatar_ == nullptr) {
        return;
    }

    std::string_view next_emotion(emotion == nullptr ? "neutral" : emotion);
    if (next_emotion == "speaking") {
        robot_speaking_ = true;
    } else {
        robot_emotion_.assign(next_emotion.data(), next_emotion.size());
        if (next_emotion == "neutral" || next_emotion == "sleepy" || next_emotion == "relaxed") {
            robot_speaking_ = false;
        }
    }
    UpdateRobotAvatar();
}

void LcdDisplay::Update() {
    Display::Update();
}

void LcdDisplay::UpdateRobotAvatar() {
    if (robot_avatar_ == nullptr) {
        return;
    }

    robot_phase_ = (robot_phase_ + 1) % 48;
    const int face_w = lv_obj_get_width(robot_avatar_);
    const int face_h = lv_obj_get_height(robot_avatar_);

    std::string_view emotion(robot_emotion_);
    lv_color_t face = lv_color_hex(0x55efff);
    if (emotion == "happy" || emotion == "laughing" || emotion == "funny" || emotion == "loving") {
        face = lv_color_hex(0x79ffb2);
    } else if (emotion == "sad" || emotion == "crying") {
        face = lv_color_hex(0x7cc8ff);
    } else if (emotion == "angry") {
        face = lv_color_hex(0xff5d6c);
    } else if (emotion == "thinking" || emotion == "confused") {
        face = lv_color_hex(0x9bdcff);
    }

    const bool bright = (robot_phase_ % 14) < 9;
    if (!bright && emotion == "neutral") {
        face = lv_color_hex(0x22cde9);
    }

    const int gaze = (robot_phase_ >= 7 && robot_phase_ <= 15) ? -4 :
                     (robot_phase_ >= 28 && robot_phase_ <= 36) ? 4 : 0;
    const bool blink = !robot_speaking_ && (robot_phase_ == 18 || robot_phase_ == 19 || robot_phase_ == 43);
    const bool sleepy = emotion == "sleepy" || emotion == "relaxed";
    const int scaled_gaze = gaze * std::max(face_w, 160) / 112;
    int eye_w = face_w * 24 / 100;
    int eye_h = blink ? std::max(face_h * 4 / 100, 5) : (sleepy ? face_h * 7 / 100 : face_h * 28 / 100);
    int eye_y = blink ? face_h * 31 / 100 : (sleepy ? face_h * 30 / 100 : face_h * 18 / 100);
    int left_eye_x = face_w * 18 / 100 + scaled_gaze;
    int right_eye_x = face_w * 58 / 100 + scaled_gaze;
    int mouth_w = face_w * 24 / 100;
    int mouth_h = std::max(face_h * 4 / 100, 5);
    int mouth_y = face_h * 73 / 100;

    if (robot_speaking_) {
        const int talk_frame = robot_phase_ % 6;
        mouth_w = (talk_frame == 0 || talk_frame == 3) ? face_w * 22 / 100 :
                  (talk_frame == 1 || talk_frame == 4) ? face_w * 34 / 100 : face_w * 28 / 100;
        mouth_h = (talk_frame == 0 || talk_frame == 3) ? face_h * 5 / 100 :
                  (talk_frame == 1 || talk_frame == 4) ? face_h * 14 / 100 : face_h * 19 / 100;
        mouth_y = face_h * 70 / 100 - mouth_h / 5;
    } else if (emotion == "happy" || emotion == "laughing" || emotion == "funny" || emotion == "loving") {
        eye_h = blink ? std::max(face_h * 4 / 100, 5) : face_h * 13 / 100;
        eye_y = blink ? face_h * 31 / 100 : face_h * 25 / 100;
        mouth_w = face_w * 42 / 100;
        mouth_h = std::max(face_h * 5 / 100, 6);
    } else if (emotion == "surprised" || emotion == "shocked") {
        eye_w = face_w * 26 / 100;
        eye_h = blink ? std::max(face_h * 4 / 100, 5) : face_h * 30 / 100;
        eye_y = blink ? face_h * 31 / 100 : face_h * 17 / 100;
        mouth_w = face_w * 17 / 100;
        mouth_h = face_h * 18 / 100;
        mouth_y = face_h * 66 / 100;
    } else if (emotion == "sad" || emotion == "crying") {
        eye_h = blink ? std::max(face_h * 4 / 100, 5) : face_h * 12 / 100;
        eye_y = blink ? face_h * 31 / 100 : face_h * 30 / 100;
        mouth_w = face_w * 24 / 100;
        mouth_h = std::max(face_h * 4 / 100, 5);
        mouth_y = face_h * 76 / 100;
    } else if (emotion == "angry") {
        eye_w = face_w * 27 / 100;
        eye_h = blink ? std::max(face_h * 4 / 100, 5) : face_h * 10 / 100;
        eye_y = blink ? face_h * 31 / 100 : face_h * 30 / 100;
        mouth_w = face_w * 30 / 100;
        mouth_h = std::max(face_h * 4 / 100, 5);
    }

    const int mouth_x = (face_w - mouth_w) / 2;
    lv_obj_set_size(robot_left_eye_, eye_w, eye_h);
    lv_obj_set_size(robot_right_eye_, eye_w, eye_h);
    lv_obj_set_pos(robot_left_eye_, left_eye_x, eye_y);
    lv_obj_set_pos(robot_right_eye_, right_eye_x, eye_y);
    lv_obj_set_style_bg_color(robot_left_eye_, face, 0);
    lv_obj_set_style_bg_color(robot_right_eye_, face, 0);
    lv_obj_set_style_bg_color(robot_mouth_, face, 0);
    lv_obj_set_style_radius(robot_left_eye_, std::min(eye_w, eye_h) / 2, 0);
    lv_obj_set_style_radius(robot_right_eye_, std::min(eye_w, eye_h) / 2, 0);
    lv_obj_set_style_radius(robot_mouth_, std::min(mouth_w, mouth_h) / 2, 0);
    lv_obj_set_size(robot_mouth_, mouth_w, mouth_h);
    lv_obj_set_pos(robot_mouth_, mouth_x, mouth_y);
}

void LcdDisplay::SetEmotion(const char* emotion) {
    {
        DisplayLockGuard lock(this);
        ApplyRobotEmotion(emotion);
        if (emotion_label_ != nullptr) {
            lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    return;

    struct Emotion {
        const char* icon;
        const char* text;
    };

    static const std::vector<Emotion> emotions = {
        {"😶", "neutral"},
        {"🙂", "happy"},
        {"😆", "laughing"},
        {"😂", "funny"},
        {"😔", "sad"},
        {"😠", "angry"},
        {"😭", "crying"},
        {"😍", "loving"},
        {"😳", "embarrassed"},
        {"😯", "surprised"},
        {"😱", "shocked"},
        {"🤔", "thinking"},
        {"😉", "winking"},
        {"😎", "cool"},
        {"😌", "relaxed"},
        {"🤤", "delicious"},
        {"😘", "kissy"},
        {"😏", "confident"},
        {"😴", "sleepy"},
        {"😜", "silly"},
        {"🙄", "confused"}
    };
    
    // 查找匹配的表�?
    std::string_view emotion_view(emotion);
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion_view](const Emotion& e) { return e.text == emotion_view; });

    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }

    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    lv_obj_set_style_text_font(emotion_label_, fonts_.emoji_font, 0);
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);
    } else {
        lv_label_set_text(emotion_label_, "😶");
    }
}

void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ != nullptr) {
        lv_label_set_text(chat_message_label_, "");
        lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
    }

    if (role != nullptr && std::string_view(role) == "assistant" && content != nullptr && content[0] != '\0') {
        robot_speaking_ = true;
    } else if (role != nullptr && (std::string_view(role) == "system" || std::string_view(role) == "user")) {
        robot_speaking_ = false;
    }
    UpdateRobotAvatar();
}

void LcdDisplay::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, icon);
}
