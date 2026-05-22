#include "internet_radio.h"

#include "application.h"
#include "esp_mp3_dec.h"

#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <wifi_station.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

LV_FONT_DECLARE(font_puhui_16_4);

namespace {

static const char* TAG = "InternetRadio";

struct RadioStation {
    enum class StreamType : uint8_t {
        kMp3Direct,
        kHlsM3u8,
    };

    const char* name;
    const char* url;
    StreamType type;
};

static const RadioStation kStations[] = {
    {"Groove Salad", "https://ice5.somafm.com/groovesalad-128-mp3", RadioStation::StreamType::kMp3Direct},
    {"SomaFM Live", "https://ice5.somafm.com/live-128-mp3", RadioStation::StreamType::kMp3Direct},
    {"n5MD Radio", "https://ice5.somafm.com/n5md-128-mp3", RadioStation::StreamType::kMp3Direct},
    {"The In-Sound", "https://ice5.somafm.com/insound-128-mp3", RadioStation::StreamType::kMp3Direct},
    {"Dark Zone", "https://ice5.somafm.com/darkzone-128-mp3", RadioStation::StreamType::kMp3Direct},
    {"Mission Control", "https://ice5.somafm.com/missioncontrol-128-mp3", RadioStation::StreamType::kMp3Direct},
    {"CNR 中国之声", "https://live-play.cctvnews.cctv.com/cctv/zgzs192.m3u8", RadioStation::StreamType::kHlsM3u8},
    {"CNR 音乐之声", "http://liveop.cctv.cn/hls/yyzs192/playlist.m3u8", RadioStation::StreamType::kHlsM3u8},
    {"CNR 文艺之声", "http://audiows010.cnr.cn/live/wyzs192/playlist.m3u8", RadioStation::StreamType::kHlsM3u8},
    {"广东音乐之声 FM99.3", "http://live.xmcdn.com/live/74/64.m3u8", RadioStation::StreamType::kHlsM3u8},
    {"广东新闻频道 FM91.4", "http://live.xmcdn.com/live/245/64.m3u8", RadioStation::StreamType::kHlsM3u8},
    {"广东羊城交通 FM105.2", "http://live.xmcdn.com/live/248/64.m3u8", RadioStation::StreamType::kHlsM3u8},
    {"广东珠江经济台 FM97.4", "http://live.xmcdn.com/live/252/64.m3u8", RadioStation::StreamType::kHlsM3u8},
    {"广州 MYFM 88.0", "http://live.xmcdn.com/live/259/64.m3u8", RadioStation::StreamType::kHlsM3u8},
};

static constexpr int kStationCount = sizeof(kStations) / sizeof(kStations[0]);
static constexpr int kOutputRate = 24000;
static constexpr size_t kRawBufferBytes = 16 * 1024;
static constexpr size_t kPcmBufferBytes = 8 * 1024;
static constexpr int kHttpReadChunk = 2048;
static constexpr const char* kUserAgent = "Mozilla/5.0 ESP32 Radio";

lv_obj_t* AddLabel(lv_obj_t* parent, int y, const char* text, uint32_t color,
                   lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_pos(label, 4, y);
    lv_obj_set_width(label, 120);
    lv_obj_set_style_text_font(label, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    return label;
}

int MixToMonoAndResample(const int16_t* src, int sample_count, int channels, int sample_rate,
                         int16_t* mono, int mono_capacity, int16_t* out, int out_capacity) {
    if (src == nullptr || sample_count <= 0 || channels <= 0 || mono_capacity <= 0 || out_capacity <= 0) {
        return 0;
    }

    int mono_len = 0;
    for (int i = 0; i < sample_count && mono_len < mono_capacity; i += channels) {
        int32_t mixed = 0;
        int used = 0;
        for (int ch = 0; ch < channels && i + ch < sample_count; ++ch) {
            mixed += src[i + ch];
            ++used;
        }
        mono[mono_len++] = static_cast<int16_t>(mixed / std::max(used, 1));
    }

    if (sample_rate <= 0 || sample_rate == kOutputRate) {
        int copy = std::min(mono_len, out_capacity);
        std::memcpy(out, mono, copy * sizeof(int16_t));
        return copy;
    }

    const float ratio = static_cast<float>(sample_rate) / static_cast<float>(kOutputRate);
    const int out_samples = std::min(static_cast<int>(mono_len / ratio) + 1, out_capacity);
    int out_len = 0;
    for (int i = 0; i < out_samples; ++i) {
        const float in_pos = i * ratio;
        const int in_idx = static_cast<int>(in_pos);
        const float frac = in_pos - in_idx;
        if (in_idx + 1 < mono_len) {
            out[out_len++] = static_cast<int16_t>(mono[in_idx] * (1.0f - frac) + mono[in_idx + 1] * frac);
        } else if (in_idx < mono_len) {
            out[out_len++] = mono[in_idx];
        }
    }
    return out_len;
}

std::string TrimLine(const std::string& line) {
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t' || line[start] == '\r')) {
        ++start;
    }
    size_t end = line.size();
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r')) {
        --end;
    }
    return line.substr(start, end - start);
}

