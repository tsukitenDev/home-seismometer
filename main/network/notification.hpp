#pragma once

#include <esp_err.h>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

static constexpr int WEBHOOK_MAX_COUNT = 2;

struct EarthquakeData {
    int shindo;
};

struct WebhookSetting {
    bool enabled;
    int shindoThreshold;
    std::string name;
    std::string url;
    std::string payloadTemplate;
};

esp_err_t init_webhook_settings();
WebhookSetting get_webhook_setting(int index);
esp_err_t set_webhook_setting(int index, const WebhookSetting& setting);
WebhookSetting webhook_setting_from_json(const nlohmann::json& j);
std::string process_template(const std::string& template_str, const EarthquakeData& earthquake_data);
esp_err_t send_webhook(const std::string& url, const std::string& payload_str);
esp_err_t send_webhook_by_index(int index, const EarthquakeData& earthquake_data);
