#pragma once

#include <cstdio>

#include "sdkconfig.h"

#include "esp_err.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include <esp_http_server.h>


/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)




/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename);



// ファイルを返す
esp_err_t httpd_resp_sendfile(httpd_req_t *req, const char *filepath);