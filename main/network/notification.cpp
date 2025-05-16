#include "notification.hpp"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char *TAG_NOTIFICATION = "notification";

std::vector<WebhookSetting> load_webhook_settings() {
    ESP_LOGD(TAG_NOTIFICATION, "Loading webhook settings");
    return {
        // id, enabled, shindoThreshold, name, url, payloadTemplate
        {0, true, 30, "webhook-site", "https://webhook.site", {{"content", "test"}}},
        {1, false, 50, "example", "https://example.com/webhook", {{"alert", "Earthquake detected"}, {"level", "3"}}}
    };
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

esp_err_t send_webhook_by_id(int webhook_id) {
    auto settings = load_webhook_settings();
    const WebhookSetting* target_setting = nullptr;
    if (webhook_id >= 0 && webhook_id < settings.size()) {
        target_setting = &settings[webhook_id];
    }

    if (!target_setting) {
        ESP_LOGE(TAG_NOTIFICATION, "Webhook setting with index %d not found.", webhook_id);
        return ESP_ERR_NOT_FOUND;
    }

    std::string payload_str = target_setting->payloadTemplate.dump();

    esp_http_client_config_t config = {
        .url = target_setting->url.c_str(),
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .event_handler = _webhook_http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach
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
