#pragma once

#include <esp_err.h>
#include <string>
#include <vector>

struct EarthquakeData {
    int shindo;
};

struct WebhookSetting {
    int id;
    bool enabled;
    int shindoThreshold;
    std::string name;
    std::string url;
    std::string payloadTemplate;
};

std::vector<WebhookSetting> load_webhook_settings();
std::string process_template(const std::string& template_str, const EarthquakeData& earthquake_data);
esp_err_t send_webhook_by_id(int webhook_id, const EarthquakeData& earthquake_data);
