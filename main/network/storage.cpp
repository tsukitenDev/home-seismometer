#include "storage.hpp"


#include "esp_log.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_vfs_semihost.h"

#include "sdkconfig.h"

static const char *TAG = "storage";

// Mount path for the partition
const char *base_path = "/spiflash";
const char *userdata_path = "/userdata";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static wl_handle_t s_wl_handle_userdata = WL_INVALID_HANDLE;


#if CONFIG_WEB_DEPLOY_SEMIHOST
esp_err_t init_fs(void)
{
    esp_err_t ret = esp_vfs_semihost_register(base_path);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register semihost driver (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

#endif



#if CONFIG_WEB_DEPLOY_SPIFLASH
esp_err_t init_fs(){
  ESP_LOGI("storage", "Mounting FAT filesystem");
  // To mount device we need name of device partition, define base_path
  // and allow format partition in case if it is new one and was not formatted before
  const esp_vfs_fat_mount_config_t mount_config = {
          .format_if_mount_failed = false,
          .max_files = 4,
          .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
          //.use_one_fat = false,
  };

  esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
      return ESP_FAIL;
  }
  return ESP_OK;
}
#endif

esp_err_t init_userdata_fs() {
    ESP_LOGI(TAG, "Mounting userdata FAT filesystem");
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(userdata_path, "userdata", &mount_config, &s_wl_handle_userdata);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount userdata FATFS (%s)", esp_err_to_name(err));
        return ESP_FAIL;
    }
    return ESP_OK;
}