std::string ResolveSegmentUrl(const std::string& playlist_url, const std::string& segment) {
    if (segment.find("http://") == 0 || segment.find("https://") == 0) {
        return segment;
    }
    if (segment.find("//") == 0) {
        const auto scheme_end = playlist_url.find("://");
        const std::string scheme = scheme_end == std::string::npos ? "http" : playlist_url.substr(0, scheme_end);
        return scheme + ":" + segment;
    }

    const auto scheme_end = playlist_url.find("://");
    if (scheme_end == std::string::npos) {
        return segment;
    }
    const auto host_start = scheme_end + 3;
    const auto path_start = playlist_url.find('/', host_start);
    const std::string origin = path_start == std::string::npos ? playlist_url : playlist_url.substr(0, path_start);

    if (!segment.empty() && segment[0] == '/') {
        return origin + segment;
    }

    const auto last_slash = playlist_url.rfind('/');
    const std::string base = last_slash == std::string::npos ? (playlist_url + "/") : playlist_url.substr(0, last_slash + 1);
    return base + segment;
}

std::vector<std::string> ParseSegmentUrls(const std::string& playlist_url, const std::string& playlist) {
    std::vector<std::string> segments;
    size_t pos = 0;
    while (pos < playlist.size() && segments.size() < 3) {
        const size_t line_end = playlist.find('\n', pos);
        const std::string line = TrimLine(playlist.substr(pos, line_end == std::string::npos ? std::string::npos : line_end - pos));
        pos = line_end == std::string::npos ? playlist.size() : line_end + 1;

        if (line.empty() || line[0] == '#') {
            continue;
        }
        segments.push_back(ResolveSegmentUrl(playlist_url, line));
    }
    return segments;
}

}  // namespace

InternetRadio::~InternetRadio() {
    Stop();
    if (pcm_ring_ != nullptr) {
        vRingbufferDelete(pcm_ring_);
        pcm_ring_ = nullptr;
    }
    if (ring_struct_ != nullptr) {
        heap_caps_free(ring_struct_);
        ring_struct_ = nullptr;
    }
    if (ring_storage_ != nullptr) {
        heap_caps_free(ring_storage_);
        ring_storage_ = nullptr;
    }
}

