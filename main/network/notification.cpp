#include "notification.hpp"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG_NOTIFICATION = "notification";

std::vector<WebhookSetting> load_webhook_settings() {
    ESP_LOGD(TAG_NOTIFICATION, "Loading webhook settings");
    return {
        // id, enabled, shindoThreshold, name, url, payloadTemplate
        {0, true, 30, "webhook-site", "https://webhook.site/b196cafb-e1b2-43c1-bb72-176cd682b809",
         R"({"content": "地震を検知しました。震度: {{shindo}}")"},
        {1, false, 50, "example", "https://example.com/webhook",
         R"({"content": "Earthquake detected"})"}
    };
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

esp_err_t send_webhook_by_id(int webhook_id, const EarthquakeData& earthquake_data) {

    vTaskDelay(10 / portTICK_PERIOD_MS);

    auto settings = load_webhook_settings();
    const WebhookSetting* target_setting = nullptr;
    if (webhook_id >= 0 && webhook_id < settings.size()) {
        target_setting = &settings[webhook_id];
    }

    if (!target_setting) {
        ESP_LOGE(TAG_NOTIFICATION, "Webhook setting with index %d not found.", webhook_id);
        return ESP_ERR_NOT_FOUND;
    }

    std::string payload_str = process_template(target_setting->payloadTemplate, earthquake_data);

    esp_http_client_config_t config = {
        .url = target_setting->url.c_str(),
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .event_handler = _webhook_http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG_NOTIFICATION, "Failed to initialise HTTP client for webhook index %d", webhook_id);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload_str.c_str(), payload_str.length());

    ESP_LOGI(TAG_NOTIFICATION, "Sending Webhook at index %d to %s", webhook_id, target_setting->url.c_str());
    ESP_LOGD(TAG_NOTIFICATION, "Webhook Payload: %s", payload_str.c_str());

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG_NOTIFICATION, "Webhook at index %d: HTTP Status = %d, content_length = %lld",
                 webhook_id,
                 status_code,
                 esp_http_client_get_content_length(client));
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGW(TAG_NOTIFICATION, "Webhook at index %d sent, but received non-success status: %d", webhook_id, status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG_NOTIFICATION, "Failed to send webhook at index %d: %s", webhook_id, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
