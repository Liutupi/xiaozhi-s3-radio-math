#include "internet_radio.h"

#include "application.h"
#include "esp_mp3_dec.h"

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <wifi_station.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <inttypes.h>
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

    std::string name;
    std::string category;
    std::array<std::string, 3> urls;
    StreamType type;
};

struct BuiltinStation {
    const char* name;
    const char* category;
    const char* urls[3];
    RadioStation::StreamType type;
};

static const BuiltinStation kBuiltinStations[] = {
    {"Groove Salad", "英文", {"https://ice5.somafm.com/groovesalad-128-mp3", nullptr, nullptr}, RadioStation::StreamType::kMp3Direct},
    {"SomaFM Live", "英文", {"https://ice5.somafm.com/live-128-mp3", nullptr, nullptr}, RadioStation::StreamType::kMp3Direct},
    {"n5MD Radio", "英文", {"https://ice5.somafm.com/n5md-128-mp3", nullptr, nullptr}, RadioStation::StreamType::kMp3Direct},
    {"The In-Sound", "英文", {"https://ice5.somafm.com/insound-128-mp3", nullptr, nullptr}, RadioStation::StreamType::kMp3Direct},
    {"Dark Zone", "英文", {"https://ice5.somafm.com/darkzone-128-mp3", nullptr, nullptr}, RadioStation::StreamType::kMp3Direct},
    {"Mission Control", "英文", {"https://ice5.somafm.com/missioncontrol-128-mp3", nullptr, nullptr}, RadioStation::StreamType::kMp3Direct},
    {"CNR中国之声", "新闻", {"https://lhttp.qtfm.cn/live/15318317/64k.mp3", "https://lhttp-hw.qtfm.cn/live/15318317/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"江苏新闻广播", "新闻", {"https://lhttp.qtfm.cn/live/4944/64k.mp3", "https://lhttp-hw.qtfm.cn/live/4944/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"安徽综合广播", "新闻", {"https://lhttp.qingting.fm/live/4919/64k.mp3", "https://lhttp.qtfm.cn/live/4919/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"广州新闻资讯", "广东", {"http://lhttp.qingting.fm/live/4848/64k.mp3", "https://lhttp.qtfm.cn/live/4848/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"广州交通经济", "交通", {"http://lhttp.qingting.fm/live/4955/64k.mp3", "https://lhttp.qtfm.cn/live/4955/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"珠江经济电台", "广东", {"http://lhttp.qingting.fm/live/1259/64k.mp3", "https://lhttp.qtfm.cn/live/1259/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"广东音乐之声", "广东", {"http://lhttp.qingting.fm/live/1260/64k.mp3", "https://lhttp.qtfm.cn/live/1260/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"广东文体广播", "广东", {"https://lhttp.qtfm.cn/live/471/64k.mp3", "https://lhttp-hw.qtfm.cn/live/471/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"广东珠江之声", "广东", {"http://lhttp.qingting.fm/live/470/64k.mp3", "https://lhttp.qtfm.cn/live/470/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"深圳飞扬971", "广东", {"http://lhttp.qingting.fm/live/1271/64k.mp3", "https://lhttp.qtfm.cn/live/1271/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"惠州音乐广播", "广东", {"http://lhttp.qingting.fm/live/5021523/64k.mp3", "https://lhttp.qtfm.cn/live/5021523/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"茂名交通广播", "交通", {"https://lhttp.qingting.fm/live/20211574/64k.mp3", "https://lhttp.qtfm.cn/live/20211574/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"两广之声音乐台", "华语音乐", {"http://lhttp.qingting.fm/live/20500149/64k.mp3", "https://lhttp.qtfm.cn/live/20500149/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"亚洲粤语", "华语音乐", {"https://lhttp.qingting.fm/live/15318569/64k.mp3", "https://lhttp.qtfm.cn/live/15318569/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"500首华语经典", "华语音乐", {"https://lhttp.qtfm.cn/live/5022308/64k.mp3", "https://lhttp-hw.qtfm.cn/live/5022308/64k.mp3", "http://lhttp.qingting.fm/live/5022308/64k.mp3"}, RadioStation::StreamType::kMp3Direct},
    {"清晨音乐台", "华语音乐", {"http://lhttp.qingting.fm/live/4915/64k.mp3", "https://lhttp.qtfm.cn/live/4915/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"怀旧好声音", "华语音乐", {"http://lhttp.qingting.fm/live/1223/64k.mp3", "https://lhttp.qtfm.cn/live/1223/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"959年代音乐", "华语音乐", {"http://lhttp.qingting.fm/live/5021381/64k.mp3", "https://lhttp.qtfm.cn/live/5021381/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"动听音乐台", "华语音乐", {"http://lhttp.qingting.fm/live/5022107/64k.mp3", "https://lhttp.qtfm.cn/live/5022107/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"上海LoveRadio", "华语音乐", {"http://lhttp.qingting.fm/live/273/64k.mp3", "https://lhttp.qtfm.cn/live/273/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"上海动感101", "华语音乐", {"http://lhttp.qingting.fm/live/274/64k.mp3", "https://lhttp.qtfm.cn/live/274/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"江苏经典流行音乐", "华语音乐", {"http://lhttp.qingting.fm/live/4938/64k.mp3", "https://lhttp.qtfm.cn/live/4938/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"北京音乐广播", "华语音乐", {"http://lhttp.qingting.fm/live/332/64k.mp3", "https://lhttp.qtfm.cn/live/332/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"湖北经典音乐广播", "华语音乐", {"http://lhttp.qingting.fm/live/1296/64k.mp3", "https://lhttp.qtfm.cn/live/1296/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"武汉经典音乐广播", "华语音乐", {"http://lhttp.qingting.fm/live/1297/64k.mp3", "https://lhttp.qtfm.cn/live/1297/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"90.7 MIX FM", "华语音乐", {"https://lhttp.qingting.fm/live/15318146/64k.mp3", "https://lhttp.qtfm.cn/live/15318146/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
    {"包河之声 FM100.8", "本地", {"https://lhttp-hw.qtfm.cn/live/5022668/64k.mp3", "https://lhttp.qtfm.cn/live/5022668/64k.mp3", nullptr}, RadioStation::StreamType::kMp3Direct},
};

static std::vector<RadioStation> g_stations;
static bool g_remote_load_attempted = false;

static constexpr int kOutputRate = 24000;
static constexpr size_t kRawBufferBytes = 16 * 1024;
static constexpr size_t kPcmBufferBytes = 8 * 1024;
static constexpr int kHttpReadChunk = 2048;
static constexpr const char* kUserAgent = "Mozilla/5.0 ESP32 Radio";
static constexpr const char* kRemoteStationsUrl =
    "https://raw.githubusercontent.com/Liutupi/xiaozhi-s3-radio-math/main/stations.json";

const char* StreamTypeName(RadioStation::StreamType type) {
    return type == RadioStation::StreamType::kMp3Direct ? "MP3 direct" : "HLS m3u8";
}

int StationCount() {
    return static_cast<int>(g_stations.size());
}

const RadioStation& CurrentStation(int index) {
    return g_stations[std::max(0, std::min(index, StationCount() - 1))];
}

void LoadBuiltinStations() {
    g_stations.clear();
    g_stations.reserve(sizeof(kBuiltinStations) / sizeof(kBuiltinStations[0]));
    for (const auto& builtin : kBuiltinStations) {
        RadioStation station;
        station.name = builtin.name;
        station.category = builtin.category;
        station.type = builtin.type;
        for (size_t i = 0; i < station.urls.size(); ++i) {
            if (builtin.urls[i] != nullptr) {
                station.urls[i] = builtin.urls[i];
            }
        }
        g_stations.push_back(std::move(station));
    }
}

bool UrlLooksPlayableMp3(const std::string& url) {
    if (url.empty()) {
        return false;
    }
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower.find(".mp3") != std::string::npos &&
           lower.find(".m3u8") == std::string::npos &&
           lower.find(".aac") == std::string::npos &&
           lower.find(".flv") == std::string::npos &&
           lower.find("token=") == std::string::npos;
}

RadioStation::StreamType ParseStreamType(const char* type) {
    if (type == nullptr) {
        return RadioStation::StreamType::kMp3Direct;
    }
    if (std::strstr(type, "hls") != nullptr || std::strstr(type, "m3u8") != nullptr) {
        return RadioStation::StreamType::kHlsM3u8;
    }
    return RadioStation::StreamType::kMp3Direct;
}

bool AddRemoteStationFromJson(const cJSON* item, std::vector<RadioStation>& stations) {
    if (!cJSON_IsObject(item)) {
        return false;
    }

    const cJSON* enabled = cJSON_GetObjectItem(item, "enabled");
    if (cJSON_IsBool(enabled) && !cJSON_IsTrue(enabled)) {
        return false;
    }
    const cJSON* name = cJSON_GetObjectItem(item, "name");
    const cJSON* url = cJSON_GetObjectItem(item, "url");
    if (!cJSON_IsString(name) || !cJSON_IsString(url)) {
        return false;
    }

    RadioStation station;
    station.name = name->valuestring;
    const cJSON* category = cJSON_GetObjectItem(item, "category");
    station.category = cJSON_IsString(category) ? category->valuestring : "华语音乐";
    const cJSON* type = cJSON_GetObjectItem(item, "type");
    station.type = cJSON_IsString(type) ? ParseStreamType(type->valuestring) : RadioStation::StreamType::kMp3Direct;

    if (station.type != RadioStation::StreamType::kMp3Direct || !UrlLooksPlayableMp3(url->valuestring)) {
        ESP_LOGW(TAG, "Skip unsupported remote station: name=%s url=%s", station.name.c_str(), url->valuestring);
        return false;
    }
    station.urls[0] = url->valuestring;

    const cJSON* fallbacks = cJSON_GetObjectItem(item, "fallback_urls");
    int out = 1;
    if (cJSON_IsArray(fallbacks)) {
        const cJSON* fallback = nullptr;
        cJSON_ArrayForEach(fallback, fallbacks) {
            if (out >= static_cast<int>(station.urls.size())) {
                break;
            }
            if (cJSON_IsString(fallback) && UrlLooksPlayableMp3(fallback->valuestring)) {
                station.urls[out++] = fallback->valuestring;
            }
        }
    }
    stations.push_back(std::move(station));
    return true;
}

bool DownloadText(const char* url, std::string& text, size_t max_bytes) {
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 3500;
    config.buffer_size = 2048;
    config.buffer_size_tx = 512;
    config.disable_auto_redirect = false;
    config.max_redirection_count = 5;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        return false;
    }
    esp_http_client_set_header(client, "User-Agent", kUserAgent);
    esp_http_client_set_header(client, "Accept", "application/json,*/*");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Remote station list open failed: url=%s err=%s", url, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }
    esp_http_client_fetch_headers(client);
    const int status_code = esp_http_client_get_status_code(client);
    if (status_code < 200 || status_code >= 300) {
        ESP_LOGW(TAG, "Remote station list status=%d url=%s", status_code, url);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    char buffer[1024];
    while (text.size() < max_bytes) {
        const int read = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read <= 0) {
            break;
        }
        text.append(buffer, read);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return !text.empty() && text.size() < max_bytes;
}

bool LoadRemoteStations() {
    std::string json;
    if (!DownloadText(kRemoteStationsUrl, json, 32 * 1024)) {
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(json.data(), json.size());
    if (root == nullptr) {
        ESP_LOGW(TAG, "Remote station list JSON parse failed");
        return false;
    }
    cJSON* array = root;
    cJSON* stations_object = cJSON_GetObjectItem(root, "stations");
    if (cJSON_IsArray(stations_object)) {
        array = stations_object;
    }
    if (!cJSON_IsArray(array)) {
        ESP_LOGW(TAG, "Remote station list is not an array");
        cJSON_Delete(root);
        return false;
    }

    std::vector<RadioStation> remote_stations;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, array) {
        AddRemoteStationFromJson(item, remote_stations);
    }
    cJSON_Delete(root);

    if (remote_stations.empty()) {
        ESP_LOGW(TAG, "Remote station list had no playable MP3 stations");
        return false;
    }
    g_stations = std::move(remote_stations);
    ESP_LOGI(TAG, "Loaded %d remote stations from %s", StationCount(), kRemoteStationsUrl);
    return true;
}

void EnsureStationsLoaded() {
    if (g_stations.empty()) {
        LoadBuiltinStations();
    }
    if (!g_remote_load_attempted && WifiStation::GetInstance().IsConnected()) {
        g_remote_load_attempted = true;
        if (!LoadRemoteStations()) {
            ESP_LOGW(TAG, "Using built-in radio stations");
            LoadBuiltinStations();
        }
    }
}

bool IsAudioMpegContentType(const char* content_type) {
    if (content_type == nullptr || content_type[0] == '\0') {
        return true;
    }
    return std::strstr(content_type, "audio/mpeg") != nullptr ||
           std::strstr(content_type, "audio/mp3") != nullptr ||
           std::strstr(content_type, "application/octet-stream") != nullptr;
}

struct Mp3ProbeResult {
    int status_code = 0;
    int64_t content_length = -1;
    char content_type[64] = {};
    bool ok = false;
};

Mp3ProbeResult ProbeMp3Station(const RadioStation& station, const char* url, int station_index,
                               int station_count, int url_index) {
    Mp3ProbeResult result;
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 2500;
    config.buffer_size = 1024;
    config.buffer_size_tx = 512;
    config.disable_auto_redirect = false;
    config.max_redirection_count = 5;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGW(TAG, "probe init failed: station=%s url=%s", station.name.c_str(), url);
        return result;
    }
    esp_http_client_set_header(client, "User-Agent", kUserAgent);
    esp_http_client_set_header(client, "Accept", "audio/mpeg,*/*");
    esp_http_client_set_header(client, "Icy-MetaData", "0");

    const esp_err_t open_err = esp_http_client_open(client, 0);
    if (open_err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        result.status_code = esp_http_client_get_status_code(client);
        result.content_length = esp_http_client_get_content_length(client);
        char* content_type = nullptr;
        esp_http_client_get_header(client, "Content-Type", &content_type);
        if (content_type != nullptr) {
            strlcpy(result.content_type, content_type, sizeof(result.content_type));
        }
        result.ok = result.status_code >= 200 && result.status_code < 400 &&
                    IsAudioMpegContentType(result.content_type);
        ESP_LOGI(TAG, "probe station=%d/%d name=%s category=%s url_index=%d using_fallback=%s url=%s "
                      "HTTP status=%d content-type=%s content-length=%lld ok=%s",
                 station_index + 1, station_count, station.name.c_str(), station.category.c_str(), url_index,
                 url_index > 0 ? "yes" : "no", url, result.status_code,
                 result.content_type[0] ? result.content_type : "(none)",
                 static_cast<long long>(result.content_length), result.ok ? "yes" : "no");
    } else {
        ESP_LOGW(TAG, "probe open failed: station=%d/%d name=%s category=%s url=%s err=%s",
                 station_index + 1, station_count, station.name.c_str(), station.category.c_str(), url,
                 esp_err_to_name(open_err));
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}

lv_obj_t* AddLabel(lv_obj_t* parent, int x, int y, int width, const char* text, uint32_t color,
                   lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, text);
    return label;
}

void PlaceLabel(lv_obj_t* label, int x, int y, int width, bool visible = true) {
    if (label == nullptr) {
        return;
    }
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    if (visible) {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
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

    EnsureStationsLoaded();
    if (StationCount() <= 0) {
        ESP_LOGE(TAG, "No radio stations available");
        return;
    }
    station_index_ = std::max(0, std::min(station_index_, StationCount() - 1));

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

    const int w = display_->width();
    const int h = display_->height();
    const int margin = w <= 140 ? 2 : 6;
    const int text_w = std::max(40, w - margin * 2);
    const bool compact = h < 150;

    title_label_ = AddLabel(layer_, margin, compact ? 2 : 4, text_w, "RADIO", 0x38bdf8);
    wifi_label_ = AddLabel(layer_, margin, 24, text_w, "WiFi: ...", 0x94a3b8);
    index_label_ = AddLabel(layer_, margin, compact ? 20 : 43, text_w, "Station 1/6", 0xffd166);
    name_label_ = AddLabel(layer_, margin, compact ? 38 : 62, text_w, CurrentStation(station_index_).name.c_str(), 0xf8fafc);
    status_label_ = AddLabel(layer_, margin, compact ? 56 : 84, text_w, "Connecting", 0xfacc15);
    info_label_ = AddLabel(layer_, margin, compact ? 74 : 105, text_w, "MP3 direct stream", 0x94a3b8);
    next_label_ = AddLabel(layer_, margin, 126, text_w, "GPIO39 next", 0x94a3b8);

    if (compact) {
        PlaceLabel(wifi_label_, margin, 0, text_w, false);
        PlaceLabel(next_label_, margin, 0, text_w, false);
        if (h < 86) {
            PlaceLabel(info_label_, margin, 0, text_w, false);
        }
    } else {
        const int line = h < 190 ? 18 : 21;
        const int y0 = h < 190 ? 3 : 6;
        PlaceLabel(title_label_, margin, y0, text_w);
        PlaceLabel(wifi_label_, margin, y0 + line, text_w);
        PlaceLabel(index_label_, margin, y0 + line * 2, text_w);
        PlaceLabel(name_label_, margin, y0 + line * 3, text_w);
        PlaceLabel(status_label_, margin, y0 + line * 4, text_w);
        PlaceLabel(info_label_, margin, y0 + line * 5, text_w);
        PlaceLabel(next_label_, margin, y0 + line * 6, text_w, y0 + line * 7 < h);
    }
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
    next_label_ = nullptr;
}

void InternetRadio::UpdateUi() {
    if (display_ == nullptr || layer_ == nullptr || StationCount() <= 0) {
        return;
    }

    DisplayLockGuard lock(display_);
    if (!lv_obj_is_valid(layer_)) {
        return;
    }

    const int h = display_->height();
    const bool compact = h < 150;

    lv_label_set_text(wifi_label_, WifiStation::GetInstance().IsConnected() ? "WiFi: Connected" : "WiFi: Offline");

    char index_text[24];
    snprintf(index_text, sizeof(index_text), compact ? "%d/%d" : "Station %d/%d", station_index_ + 1, StationCount());
    lv_label_set_text(index_label_, index_text);
    const RadioStation& station = CurrentStation(station_index_);
    lv_label_set_text(name_label_, station.name.c_str());

    const char* status = "Idle";
    uint32_t color = 0x94a3b8;
    switch (state_) {
        case State::kConnecting:
            status = "Connecting";
            color = 0xfacc15;
            break;
        case State::kBuffering:
            status = "Buffering";
            color = 0xfacc15;
            break;
        case State::kPlaying:
            status = paused_ ? "Paused" : "Playing";
            color = paused_ ? 0xfacc15 : 0x4ade80;
            break;
        case State::kHlsUnsupported:
            status = "HLS unsupported";
            color = 0xef4444;
            break;
        case State::kError:
            status = "Source failed";
            color = 0xef4444;
            break;
        default:
            break;
    }
    lv_obj_set_style_text_color(status_label_, lv_color_hex(color), 0);
    lv_label_set_text(status_label_, status);

    char info_text[40];
    if (state_ == State::kHlsUnsupported) {
        snprintf(info_text, sizeof(info_text), "Need AAC/TS decoder");
    } else if (station.type == RadioStation::StreamType::kHlsM3u8) {
        snprintf(info_text, sizeof(info_text), "HLS m3u8");
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
    if (StationCount() <= 0) {
        return;
    }
    station_index_ = (station_index_ + delta + StationCount()) % StationCount();
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

        if (StationCount() <= 0) {
            SetState(State::kError);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        station_index_ = std::max(0, std::min(station_index_, StationCount() - 1));
        const RadioStation& station = CurrentStation(station_index_);
        ESP_LOGI(TAG, "station=%d/%d name=%s category=%s stream_type=%s",
                 station_index_ + 1, StationCount(), station.name.c_str(), station.category.c_str(),
                 StreamTypeName(station.type));
        if (station.type == RadioStation::StreamType::kHlsM3u8) {
            FlushPcmRing();
            if (codec_ != nullptr) {
                codec_->EnableOutput(false);
            }
            ESP_LOGW(TAG, "HLS is unsupported in this build: station=%s url=%s",
                     station.name.c_str(), station.urls[0].c_str());
            SetState(State::kHlsUnsupported);
            while (running_ && !reconnect_now_) {
                UpdateUi();
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            continue;
        }

        bool played_or_interrupted = false;

        for (int url_index = 0; url_index < static_cast<int>(station.urls.size()); ++url_index) {
            const std::string& url_string = station.urls[url_index];
            if (!running_ || reconnect_now_ || url_string.empty()) {
                break;
            }
            const char* url = url_string.c_str();

            const Mp3ProbeResult probe = ProbeMp3Station(station, url, station_index_, StationCount(), url_index);
            if (!probe.ok) {
                continue;
            }

            ESP_LOGI(TAG, "opening station=%d/%d name=%s category=%s selected_url=%s using_fallback=%s "
                          "HTTP status=%d content-type=%s stream_type=%s",
                     station_index_ + 1, StationCount(), station.name.c_str(), station.category.c_str(), url,
                     url_index > 0 ? "yes" : "no", probe.status_code,
                     probe.content_type[0] ? probe.content_type : "(none)", StreamTypeName(station.type));
            esp_http_client_config_t config = {};
            config.url = url;
            config.timeout_ms = 4000;
            config.buffer_size = 2048;
            config.buffer_size_tx = 512;
            config.disable_auto_redirect = false;
            config.max_redirection_count = 5;
            config.crt_bundle_attach = esp_crt_bundle_attach;

            esp_http_client_handle_t client = esp_http_client_init(&config);
            if (client == nullptr) {
                ESP_LOGW(TAG, "HTTP client init failed: station=%s url=%s", station.name.c_str(), url);
                continue;
            }
            current_client_ = client;
            esp_http_client_set_header(client, "User-Agent", kUserAgent);
            esp_http_client_set_header(client, "Accept", "audio/mpeg,*/*");
            esp_http_client_set_header(client, "Icy-MetaData", "0");

            esp_err_t open_err = esp_http_client_open(client, 0);
            if (open_err != ESP_OK) {
                ESP_LOGW(TAG, "open failed: station=%s url=%s err=%s", station.name.c_str(), url, esp_err_to_name(open_err));
                current_client_ = nullptr;
                esp_http_client_cleanup(client);
                continue;
            }

            esp_http_client_fetch_headers(client);
            const int status_code = esp_http_client_get_status_code(client);
            char* content_type = nullptr;
            esp_http_client_get_header(client, "Content-Type", &content_type);
            char final_url[256] = {};
            esp_http_client_get_url(client, final_url, sizeof(final_url));
            const int64_t content_length = esp_http_client_get_content_length(client);
            ESP_LOGI(TAG, "HTTP status=%d station=%s requested_url=%s final_url=%s content-type=%s content-length=%lld",
                     status_code, station.name.c_str(), url, final_url[0] ? final_url : "(unknown)",
                     content_type != nullptr ? content_type : "(none)", static_cast<long long>(content_length));

            if (status_code < 200 || status_code >= 400 || !IsAudioMpegContentType(content_type)) {
                ESP_LOGW(TAG, "skip URL: station=%s status=%d content-type=%s",
                         station.name.c_str(), status_code, content_type != nullptr ? content_type : "(none)");
                esp_http_client_close(client);
                current_client_ = nullptr;
                esp_http_client_cleanup(client);
                continue;
            }

            void* decoder = nullptr;
            const esp_audio_err_t open_decoder_err = esp_mp3_dec_open(nullptr, 0, &decoder);
            if (open_decoder_err != ESP_AUDIO_ERR_OK) {
                ESP_LOGE(TAG, "MP3 decoder open failed: station=%s decoder_error=%d", station.name.c_str(), open_decoder_err);
                esp_http_client_close(client);
                current_client_ = nullptr;
                esp_http_client_cleanup(client);
                continue;
            }

            int raw_len = 0;
            int no_decode_full_buffer_count = 0;
            uint32_t decoded_frame_count = 0;
            SetState(State::kBuffering);
            if (codec_ != nullptr) {
                codec_->EnableOutput(true);
            }

            while (running_ && !reconnect_now_) {
                if (raw_len < static_cast<int>(kRawBufferBytes - kHttpReadChunk)) {
                    int read = esp_http_client_read(client, reinterpret_cast<char*>(raw_buffer + raw_len),
                                                    kRawBufferBytes - raw_len);
                    if (read <= 0) {
                        ESP_LOGW(TAG, "stream read ended: station=%s url=%s read=%d", station.name.c_str(), url, read);
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
                    ESP_LOGD(TAG, "decode station=%s decoder_error=%d raw.consumed=%" PRIu32
                                  " frame.decoded_size=%" PRIu32 " sample_rate=%d channels=%d bitrate=%d",
                             station.name.c_str(), dec_err, raw.consumed, frame.decoded_size,
                             info.sample_rate, info.channel, info.bitrate);
                    if (dec_err != ESP_AUDIO_ERR_OK || raw.consumed == 0) {
                        ESP_LOGW(TAG, "decoder not ready/error: station=%s decoder_error=%d raw.consumed=%" PRIu32
                                      " frame.decoded_size=%" PRIu32,
                                 station.name.c_str(), dec_err, raw.consumed, frame.decoded_size);
                        break;
                    }
                    decoded_any = true;
                    no_decode_full_buffer_count = 0;
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
                    ++decoded_frame_count;
                    if (decoded_frame_count <= 3 || decoded_frame_count % 200 == 0) {
                        ESP_LOGI(TAG, "decoded station=%s frame=%" PRIu32 " raw.consumed=%" PRIu32
                                      " frame.decoded_size=%" PRIu32 " sample_rate=%d channels=%d bitrate=%d",
                                 station.name.c_str(), decoded_frame_count, raw.consumed, frame.decoded_size,
                                 sample_rate_, channels_, bitrate_kbps_);
                    }

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
                    ++no_decode_full_buffer_count;
                    ESP_LOGW(TAG, "no MP3 frame decoded: station=%s url=%s raw_len=%d failures=%d",
                             station.name.c_str(), url, raw_len, no_decode_full_buffer_count);
                    raw_len = 0;
                    if (no_decode_full_buffer_count >= 3) {
                        ESP_LOGW(TAG, "skip undecodable MP3 URL: station=%s url=%s", station.name.c_str(), url);
                        break;
                    }
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

            if (!running_ || reconnect_now_) {
                played_or_interrupted = true;
                break;
            }
            if (decoded_frame_count > 0) {
                played_or_interrupted = true;
                break;
            }
        }

        if (running_ && !reconnect_now_ && !played_or_interrupted) {
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
