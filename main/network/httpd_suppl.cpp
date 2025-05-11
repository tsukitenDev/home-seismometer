#include "httpd_suppl.hpp"

#include <cstdio>

#include "sdkconfig.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include "esp_log.h"

#include <esp_http_server.h>



/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename){
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".csv")) {
      return httpd_resp_set_type(req, "text/csv");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}



// ファイルを返す
esp_err_t httpd_resp_sendfile(httpd_req_t *req, const char *filepath){

    // ファイル名が有効か確認
    if (filepath == NULL || strlen(filepath) == 0 || strlen(filepath) >= FILE_PATH_MAX) {
        ESP_LOGE("stat", "Invalid filename");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        return ESP_FAIL;
    }

    struct stat st;
    if (stat(filepath, &st)) {
        ESP_LOGE("stat", "File not found: %s", filepath);
        httpd_resp_send_err(req,
            HTTPD_404_NOT_FOUND,
            "File not found"
            );
        return ESP_FAIL;
    }

    // 情報出力
    ESP_LOGI("stat", "File size: %ld kB, Free heap: %ld kB", (uint32_t)st.st_size / 1000,
        (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1000);

    // オープン
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        ESP_LOGE("stat", "Failed to open file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
        return ESP_FAIL;
    }

    // 属性設定
    set_content_type_from_file(req, filepath);

    char* heap_resp_data = (char*)heap_caps_malloc(sizeof(char)*st.st_size + 1, MALLOC_CAP_SPIRAM);
    //std::vector<char> heap_resp_data(st.st_size);
    //memset((void *)resp_data, 0, sizeof(resp_data));
    if (fread(heap_resp_data, sizeof(char), st.st_size, fp) == 0)
    {
        ESP_LOGE("stat", "fread failed");
    }
    heap_resp_data[st.st_size] = '\0';
    fclose(fp);
    
    // レスポンス送信
    esp_err_t ret = httpd_resp_send(req, heap_resp_data, HTTPD_RESP_USE_STRLEN);

    // ヒープ解放
    heap_caps_free(heap_resp_data);

    return ret;
}