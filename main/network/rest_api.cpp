#include "rest_api.hpp"

#include "esp_log.h"

#include "wlan.hpp"

#include "esp_http_server.h" 

#include "nlohmann/json.hpp"

#include "httpd_suppl.hpp"


#include "device_info.hpp"



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


void register_rest_api_handlers(httpd_handle_t& server) {
    ESP_LOGI("API", "Registering API handlers");
    httpd_register_uri_handler(server, &device_info_uri);
}