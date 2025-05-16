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
    char buf[64];
    nlohmann::json resp_json;

    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > sizeof(buf)) {
        ESP_LOGE(TAG_API, "Query string too long: %d", query_len);
        resp_json["status"] = "error";
        resp_json["message"] = "Query string too long.";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param_val[16];
        if (httpd_query_key_value(buf, "id", param_val, sizeof(param_val)) == ESP_OK) {
            int webhook_id = atoi(param_val);
            ESP_LOGI(TAG_API, "Received send webhook request for ID: %d", webhook_id);
            
            esp_err_t send_err = send_webhook_by_id(webhook_id);

            if (send_err == ESP_OK) {
                resp_json["status"] = "success";
                resp_json["message"] = "Webhook sent successfully via ESP32.";
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
            } else if (send_err == ESP_ERR_NOT_FOUND) {
                resp_json["status"] = "error";
                resp_json["message"] = "Webhook ID not found.";
                httpd_resp_set_type(req, "application/json");
                httpd_resp_set_status(req, "404 Not Found");
                httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
            } else {
                resp_json["status"] = "error";
                resp_json["message"] = "Failed to send webhook via ESP32.";
                httpd_resp_set_type(req, "application/json");
                httpd_resp_set_status(req, "500 Internal Server Error");
                httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
            }
            return ESP_OK;
        }
    }

    resp_json["status"] = "error";
    resp_json["message"] = "Missing or invalid 'id' parameter.";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, resp_json.dump().c_str(), HTTPD_RESP_USE_STRLEN);
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
    auto settings = load_webhook_settings();
    nlohmann::json json_array = nlohmann::json::array();

    for (size_t i = 0; i < settings.size(); ++i) {
        const auto& setting = settings[i];
        nlohmann::json webhook_obj;
        webhook_obj["id"] = i; // Use index as ID
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

void register_rest_api_handlers(httpd_handle_t& server) {
    ESP_LOGI(TAG_API, "Registering API handlers");
    httpd_register_uri_handler(server, &device_info_uri);
    httpd_register_uri_handler(server, &webhook_send_uri);
    httpd_register_uri_handler(server, &webhook_list_uri);
}