void InternetRadio::Start(Display* display, AudioCodec* codec) {
    if (running_ || display == nullptr || codec == nullptr) {
        return;
    }

    display_ = display;
    codec_ = codec;
    paused_ = false;
    reconnect_now_ = false;
    bitrate_kbps_ = 0;
    sample_rate_ = 0;
    channels_ = 0;
    FlushPcmRing();

    if (ring_struct_ == nullptr) {
        ring_struct_ = static_cast<StaticRingbuffer_t*>(heap_caps_malloc(sizeof(StaticRingbuffer_t),
                                                                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (ring_storage_ == nullptr) {
        ring_storage_ = static_cast<uint8_t*>(heap_caps_malloc(kPcmRingBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (pcm_ring_ == nullptr && ring_struct_ != nullptr && ring_storage_ != nullptr) {
        pcm_ring_ = xRingbufferCreateStatic(kPcmRingBytes, RINGBUF_TYPE_BYTEBUF, ring_storage_, ring_struct_);
    }
    if (pcm_ring_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate PCM ring buffer");
        return;
    }

    StopVoiceSession();
    codec_->EnableInput(false);
    codec_->EnableOutput(true);
    output_chunk_.reserve(2048);

    running_ = true;
    SetState(State::kConnecting);
    CreateUi();

    if (xTaskCreate(&InternetRadio::StreamTaskEntry, "internet_radio", 12288, this, 5, &stream_task_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stream task");
        running_ = false;
        DestroyUi();
        return;
    }
    if (xTaskCreate(&InternetRadio::PlayTaskEntry, "radio_play", 4096, this, 6, &play_task_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create play task");
        running_ = false;
    }
}

void InternetRadio::Stop() {
    if (!running_ && layer_ == nullptr) {
        return;
    }

    running_ = false;
    reconnect_now_ = true;
    if (current_client_ != nullptr) {
        esp_http_client_close(current_client_);
    }

    for (int i = 0; i < 20 && (stream_task_ != nullptr || play_task_ != nullptr); ++i) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (stream_task_ != nullptr) {
        vTaskDelete(stream_task_);
        stream_task_ = nullptr;
    }
    if (play_task_ != nullptr) {
        vTaskDelete(play_task_);
        play_task_ = nullptr;
    }

    if (codec_ != nullptr) {
        codec_->EnableOutput(false);
        codec_->EnableInput(true);
    }
    FlushPcmRing();
    DestroyUi();
    display_ = nullptr;
    codec_ = nullptr;
    SetState(State::kIdle);
}

bool InternetRadio::HandleClick() {
    if (!running_) {
        return false;
    }
    paused_ = !paused_;
    if (codec_ != nullptr) {
        codec_->EnableOutput(!paused_);
    }
    UpdateUi();
    return true;
}

bool InternetRadio::HandleDoubleClick() {
    if (!running_) {
        return false;
    }
    SwitchStation(-1);
    return true;
}

bool InternetRadio::MoveRight() {
    if (!running_) {
        return false;
    }
    SwitchStation(1);
    return true;
}

void InternetRadio::CreateUi() {
    if (display_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(display_);
    if (layer_ != nullptr) {
        lv_obj_delete(layer_);
    }

    layer_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(layer_, display_->width(), display_->height());
    lv_obj_set_pos(layer_, 0, 0);
    lv_obj_set_scrollbar_mode(layer_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(layer_, lv_color_hex(0x07111f), 0);
    lv_obj_set_style_bg_opa(layer_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(layer_, 0, 0);
    lv_obj_set_style_pad_all(layer_, 0, 0);

    title_label_ = AddLabel(layer_, 4, "NETWORK RADIO", 0x38bdf8);
    wifi_label_ = AddLabel(layer_, 24, "WiFi: ...", 0x94a3b8);
    index_label_ = AddLabel(layer_, 43, "Station 1/6", 0xffd166);
    name_label_ = AddLabel(layer_, 62, kStations[station_index_].name, 0xf8fafc);
    status_label_ = AddLabel(layer_, 84, "Connecting", 0xfacc15);
    info_label_ = AddLabel(layer_, 105, "MP3 direct stream", 0x94a3b8);
    boot_label_ = AddLabel(layer_, 126, "BOOT pause/play", 0x94a3b8);
    next_label_ = AddLabel(layer_, 143, "GPIO39 next", 0x94a3b8);
    lv_obj_move_foreground(layer_);
    UpdateUi();
}

void InternetRadio::DestroyUi() {
    if (display_ == nullptr || layer_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(display_);
    lv_obj_delete(layer_);
    layer_ = nullptr;
    title_label_ = nullptr;
    wifi_label_ = nullptr;
    index_label_ = nullptr;
    name_label_ = nullptr;
    status_label_ = nullptr;
    info_label_ = nullptr;
    boot_label_ = nullptr;
    next_label_ = nullptr;
}

void InternetRadio::UpdateUi() {
    if (display_ == nullptr || layer_ == nullptr) {
        return;
    }

    DisplayLockGuard lock(display_);
    if (!lv_obj_is_valid(layer_)) {
        return;
    }

    lv_label_set_text(wifi_label_, WifiStation::GetInstance().IsConnected() ? "WiFi: Connected" : "WiFi: Offline");

    char index_text[24];
    snprintf(index_text, sizeof(index_text), "Station %d/%d", station_index_ + 1, kStationCount);
    lv_label_set_text(index_label_, index_text);
    lv_label_set_text(name_label_, kStations[station_index_].name);

    const char* status = "Idle";
    uint32_t color = 0x94a3b8;
    switch (state_) {
        case State::kConnecting:
            status = "Connecting";
            color = 0xfacc15;
            break;
        case State::kBuffering:
            status = "Connecting";
            color = 0xfacc15;
            break;
        case State::kPlaying:
            status = paused_ ? "Paused" : "Playing";
            color = paused_ ? 0xfacc15 : 0x4ade80;
            break;
        case State::kHlsParsed:
            status = "HLS parsed";
            color = 0x4ade80;
            break;
        case State::kHlsParseFailed:
            status = "HLS Parse Failed";
            color = 0xef4444;
            break;
        case State::kError:
            status = "Error";
            color = 0xef4444;
            break;
        default:
            break;
    }
    lv_obj_set_style_text_color(status_label_, lv_color_hex(color), 0);
    lv_label_set_text(status_label_, status);

    char info_text[40];
    if (state_ == State::kHlsParsed) {
        snprintf(info_text, sizeof(info_text), "Codec not ready");
    } else if (state_ == State::kHlsParseFailed) {
        snprintf(info_text, sizeof(info_text), "m3u8 parse failed");
    } else if (kStations[station_index_].type == RadioStation::StreamType::kHlsM3u8) {
        snprintf(info_text, sizeof(info_text), "HLS m3u8 probe");
    } else if (sample_rate_ > 0) {
        snprintf(info_text, sizeof(info_text), "%dk %s %dkbps", sample_rate_ / 1000,
                 channels_ > 1 ? "stereo" : "mono", bitrate_kbps_);
    } else {
        snprintf(info_text, sizeof(info_text), "MP3 direct stream");
    }
    lv_label_set_text(info_label_, info_text);
}

void InternetRadio::SetState(State state) {
    state_ = state;
    UpdateUi();
}

void InternetRadio::SwitchStation(int delta) {
    station_index_ = (station_index_ + delta + kStationCount) % kStationCount;
    paused_ = false;
    bitrate_kbps_ = 0;
    sample_rate_ = 0;
    channels_ = 0;
    reconnect_now_ = true;
    FlushPcmRing();
    if (codec_ != nullptr) {
        codec_->EnableOutput(true);
    }
    if (current_client_ != nullptr) {
        esp_http_client_close(current_client_);
    }
    SetState(State::kConnecting);
}

bool InternetRadio::ProbeHlsPlaylist(const char* url) {
    ESP_LOGI(TAG, "HLS playlist probe: %s", url);

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 6000;
    config.buffer_size = 2048;
    config.buffer_size_tx = 512;
    config.disable_auto_redirect = false;
    config.max_redirection_count = 3;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGW(TAG, "HLS client init failed");
        return false;
    }

    current_client_ = client;
    esp_http_client_set_header(client, "User-Agent", kUserAgent);
    esp_http_client_set_header(client, "Accept", "application/vnd.apple.mpegurl,application/x-mpegURL,*/*");

    esp_err_t open_err = esp_http_client_open(client, 0);
    if (open_err != ESP_OK) {
        ESP_LOGW(TAG, "HLS open failed: %s", esp_err_to_name(open_err));
        current_client_ = nullptr;
        esp_http_client_cleanup(client);
        return false;
    }

    esp_http_client_fetch_headers(client);
    const int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HLS HTTP %d: %s", status_code, url);
    if (status_code < 200 || status_code >= 400) {
        esp_http_client_close(client);
        current_client_ = nullptr;
        esp_http_client_cleanup(client);
        return false;
    }

    std::string playlist;
    playlist.reserve(4096);
    char buffer[512];
    while (running_ && !reconnect_now_ && playlist.size() < 8192) {
        const int read = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read <= 0) {
            break;
        }
        playlist.append(buffer, read);
    }

    esp_http_client_close(client);
    current_client_ = nullptr;
    esp_http_client_cleanup(client);

    if (playlist.empty()) {
        ESP_LOGW(TAG, "HLS playlist is empty");
        return false;
    }

    const int preview_len = std::min<int>(playlist.size(), 500);
    ESP_LOGI(TAG, "HLS playlist first %d bytes:\n%.*s", preview_len, preview_len, playlist.c_str());

    const std::vector<std::string> segments = ParseSegmentUrls(url, playlist);
    for (size_t i = 0; i < segments.size(); ++i) {
        ESP_LOGI(TAG, "HLS segment %d: %s", static_cast<int>(i + 1), segments[i].c_str());
    }

    return !segments.empty();
}

void InternetRadio::FlushPcmRing() {
    if (pcm_ring_ == nullptr) {
        return;
    }
    size_t size = 0;
    void* item = nullptr;
    while ((item = xRingbufferReceive(pcm_ring_, &size, 0)) != nullptr) {
        vRingbufferReturnItem(pcm_ring_, item);
    }
}

void InternetRadio::StopVoiceSession() {
    auto& app = Application::GetInstance();
    const auto state = app.GetDeviceState();
    if (state == kDeviceStateSpeaking) {
        app.AbortSpeaking(kAbortReasonNone);
        app.SetDeviceState(kDeviceStateIdle);
    } else if (state == kDeviceStateListening || state == kDeviceStateConnecting || state == kDeviceStateActivating) {
        app.SetDeviceState(kDeviceStateIdle);
    }
}

void InternetRadio::StreamTaskEntry(void* arg) {
    static_cast<InternetRadio*>(arg)->StreamLoop();
}

void InternetRadio::PlayTaskEntry(void* arg) {
    static_cast<InternetRadio*>(arg)->PlayLoop();
}

void InternetRadio::PlayLoop() {
    ESP_LOGI(TAG, "play task started");
    while (running_) {
        size_t rx_size = 0;
        auto* item = static_cast<uint8_t*>(xRingbufferReceiveUpTo(pcm_ring_, &rx_size, pdMS_TO_TICKS(50), 4096));
        if (item == nullptr || rx_size == 0) {
            continue;
        }

        if (!paused_ && codec_ != nullptr) {
            const size_t samples = rx_size / sizeof(int16_t);
            output_chunk_.assign(reinterpret_cast<int16_t*>(item), reinterpret_cast<int16_t*>(item) + samples);
            codec_->OutputData(output_chunk_);
        }
        vRingbufferReturnItem(pcm_ring_, item);
    }
    play_task_ = nullptr;
    vTaskDelete(nullptr);
}

void InternetRadio::StreamLoop() {
    ESP_LOGI(TAG, "stream task started");

    auto* raw_buffer = static_cast<uint8_t*>(heap_caps_malloc(kRawBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto* pcm_buffer = static_cast<uint8_t*>(heap_caps_malloc(kPcmBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto* mono_buffer = static_cast<int16_t*>(heap_caps_malloc(2304 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto* resample_buffer = static_cast<int16_t*>(heap_caps_malloc(2304 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (raw_buffer == nullptr || pcm_buffer == nullptr || mono_buffer == nullptr || resample_buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate stream buffers");
        SetState(State::kError);
        running_ = false;
    }

    while (running_) {
        reconnect_now_ = false;
        SetState(WifiStation::GetInstance().IsConnected() ? State::kConnecting : State::kError);
        if (!WifiStation::GetInstance().IsConnected()) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        const RadioStation& station = kStations[station_index_];
        if (station.type == RadioStation::StreamType::kHlsM3u8) {
            FlushPcmRing();
            if (codec_ != nullptr) {
                codec_->EnableOutput(false);
            }
            const bool parsed = ProbeHlsPlaylist(station.url);
            SetState(parsed ? State::kHlsParsed : State::kHlsParseFailed);
            while (running_ && !reconnect_now_) {
                UpdateUi();
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            continue;
        }

        esp_http_client_config_t config = {};
        config.url = station.url;
        config.timeout_ms = 4000;
        config.buffer_size = 2048;
        config.buffer_size_tx = 512;
        config.disable_auto_redirect = false;
        config.max_redirection_count = 3;
        config.crt_bundle_attach = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
            SetState(State::kError);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }
        current_client_ = client;
        esp_http_client_set_header(client, "User-Agent", kUserAgent);
        esp_http_client_set_header(client, "Icy-MetaData", "0");

        esp_err_t open_err = esp_http_client_open(client, 0);
        if (open_err != ESP_OK) {
            ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(open_err));
            current_client_ = nullptr;
            esp_http_client_cleanup(client);
            SetState(State::kError);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        esp_http_client_fetch_headers(client);
        const int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %d: %s", status_code, station.url);
        if (status_code < 200 || status_code >= 400) {
            esp_http_client_close(client);
            current_client_ = nullptr;
            esp_http_client_cleanup(client);
            SetState(State::kError);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        void* decoder = nullptr;
        if (esp_mp3_dec_open(nullptr, 0, &decoder) != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "MP3 decoder open failed");
            esp_http_client_close(client);
            current_client_ = nullptr;
            esp_http_client_cleanup(client);
            SetState(State::kError);
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        int raw_len = 0;
        SetState(State::kBuffering);
        if (codec_ != nullptr) {
            codec_->EnableOutput(true);
        }

        while (running_ && !reconnect_now_) {
            if (raw_len < static_cast<int>(kRawBufferBytes - kHttpReadChunk)) {
                int read = esp_http_client_read(client, reinterpret_cast<char*>(raw_buffer + raw_len),
                                                kRawBufferBytes - raw_len);
                if (read <= 0) {
                    ESP_LOGW(TAG, "stream read ended: %d", read);
                    break;
                }
                raw_len += read;
            }

            bool decoded_any = false;
            while (raw_len > 0 && running_ && !reconnect_now_) {
                esp_audio_dec_in_raw_t raw = {};
                raw.buffer = raw_buffer;
                raw.len = raw_len;
                esp_audio_dec_out_frame_t frame = {};
                frame.buffer = pcm_buffer;
                frame.len = kPcmBufferBytes;
                esp_audio_dec_info_t info = {};

                const esp_audio_err_t dec_err = esp_mp3_dec_decode(decoder, &raw, &frame, &info);
                if (dec_err != ESP_AUDIO_ERR_OK || raw.consumed == 0) {
                    break;
                }
                decoded_any = true;
                if (raw.consumed < static_cast<uint32_t>(raw_len)) {
                    std::memmove(raw_buffer, raw_buffer + raw.consumed, raw_len - raw.consumed);
                }
                raw_len -= raw.consumed;

                if (frame.decoded_size == 0) {
                    continue;
                }

                sample_rate_ = info.sample_rate > 0 ? info.sample_rate : sample_rate_;
                channels_ = info.channel > 0 ? info.channel : 1;
                bitrate_kbps_ = info.bitrate > 0 ? info.bitrate / 1000 : bitrate_kbps_;

                const int out_len = MixToMonoAndResample(reinterpret_cast<int16_t*>(pcm_buffer),
                                                         frame.decoded_size / sizeof(int16_t),
                                                         channels_, sample_rate_,
                                                         mono_buffer, 2304,
                                                         resample_buffer, 2304);
                if (out_len > 0) {
                    if (state_ != State::kPlaying) {
                        SetState(State::kPlaying);
                    }
                    xRingbufferSend(pcm_ring_, resample_buffer, out_len * sizeof(int16_t), pdMS_TO_TICKS(100));
                }
            }

            if (!decoded_any && raw_len >= static_cast<int>(kRawBufferBytes - kHttpReadChunk)) {
                raw_len = 0;
            }
            UpdateUi();
            vTaskDelay(pdMS_TO_TICKS(2));
        }

        esp_mp3_dec_close(decoder);
        esp_http_client_close(client);
        if (current_client_ == client) {
            current_client_ = nullptr;
        }
        esp_http_client_cleanup(client);
        FlushPcmRing();
        if (codec_ != nullptr) {
            codec_->EnableOutput(false);
        }

        if (running_ && !reconnect_now_) {
            SetState(State::kError);
            vTaskDelay(pdMS_TO_TICKS(2500));
        }
    }

    if (raw_buffer != nullptr) {
        heap_caps_free(raw_buffer);
    }
    if (pcm_buffer != nullptr) {
        heap_caps_free(pcm_buffer);
    }
    if (mono_buffer != nullptr) {
        heap_caps_free(mono_buffer);
    }
    if (resample_buffer != nullptr) {
        heap_caps_free(resample_buffer);
    }
    stream_task_ = nullptr;
    vTaskDelete(nullptr);
}
