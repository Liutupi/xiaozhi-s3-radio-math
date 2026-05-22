#ifndef HOME_ASSISTANT_CONFIG_EXAMPLE_H
#define HOME_ASSISTANT_CONFIG_EXAMPLE_H

// Copy this file to home_assistant_config.local.h and fill in your private token.
// home_assistant_config.local.h is ignored by git and must not be pushed.

#define HOME_ASSISTANT_BASE_URL "http://192.168.3.200:8123"
#define HOME_ASSISTANT_TOKEN "paste-your-home-assistant-long-lived-access-token-here"

#define HOME_ASSISTANT_PLAY_CONTROL_ENTITY "media_player.xiaomi_s12_8133_play_control"
#define HOME_ASSISTANT_PLAY_MUSIC_ENTITY "button.xiaomi_s12_8133_play_music"
#define HOME_ASSISTANT_PLAY_TEXT_ENTITY "text.xiaomi_s12_8133_play_text"
#define HOME_ASSISTANT_WAKE_UP_ENTITY "button.xiaomi_s12_8133_wake_up"
#define HOME_ASSISTANT_SLEEP_MODE_ENTITY "switch.xiaomi_s12_8133_sleep_mode"
#define HOME_ASSISTANT_STOP_ALARM_ENTITY "button.xiaomi_s12_8133_stop_alarm"
#define HOME_ASSISTANT_EXECUTE_TEXT_DIRECTIVE_ENTITY "text.xiaomi_s12_8133_execute_text_directive"

#endif  // HOME_ASSISTANT_CONFIG_EXAMPLE_H
