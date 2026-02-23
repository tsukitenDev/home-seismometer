#include "rest_api.hpp"

#include "esp_log.h"
#include "wlan.hpp"
#include "esp_http_server.h" 
#include "nlohmann/json.hpp"
#include "httpd_suppl.hpp"
#include "device_info.hpp"
#include "notification.hpp"

static const char *TAG_API = "rest_api";

extern device_info_t device_info;

esp_err_t get_device_info_handler(httpd_req_t *req) {
    nlohmann::json json_obj;
    json_obj["device_name"] = device_info.device_name;
    json_obj["firmware_version"] = device_info.firmware_version;
    json_obj["sensor_name"] = device_info.sensor_name;

    ap_record_t current_ap = wifi_get_current_ap();
    json_obj["ap_ssid"] = current_ap.ssid;
    json_obj["ap_rssi"] = current_ap.rssi;
    json_obj["ap_authmode"] = current_ap.authmode;

    json_obj["ip_address"] = wifi_get_ip_address_str();

    std::string json_response_str = json_obj.dump();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response_str.c_str(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t device_info_uri = {
    .uri       = "/api/device_info",
    .method    = HTTP_GET,
    .handler   = get_device_info_handler,
    .user_ctx  = NULL
};


esp_err_t send_webhook_handler(httpd_req_t *req) {
    nlohmann::json resp_json;

    int content_len = req->content_len;
    if (content_len <= 0 || content_len > 4096) {
        resp_json["status"] = "error";
        resp_json["message"] = "Invalid content length.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    std::string body(content_len, '\0');
    int received = httpd_req_recv(req, &body[0], content_len);
    if (received != content_len) {
        resp_json["status"] = "error";
        resp_json["message"] = "Failed to receive request body.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    nlohmann::json req_json = nlohmann::json::parse(body, nullptr, false);
    if (req_json.is_discarded()) {
        resp_json["status"] = "error";
        resp_json["message"] = "Invalid JSON format.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    std::string url = req_json.value("url", std::string(""));
    std::string payload_template = req_json.value("payload_template", std::string(""));

    if (url.empty() || payload_template.empty()) {
        resp_json["status"] = "error";
        resp_json["message"] = "Missing 'url' or 'payload_template'.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // テスト用EarthquakeData
    EarthquakeData test_data;
    test_data.time_shake_start = time(nullptr);
    test_data.report_count = 1;
    test_data.shindo = 35;
    test_data.is_test = true;

    std::string payload_str = process_template(payload_template, test_data);
    ESP_LOGI(TAG_API, "Test sending webhook to %s", url.c_str());

    esp_err_t send_err = send_webhook(url, payload_str);

    if (send_err == ESP_OK) {
        resp_json["status"] = "success";
        resp_json["message"] = "Webhook sent successfully.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
    } else {
        resp_json["status"] = "error";
        resp_json["message"] = "Failed to send webhook.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

httpd_uri_t webhook_send_uri = {
    .uri       = "/api/webhook/send",
    .method    = HTTP_POST,
    .handler   = send_webhook_handler,
    .user_ctx  = NULL
};

// Webhook設定一覧取得
esp_err_t get_webhook_list_handler(httpd_req_t *req) {
    nlohmann::json json_array = nlohmann::json::array();

    for (int i = 0; i < WEBHOOK_MAX_COUNT; ++i) {
        auto setting = get_webhook_setting(i);
        nlohmann::json webhook_obj;
        webhook_obj["id"] = i;
        webhook_obj["name"] = setting.name;
        webhook_obj["url"] = setting.url;
        webhook_obj["payload_template"] = setting.payloadTemplate;
        webhook_obj["enabled"] = setting.enabled;
        webhook_obj["shindo_threshold"] = setting.shindoThreshold;
        json_array.push_back(webhook_obj);
    }

    std::string json_response_str = json_array.dump();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response_str.c_str(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t webhook_list_uri = {
    .uri       = "/api/webhook/list",
    .method    = HTTP_GET,
    .handler   = get_webhook_list_handler,
    .user_ctx  = NULL
};

// Webhook設定更新
esp_err_t update_webhook_handler(httpd_req_t *req) {
    nlohmann::json resp_json;

    int content_len = req->content_len;
    if (content_len <= 0 || content_len > 4096) {
        resp_json["status"] = "error";
        resp_json["message"] = "Invalid content length.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    std::string body(content_len, '\0');
    int received = httpd_req_recv(req, &body[0], content_len);
    if (received != content_len) {
        resp_json["status"] = "error";
        resp_json["message"] = "Failed to receive request body.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    nlohmann::json req_json = nlohmann::json::parse(body, nullptr, false);

    if (req_json.is_discarded()) {
        ESP_LOGE(TAG_API, "JSON parse error in webhook update");
        resp_json["status"] = "error";
        resp_json["message"] = "Invalid JSON format.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (!req_json.is_array()) {
        resp_json["status"] = "error";
        resp_json["message"] = "Request body must be a JSON array.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (req_json.size() > WEBHOOK_MAX_COUNT) {
        resp_json["status"] = "error";
        resp_json["message"] = "Too many webhook settings (max " + std::to_string(WEBHOOK_MAX_COUNT) + ").";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    std::vector<WebhookSetting> settings;
    for (size_t i = 0; i < req_json.size(); ++i) {
        settings.push_back(webhook_setting_from_json(req_json[i]));
    }

    esp_err_t save_err = ESP_OK;
    for (size_t i = 0; i < settings.size(); ++i) {
        save_err = set_webhook_setting(i, settings[i]);
        if (save_err != ESP_OK) break;
    }
    if (save_err != ESP_OK) {
        resp_json["status"] = "error";
        resp_json["message"] = "Failed to save webhook settings.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    resp_json["status"] = "success";
    resp_json["message"] = "Webhook settings saved.";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t webhook_update_uri = {
    .uri       = "/api/webhook/update",
    .method    = HTTP_POST,
    .handler   = update_webhook_handler,
    .user_ctx  = NULL
};

void register_rest_api_handlers(httpd_handle_t& server) {
    ESP_LOGI(TAG_API, "Registering API handlers");
    httpd_register_uri_handler(server, &device_info_uri);
    httpd_register_uri_handler(server, &webhook_send_uri);
    httpd_register_uri_handler(server, &webhook_list_uri);
    httpd_register_uri_handler(server, &webhook_update_uri);
}