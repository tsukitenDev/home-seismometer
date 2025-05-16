#include "network.hpp"


#include <cstdio>
#include <sstream>
#include <ios>
#include <iomanip>
#include <vector>
#include <tuple>
#include <string>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <esp_http_server.h>

#include "httpd_suppl.hpp"

#include "wlan.hpp"

#include "storage.hpp"

#include "rest_api.hpp"

#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY


/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192




esp_err_t get_monitor_handler(httpd_req_t * req) {
    httpd_resp_sendfile(req, "/spiflash/monitor.html");
    //httpd_resp_send(req, monitor_html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


httpd_uri_t monitor = {
        .uri       = "/monitor",  // Match all URIs of type /path/to/file
    	.method    = HTTP_GET,
    	.handler   = get_monitor_handler,
        .user_ctx = NULL
};


esp_err_t get_setting_handler(httpd_req_t *req) {
    httpd_resp_sendfile(req, "/spiflash/setting.html");
    return ESP_OK;
}

httpd_uri_t setting = {
    .uri       = "/setting",
    .method    = HTTP_GET,
    .handler   = get_setting_handler,
    .user_ctx  = NULL
};


/*
esp_err_t get_handler(httpd_req_t *req)
{

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>Hello World!</h1>", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

char userctx[] = "Hello World!";
httpd_uri_t hello = {
        .uri       = "/hello",  // Match all URIs of type /path/to/file
    	.method    = HTTP_GET,
    	.handler   = get_handler,
        .user_ctx = userctx
};

*/

httpd_handle_t server_handle;

static const char *TAG = "network";

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    return ESP_OK;
}


static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = echo_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

esp_err_t httpd_ws_send_frame_to_all_clients(httpd_ws_frame_t* ws_pkt) {
    static constexpr size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients] = {0};

    esp_err_t ret = httpd_get_client_list(server_handle, &fds, client_fds);

    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "ESP_ERR_INVALID_ARG");
        return ret;
    }

    for (int i = 0; i < fds; i++) {
        auto client_info = httpd_ws_get_fd_info(server_handle, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(server_handle, client_fds[i], ws_pkt);
        }
    }

    return ESP_OK;
}





esp_err_t get_csv_file_handler(httpd_req_t *req) {
    const char *TAG = "CSV_FILE";
    
    // URLからファイル名を取得
    char filepath[FILE_PATH_MAX];
    const char *uri_path = req->uri;
    const char *filename = NULL;
    
    // URIが有効か確認
    if (uri_path != NULL && strlen(uri_path) > sizeof("/csv/")) {
        filename = uri_path + sizeof("/csv/") - 1;
    }
    /*
    // ファイル名にディレクトリトラバーサル攻撃がないか確認
    if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
        ESP_LOGE(TAG, "Invalid characters in filename");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }
    */
    
    // ファイルパスを構築
    snprintf(filepath, sizeof(filepath), "/spiflash/data/%s", filename);
    
    
    // Content-Dispositionヘッダーを追加してダウンロードを促す
    char attachment[100];
    snprintf(attachment, sizeof(attachment), "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", attachment);

    esp_err_t ret = httpd_resp_sendfile(req, filepath);
    return ret;
}



httpd_uri_t csv_file = {
    .uri       = "/csv/*",
    .method    = HTTP_GET,
    .handler   = get_csv_file_handler,
    .user_ctx  = NULL
};







httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    #if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a priviliged user in linux.
    // So when a unpriviliged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unpriviliged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
    #endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &monitor);
        httpd_register_uri_handler(server, &setting);
        register_rest_api_handlers(server);

        httpd_register_uri_handler(server, &csv_file);
        
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}




void ws_send_data(std::string tag, std::vector<std::tuple<int64_t, std::array<float, 3>>>& data, uint64_t it, uint64_t n){
    std::ostringstream oss;
    oss << "{ \"" << tag <<  "\" : [";
    for(int i=it-n+1; i<=it;i++){
        int64_t time=0;
        std::array<float, 3> acc;
        time = std::get<0>(data[i & (data.size()-1)]);
        acc = std::get<1>(data[i & (data.size()-1)]);
        float x = acc[0], y=acc[1], z=acc[2];
        oss << "[" << time << ", " << x << ", " << y << ", " << z << "]";
        if(i != it) oss << ", ";
    }

    oss << "]}";
    std::string str = oss.str();
    char* cstr = new char[str.size() + 1];
    std::char_traits<char>::copy(cstr, str.c_str(), str.size() + 1);

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    ws_pkt.payload = (uint8_t*)cstr;
    ws_pkt.len = strlen(cstr);

    esp_err_t ret = httpd_ws_send_frame_to_all_clients(&ws_pkt);
    ESP_ERROR_CHECK(ret);
    delete[] cstr;
}


// 明示的実体化
template void ws_send_data(std::string tag, std::vector<std::tuple<int64_t, float>>& data, uint64_t it, uint64_t n);
template void ws_send_data(std::string tag, std::vector<std::tuple<int64_t, int32_t>>& data, uint64_t it, uint64_t n);

template<typename T>
void ws_send_data(std::string tag, std::vector<std::tuple<int64_t, T>>& data, uint64_t it, uint64_t n){
    std::ostringstream oss;
    oss << "{ \"" << tag <<  "\" : [";
    for(int i=it-n+1; i<=it;i++){
        auto [time, x] = data[i & (data.size()-1)];
        oss << "[" << time << ", " << x << "]";
        if(i != it) oss << ", ";
    }

    oss << "]}";
    std::string str = oss.str();
    char* cstr = new char[str.size() + 1];
    std::char_traits<char>::copy(cstr, str.c_str(), str.size() + 1);

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)cstr;
    ws_pkt.len = strlen(cstr);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_to_all_clients(&ws_pkt);
    delete[] cstr;

}

void httpd_init(){
    init_fs();
    //initi_web_page_buffer();
    server_handle = start_webserver();
    return;
}

