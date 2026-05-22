#include "iot/thing.h"
#include "board.h"
#include "http.h"

#include <esp_log.h>

#include <cstdio>
#include <string>

#define TAG "HomeAssistant"

#if __has_include("home_assistant_config.local.h")
#include "home_assistant_config.local.h"
#endif

#ifndef HOME_ASSISTANT_BASE_URL
#define HOME_ASSISTANT_BASE_URL "http://192.168.3.200:8123"
#endif

#ifndef HOME_ASSISTANT_TOKEN
#define HOME_ASSISTANT_TOKEN ""
#endif

#ifndef HOME_ASSISTANT_PLAY_CONTROL_ENTITY
#define HOME_ASSISTANT_PLAY_CONTROL_ENTITY "media_player.xiaomi_s12_8133_play_control"
#endif

#ifndef HOME_ASSISTANT_PLAY_MUSIC_ENTITY
#define HOME_ASSISTANT_PLAY_MUSIC_ENTITY "button.xiaomi_s12_8133_play_music"
#endif

#ifndef HOME_ASSISTANT_PLAY_TEXT_ENTITY
#define HOME_ASSISTANT_PLAY_TEXT_ENTITY "text.xiaomi_s12_8133_play_text"
#endif

#ifndef HOME_ASSISTANT_WAKE_UP_ENTITY
#define HOME_ASSISTANT_WAKE_UP_ENTITY "button.xiaomi_s12_8133_wake_up"
#endif

#ifndef HOME_ASSISTANT_SLEEP_MODE_ENTITY
#define HOME_ASSISTANT_SLEEP_MODE_ENTITY "switch.xiaomi_s12_8133_sleep_mode"
#endif

#ifndef HOME_ASSISTANT_STOP_ALARM_ENTITY
#define HOME_ASSISTANT_STOP_ALARM_ENTITY "button.xiaomi_s12_8133_stop_alarm"
#endif

#ifndef HOME_ASSISTANT_EXECUTE_TEXT_DIRECTIVE_ENTITY
#define HOME_ASSISTANT_EXECUTE_TEXT_DIRECTIVE_ENTITY "text.xiaomi_s12_8133_execute_text_directive"
#endif

namespace {
constexpr const char* kHaBaseUrl = HOME_ASSISTANT_BASE_URL;
constexpr const char* kHaToken = HOME_ASSISTANT_TOKEN;
constexpr const char* kPlayControlEntity = HOME_ASSISTANT_PLAY_CONTROL_ENTITY;
constexpr const char* kPlayMusicEntity = HOME_ASSISTANT_PLAY_MUSIC_ENTITY;
constexpr const char* kPlayTextEntity = HOME_ASSISTANT_PLAY_TEXT_ENTITY;
constexpr const char* kWakeUpEntity = HOME_ASSISTANT_WAKE_UP_ENTITY;
constexpr const char* kSleepModeEntity = HOME_ASSISTANT_SLEEP_MODE_ENTITY;
constexpr const char* kStopAlarmEntity = HOME_ASSISTANT_STOP_ALARM_ENTITY;
constexpr const char* kExecuteTextDirectiveEntity = HOME_ASSISTANT_EXECUTE_TEXT_DIRECTIVE_ENTITY;

std::string JsonEscape(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size() + 8);
    for (char c : input) {
        switch (c) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += c;
                break;
        }
    }
    return escaped;
}

std::string EntityBody(const std::string& entity_id) {
    return "{\"entity_id\":\"" + JsonEscape(entity_id) + "\"}";
}

std::string TextBody(const std::string& entity_id, const std::string& value) {
    return "{\"entity_id\":\"" + JsonEscape(entity_id) + "\",\"value\":\"" + JsonEscape(value) + "\"}";
}

bool CallService(const std::string& domain, const std::string& service, const std::string& body) {
    auto http = Board::GetInstance().CreateHttp();
    std::string auth = "Bearer ";
    auth += kHaToken;
    http->SetHeader("Authorization", auth);
    http->SetHeader("Content-Type", "application/json");

    const std::string url = std::string(kHaBaseUrl) + "/api/services/" + domain + "/" + service;
    const bool ok = http->Open("POST", url, body);
    if (!ok) {
        ESP_LOGE(TAG, "Home Assistant service call failed: %s/%s", domain.c_str(), service.c_str());
        delete http;
        return false;
    }

    ESP_LOGI(TAG, "Home Assistant service called: %s/%s", domain.c_str(), service.c_str());
    http->Close();
    delete http;
    return true;
}
}  // namespace

namespace iot {

class HomeAssistant : public Thing {
public:
    HomeAssistant() : Thing("HomeAssistant", "通过 Home Assistant 控制小米音箱和米家设备") {
        methods_.AddMethod("ExecuteTextCommand", "让小米音箱执行一句米家语音指令，例如打开客厅灯、关闭空调、播放音乐", ParameterList({
            Parameter("command", "要转发给小米音箱执行的中文指令", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            CallService("text", "set_value", TextBody(kExecuteTextDirectiveEntity, parameters["command"].string()));
        });

        methods_.AddMethod("PlayText", "让小米音箱朗读一段文字", ParameterList({
            Parameter("text", "要朗读的中文文本", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            CallService("text", "set_value", TextBody(kPlayTextEntity, parameters["text"].string()));
        });

        methods_.AddMethod("WakeUpSpeaker", "唤醒小米音箱", ParameterList(), [this](const ParameterList& parameters) {
            CallService("button", "press", EntityBody(kWakeUpEntity));
        });

        methods_.AddMethod("PlayMusic", "让小米音箱播放音乐", ParameterList(), [this](const ParameterList& parameters) {
            CallService("button", "press", EntityBody(kPlayMusicEntity));
        });

        methods_.AddMethod("StopAlarm", "停止小米音箱闹钟", ParameterList(), [this](const ParameterList& parameters) {
            CallService("button", "press", EntityBody(kStopAlarmEntity));
        });

        methods_.AddMethod("SetSleepMode", "打开或关闭小米音箱睡眠模式", ParameterList({
            Parameter("enabled", "是否开启睡眠模式", kValueTypeBoolean, true)
        }), [this](const ParameterList& parameters) {
            CallService("switch", parameters["enabled"].boolean() ? "turn_on" : "turn_off", EntityBody(kSleepModeEntity));
        });

        methods_.AddMethod("SetSpeakerVolume", "设置小米音箱音量，范围 0 到 100", ParameterList({
            Parameter("volume", "0到100之间的整数", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            int volume = parameters["volume"].number();
            if (volume < 0) {
                volume = 0;
            } else if (volume > 100) {
                volume = 100;
            }
            char body[128];
            snprintf(body, sizeof(body), "{\"entity_id\":\"%s\",\"volume_level\":%.2f}",
                kPlayControlEntity, volume / 100.0f);
            CallService("media_player", "volume_set", body);
        });
    }
};

}  // namespace iot

DECLARE_THING(HomeAssistant);
