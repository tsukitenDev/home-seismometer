#include "notification.hpp"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "storage.hpp"
#include "nlohmann/json.hpp"

#include <cstdio>
#include <string>

static const char *TAG_NOTIFICATION = "notification";

static constexpr int WEBHOOK_MAX_RETRIES = 3;
static constexpr int WEBHOOK_RETRY_DELAY_MS = 2000;
static constexpr int WEBHOOK_TIMEOUT_MS = 15000;

static const char *WEBHOOK_SETTINGS_FILE = "/userdata/webhook.json";

static bool webhook_settings_initialized = false;

static std::vector<WebhookSetting> webhook_settings;



static nlohmann::json webhook_setting_to_json(const WebhookSetting& s) {
    return {
        {"enabled", s.enabled},
        {"shindo_threshold", s.shindoThreshold},
        {"name", s.name},
        {"url", s.url},
        {"payload_template", s.payloadTemplate}
    };
}

WebhookSetting get_default_webhook_setting() {
    // enabled, shindoThreshold, name, url, payloadTemplate
    return {false, 35, "未設定", "",
         R"({"content": "揺れを検知しました。震度: {{shindo}}"})"};
}


WebhookSetting webhook_setting_from_json(const nlohmann::json& j) {
    WebhookSetting s = get_default_webhook_setting();
    s.enabled = j.value("enabled", s.enabled);
    s.shindoThreshold = j.value("shindo_threshold", s.shindoThreshold);
    s.name = j.value("name", s.name);
    s.url = j.value("url", s.url);
    s.payloadTemplate = j.value("payload_template", s.payloadTemplate);
    return s;
}




/**
 * ファイルからwebhook設定を読み込む
 * 読み込みに失敗した場合は空のvectorとする
 */
std::vector<WebhookSetting> read_webhook_settings_file() {
    FILE *f = fopen(WEBHOOK_SETTINGS_FILE, "r");
    if (!f) {
        ESP_LOGI(TAG_NOTIFICATION, "Webhook settings file not found");
        return {};
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 8192) {
        ESP_LOGW(TAG_NOTIFICATION, "Invalid webhook settings file size: %ld", file_size);
        fclose(f);
        return {};
    }

    std::string content(file_size, '\0');
    size_t read_size = fread(&content[0], 1, file_size, f);
    fclose(f);

    if (read_size != static_cast<size_t>(file_size)) {
        ESP_LOGW(TAG_NOTIFICATION, "Failed to read webhook settings file fully");
        return {};
    }

    nlohmann::json json_array = nlohmann::json::parse(content, nullptr, false);

    if (json_array.is_discarded() || !json_array.is_array()) {
        ESP_LOGE(TAG_NOTIFICATION, "Invalid webhook settings JSON");
        return {};
    }

    std::vector<WebhookSetting> settings;
    for (const auto& item : json_array) {
        settings.push_back(webhook_setting_from_json(item));
    }

    return settings;
}

static esp_err_t save_webhook_settings();

/**
 * webhook設定の初期化
 * ファイルから読み込み、内部vectorに格納する
 * ファイルが存在しない・不正な場合はデフォルト設定で初期化
 */
esp_err_t init_webhook_settings() {
    ESP_LOGI(TAG_NOTIFICATION, "Initializing webhook settings");

    webhook_settings = read_webhook_settings_file();

    if (webhook_settings.empty()) {
        ESP_LOGI(TAG_NOTIFICATION, "No valid webhook settings found, writing defaults");
    }

    bool needs_save = false;
    while (webhook_settings.size() < WEBHOOK_MAX_COUNT) {
        webhook_settings.push_back(get_default_webhook_setting());
        needs_save = true;
    }

    if (needs_save) {
        esp_err_t err = save_webhook_settings();
        if (err != ESP_OK) {
            ESP_LOGE(TAG_NOTIFICATION, "Failed to save initial webhook settings");
            return err;
        }
    }

    webhook_settings_initialized = true;

    ESP_LOGI(TAG_NOTIFICATION, "Webhook settings initialized (%d entries)", (int)webhook_settings.size());
    return ESP_OK;
}

WebhookSetting get_webhook_setting(int index) {
    if(!webhook_settings_initialized) {
        init_webhook_settings();
    }
    if (index < 0 || index >= static_cast<int>(webhook_settings.size())) {
        ESP_LOGW(TAG_NOTIFICATION, "Webhook setting index %d out of range, returning default", index);
        return get_default_webhook_setting();
    }
    return webhook_settings[index];
}

esp_err_t set_webhook_setting(int index, const WebhookSetting& setting) {
    if (index < 0 || index >= static_cast<int>(webhook_settings.size())) {
        ESP_LOGE(TAG_NOTIFICATION, "Webhook setting index %d out of range", index);
        return ESP_ERR_INVALID_ARG;
    }
    webhook_settings[index] = setting;
    return save_webhook_settings();
}

