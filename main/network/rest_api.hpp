#pragma once

#include <esp_http_server.h>


esp_err_t get_device_info_handler(httpd_req_t *req);
esp_err_t send_webhook_handler(httpd_req_t *req);
esp_err_t get_webhook_list_handler(httpd_req_t *req);
esp_err_t update_webhook_handler(httpd_req_t *req);

void register_rest_api_handlers(httpd_handle_t& server);