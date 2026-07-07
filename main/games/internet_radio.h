#ifndef INTERNET_RADIO_H_
#define INTERNET_RADIO_H_

#include "audio_codecs/audio_codec.h"
#include "display.h"

#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>

#include <cstdint>
#include <string>
#include <vector>

class InternetRadio {
public:
    InternetRadio() = default;
    ~InternetRadio();

    void Start(Display* display, AudioCodec* codec);
    void Start(Display* display, AudioCodec* codec, const std::string& station_name);
    void StartUrl(Display* display, AudioCodec* codec, const std::string& url, const std::string& title);
    void Stop();
    std::string GetStationCatalog();
    bool HandleClick();
    bool HandleDoubleClick();
    bool MoveRight();
    bool IsRunning() const { return running_; }

private:
    enum class State : uint8_t {
        kIdle,
        kConnecting,
        kBuffering,
        kPlaying,
        kHlsUnsupported,
        kError,
    };

    void CreateUi();
    void DestroyUi();
    void UpdateUi();
    void SetState(State state);
    bool SelectStationByName(const std::string& station_name);
    void SwitchStation(int delta);
    void StopVoiceSession();

    static void StreamTaskEntry(void* arg);
    void StreamLoop();

    Display* display_ = nullptr;
    AudioCodec* codec_ = nullptr;
    bool running_ = false;
    bool paused_ = false;
    bool reconnect_now_ = false;
    State state_ = State::kIdle;
    int station_index_ = 0;
    int bitrate_kbps_ = 0;
    int sample_rate_ = 0;
    int channels_ = 0;
    bool custom_stream_ = false;
    std::string custom_url_;
    std::string custom_title_;

    TaskHandle_t stream_task_ = nullptr;
    esp_http_client_handle_t current_client_ = nullptr;
    std::vector<int16_t> output_chunk_;

    lv_obj_t* layer_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* wifi_label_ = nullptr;
    lv_obj_t* index_label_ = nullptr;
    lv_obj_t* name_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* info_label_ = nullptr;
    lv_obj_t* next_label_ = nullptr;
};

#endif  // INTERNET_RADIO_H_
