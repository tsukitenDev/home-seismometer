#pragma once

#include <esp_err.h>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

struct WebhookSetting {
    int id;
    bool enabled;
    int shindoThreshold;
    std::string name;
    std::string url;
    nlohmann::json payloadTemplate;
};

std::vector<WebhookSetting> load_webhook_settings();
esp_err_t send_webhook_by_id(int webhook_id);