static esp_err_t save_webhook_settings() {
    nlohmann::json json_array = nlohmann::json::array();
    for (const auto& s : webhook_settings) {
        json_array.push_back(webhook_setting_to_json(s));
    }

    std::string json_str = json_array.dump();
    ESP_LOGD(TAG_NOTIFICATION, "Saving webhook settings: %s", json_str.c_str());

    // 一時ファイルに書き込み後リネームで安全に上書き
    const char *tmp_file = "/userdata/webhook.tmp";

    FILE *f = fopen(tmp_file, "w");
    if (!f) {
        ESP_LOGE(TAG_NOTIFICATION, "Failed to open temp file for writing");
        return ESP_FAIL;
    }

    size_t written = fwrite(json_str.c_str(), 1, json_str.size(), f);
    fclose(f);

    if (written != json_str.size()) {
        ESP_LOGE(TAG_NOTIFICATION, "Failed to write webhook settings fully");
        remove(tmp_file);
        return ESP_FAIL;
    }

    // FATFSではrename先が存在する場合に失敗するため、先に削除
    remove(WEBHOOK_SETTINGS_FILE);
    if (rename(tmp_file, WEBHOOK_SETTINGS_FILE) != 0) {
        ESP_LOGE(TAG_NOTIFICATION, "Failed to rename temp file to settings file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_NOTIFICATION, "Webhook settings saved successfully");
    return ESP_OK;
}

std::string shindo_to_str(int shindo) {
    std::string shindo_text;

    // 震度階級
    if (shindo < 5) {
        shindo_text = "0以下";
    } else if (shindo < 15) {
        shindo_text = "1";
    } else if (shindo < 25) {
        shindo_text = "2";
    } else if (shindo < 35) {
        shindo_text = "3";
    } else if (shindo < 45) {
        shindo_text = "4";
    } else if (shindo < 50) {
        shindo_text = "5弱";
    } else if (shindo < 55) {
        shindo_text = "5強";
    } else if (shindo < 60) {
        shindo_text = "6弱";
    } else if (shindo < 65) {
        shindo_text = "6強";
    } else {
        shindo_text = "7";
    }

    // 計測震度
    shindo_text += "(";
    if(shindo < 0){
        shindo_text += "-";
        shindo *= -1;
    }
    shindo_text += std::to_string(shindo / 10);
    shindo_text += ".";
    shindo_text += std::to_string(shindo % 10);
    shindo_text += ")";

    return shindo_text;
}

std::string process_template(const std::string& template_str, const EarthquakeData& earthquake_data) {
    std::string result = template_str;
    std::string shindo_str = shindo_to_str(earthquake_data.shindo);
    size_t pos = 0;
    while ((pos = result.find("{{shindo}}", pos)) != std::string::npos) {
        result.replace(pos, 10, shindo_str);
        pos += shindo_str.length();
    }

    return result;
}

esp_err_t _webhook_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG_NOTIFICATION, "WEBHOOK_HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

esp_err_t send_webhook(const std::string& url, const std::string& payload_str) {

    vTaskDelay(10 / portTICK_PERIOD_MS);

    esp_err_t err = ESP_FAIL;

    for (int attempt = 0; attempt < WEBHOOK_MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG_NOTIFICATION, "Retry %d/%d for webhook to %s",
                     attempt, WEBHOOK_MAX_RETRIES - 1, url.c_str());
            vTaskDelay(WEBHOOK_RETRY_DELAY_MS / portTICK_PERIOD_MS);
            ESP_LOGI(TAG_NOTIFICATION, "[diag] Free heap before retry: %lu",
                     (unsigned long)esp_get_free_heap_size());
        }

        esp_http_client_config_t config = {
            .url = url.c_str(),
            .method = HTTP_METHOD_POST,
            .timeout_ms = WEBHOOK_TIMEOUT_MS,
            .event_handler = _webhook_http_event_handler,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .keep_alive_enable = false,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            ESP_LOGE(TAG_NOTIFICATION, "Failed to initialise HTTP client for webhook");
            continue;
        }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, payload_str.c_str(), payload_str.length());

        ESP_LOGI(TAG_NOTIFICATION, "Sending Webhook to %s (attempt %d)",
                 url.c_str(), attempt + 1);
        ESP_LOGD(TAG_NOTIFICATION, "Webhook Payload: %s", payload_str.c_str());

        err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG_NOTIFICATION, "Webhook HTTP Status = %d, content_length = %lld",
                     status_code,
                     esp_http_client_get_content_length(client));
            if (status_code < 200 || status_code >= 300) {
                ESP_LOGW(TAG_NOTIFICATION, "Webhook sent, but received non-success status: %d", status_code);
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG_NOTIFICATION, "Failed to send webhook (attempt %d): %s",
                     attempt + 1, esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);

        if (err == ESP_OK) {
            break;
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG_NOTIFICATION, "Webhook to %s failed after %d attempts", url.c_str(), WEBHOOK_MAX_RETRIES);
    }

    return err;
}

esp_err_t send_webhook_by_index(int index, const EarthquakeData& earthquake_data) {
    if (index < 0 || index >= static_cast<int>(webhook_settings.size())) {
        ESP_LOGE(TAG_NOTIFICATION, "Webhook setting with index %d not found.", index);
        return ESP_ERR_NOT_FOUND;
    }

    const auto& target_setting = webhook_settings[index];
    std::string payload_str = process_template(target_setting.payloadTemplate, earthquake_data);
    return send_webhook(target_setting.url, payload_str);
}
