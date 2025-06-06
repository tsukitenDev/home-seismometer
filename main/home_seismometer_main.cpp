#include <cstdio>
#include <array>
#include <queue>
#include <vector>
#include <string>
#include <ios>
#include <iomanip>
#include <sys/time.h>

#include <chrono>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "driver/gpio.h"

#include "data_structure.hpp"


#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <LGFX_EQIS1_SSD1306.hpp>
#include <LGFX_1732S019_ST7789.hpp>
#include "driver/spi_master.h"

#include "network/wlan.hpp"
#include "network/network.hpp"

#include "network/device_info.hpp"


#include "task_improv.hpp"
#include "task_ws_send.hpp"
#include "task_seis_fft.hpp"

#include "board_def.hpp"


static const std::string firmware_version = "v0.1.3";

static constexpr uint32_t LONG_BUF_SIZE = 8192; 


ringBuffer<std::tuple<int64_t, std::array<float, 3>>> raw_acc_buffer(LONG_BUF_SIZE);
ringBuffer<std::tuple<int64_t, std::array<float, 3>>> hpf_acc_buffer(LONG_BUF_SIZE);

ringBuffer<std::tuple<int64_t, float>> shindo_history(LONG_BUF_SIZE);


QueueHandle_t que_bufCount;

uint32_t ws_send_period = 20;





// LovyanGFX
#if BOARD_355_1732S019
static LGFX_1732S019_ST7789 lcd;
const static uint8_t LCD_ROTATION = 1;
static int32_t shindo_threshold = 5;
static std::string firmware_name = "home-seismometer";
static std::string device_name = "CYD";
static std::string sensor_name = "ADXL355";
static std::string mdns_hostname =  "adxl";
static std::string mdns_instancename =  "CYD ESP32 Webserver";
static std::string monitor_url = "adxl.local/monitor";

#elif BOARD_EQIS1
static LGFX_EQIS1_SSD1306 lcd;
const static uint8_t LCD_ROTATION = 2;
static int32_t shindo_threshold = 15;
static std::string firmware_name = "home-seismometer";
static std::string device_name = "EQIS-1";
static std::string sensor_name = "LSM6DSO";
static std::string mdns_hostname =  "eqis-1";
static std::string mdns_instancename =  "EQIS-1 ESP32 Webserver";
static std::string monitor_url = "eqis-1.local/monitor";

#endif

device_info_t device_info = {
    .firmware_name = firmware_name,
    .firmware_version = firmware_version,
    .device_name = device_name,
    .sensor_name = sensor_name,
    .mdns_hostname =  mdns_hostname,
    .mdns_instancename =  mdns_instancename,
    .monitor_url = monitor_url,
};

EventGroupHandle_t s_shindo_fft_event_group;

QueueHandle_t que_process_shindo;
QueueHandle_t que_shindo_res;

extern SENSOR_STATE acc_sensor_state;
extern SEISMOMETER_STATE seis_state;
bool is_start_shindo = false;

extern int64_t last_cnt;


static LGFX_Sprite canvas(&lcd);


void print_heap_info(){
    multi_heap_info_t info_psram, info_internal;
    heap_caps_get_info(&info_psram, MALLOC_CAP_SPIRAM);
    heap_caps_get_info(&info_internal, MALLOC_CAP_INTERNAL);

    ESP_LOGI("heap", "INTERNAL %d / %d kB (%d %% used), MAX %d kB",
                            info_internal.total_allocated_bytes / 1024,
                            (info_internal.total_free_bytes + info_internal.total_allocated_bytes) / 1024,
                            100 * info_internal.total_allocated_bytes / (info_internal.total_free_bytes + info_internal.total_allocated_bytes),
                            info_internal.largest_free_block / 1024);
    
    ESP_LOGI("heap", "PSRAM %d / %d kB (%d %% used), MAX %d kB",
                            info_psram.total_allocated_bytes / 1024,
                            (info_psram.total_free_bytes + info_psram.total_allocated_bytes) / 1024,
                            100 * info_psram.total_allocated_bytes / (info_psram.total_free_bytes + info_psram.total_allocated_bytes),
                            info_psram.largest_free_block / 1024);
}








void task_display(void * pvParameters){
    canvas.setPsram(true);
    canvas.setTextWrap(false);
    canvas.createSprite(lcd.width(), lcd.height());

    int64_t cnt = 0;
    std::string wifi_status;
    std::string sensor_status;
    std::string seis_status;
    WIFI_STATE wifi_current_state;
    
    int display_intensity = 0;
    std::string shindo_text;

    std::string spinner =  "|/-\\";

    int32_t display_mode = 0; // 0: ステータス  1: 震度拡大  2: 待機

    vTaskDelay(100 / portTICK_PERIOD_MS);

    while(1) {
        wifi_current_state = wifi_get_state();
        if(wifi_current_state == WIFI_STATE::NOT_CONFIGURED) wifi_status = "Not Configured";
        if(wifi_current_state == WIFI_STATE::CONFIGURED)     wifi_status = "Configured";
        if(wifi_current_state == WIFI_STATE::CONNECTING)     wifi_status = "Connecting";
        if(wifi_current_state == WIFI_STATE::CONNECTED)      wifi_status = "Connected";
        if(wifi_current_state == WIFI_STATE::CONNECT_FAILED) wifi_status = "Connect Failed";
        if(wifi_current_state == WIFI_STATE::NOT_CONNECTED)  wifi_status = "Not Connected";

        if(acc_sensor_state == SENSOR_STATE::CONNECTED) sensor_status = "Connected";
        if(acc_sensor_state == SENSOR_STATE::CONNECT_FAILED) sensor_status = "Connect Failed";

        if(acc_sensor_state != SENSOR_STATE::CONNECTED ||
           seis_state == SEISMOMETER_STATE::NOT_STARTED){
            seis_status = "";
        }
        else if(seis_state == SEISMOMETER_STATE::SHINDO_STABILIZING ||
                seis_state == SEISMOMETER_STATE::SHINDO_STABILIZED){
            seis_status = "Stabilized";
        }
        else seis_status = "Stabilizing";


        cnt = last_cnt;
        int intensity_int10x = std::get<1>(shindo_history[cnt]);

        // 最大震度を表示する
        if(display_intensity < intensity_int10x) display_intensity = intensity_int10x;

        if(display_mode == 0) {
            if(seis_state == SEISMOMETER_STATE::SHINDO_STABILIZED && cnt > 120 * 100) {
                display_intensity = 0;
                display_mode = 2;
            }
        }else if(display_mode == 1){
            if(intensity_int10x < shindo_threshold) {
                display_intensity = 0;
                display_mode = 2;
            }
        }else{ // display_mode == 2
            if(intensity_int10x >= shindo_threshold) {
                display_mode = 1;
            }
        }

        // check
        // display_mode = 1;
        // display_intensity = ((cnt / 100) * 5 ) % 80;

        if(display_intensity < 5) {
            shindo_text = "0 ";
        }else if(display_intensity < 15) {
            shindo_text = "1 ";
        }else if(display_intensity < 25) {
            shindo_text = "2 ";
        }else if(display_intensity < 35) {
            shindo_text = "3 ";
        }else if(display_intensity < 45) {
            shindo_text = "4 ";
        }else if(display_intensity < 50) {
            shindo_text = "5-";
        }else if(display_intensity < 55) {
            shindo_text = "5+";
        }else if(display_intensity < 60) {
            shindo_text = "6-";
        }else if(display_intensity < 65) {
            shindo_text = "6+";
        }else{
            shindo_text = "7 ";
        }

        if(display_mode == 0) {

            float gal_x = std::get<1>(raw_acc_buffer[cnt])[0];
            float gal_y = std::get<1>(raw_acc_buffer[cnt])[1];
            float gal_z = std::get<1>(raw_acc_buffer[cnt])[2];
            float shindo = (float)intensity_int10x / 10;

            canvas.setFont(&fonts::Font0);
            canvas.fillScreen(TFT_BLACK);
            canvas.setCursor(0,0);
            canvas.printf("%s\n", monitor_url.c_str());
            canvas.printf("WiFi : %s\n", wifi_status.c_str());
            canvas.printf("Acc  : %s\n", sensor_status.c_str());
            canvas.printf("seis : %s\n", seis_status.c_str());
            canvas.printf("\n");
            canvas.printf("X      Y      Z cm/s2\n");
            canvas.printf("%6.1f %6.1f %6.1f\n", gal_x, gal_y, gal_z);
            if(seis_state == SEISMOMETER_STATE::SHINDO_STABILIZING ||
               seis_state == SEISMOMETER_STATE::SHINDO_STABILIZED){
                canvas.printf("shindo %.1f", shindo);
            }

            canvas.pushSprite(0, 0);

        }else if(display_mode == 1) {
            canvas.setFont(&fonts::Font0);
            canvas.setTextSize(1);
            canvas.fillScreen(TFT_BLACK);
            canvas.setCursor(0,0);
            if(wifi_current_state == WIFI_STATE::CONNECTED){
                canvas.printf("%s\n", monitor_url.c_str());
            }else{
                canvas.printf("WiFi : %s\n", wifi_status.c_str());                
            } 
            canvas.setCursor(0, canvas.height() - canvas.fontHeight() * 2 - 6);
            canvas.printf(" Seismic\n Intensity");
            // 過去の震度を表示しているときは*マークを表示
            if(intensity_int10x + 5 < display_intensity) canvas.printf("*");
            canvas.setFont(&fonts::Font6);
            canvas.setCursor(canvas.width() - canvas.textWidth("00") + 4,
                             canvas.height() - canvas.fontHeight() + 6);
            canvas.printf("%c", shindo_text[0]);
            canvas.setFont(&fonts::Font4);
            canvas.setTextSize(2);
            canvas.setCursor(canvas.getCursorX() + 2, canvas.getCursorY());
            canvas.printf("%c", shindo_text[1]);
            // フリーズ確認
            if((cnt / 100) % 2 == 1) canvas.drawPixel(canvas.width() - 1, canvas.height() - 1, TFT_WHITE);
            
            canvas.pushSprite(0, 0);

        }else if(display_mode == 2) {
            canvas.setFont(&fonts::Font0);
            canvas.setTextSize(1);
            canvas.fillScreen(TFT_BLACK);
            canvas.setCursor(canvas.width() - canvas.textWidth("-") - 2, canvas.height() - canvas.fontHeight() - 2);
            // フリーズ確認
            canvas.printf("%c", spinner[(cnt / 100) % 4]);

            canvas.pushSprite(0, 0);

        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}




extern "C" void app_main(void)
{
    ESP_LOGI("app", "%s for %s %s", firmware_name.c_str(), device_name.c_str(), firmware_version.c_str());
    print_heap_info();

    // LGFX初期設定
    lcd.init();
    lcd.setRotation(LCD_ROTATION);


    BaseType_t create_task_result;

    // 加速度センサ用 SPIバス設定
    esp_err_t ret;
    spi_bus_config_t buscfg{
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);


    create_task_result = xTaskCreatePinnedToCore(task_display, "display task", 4096, NULL, 1, NULL, 0);
    if (create_task_result != pdPASS) ESP_LOGI("task", "Failed to create task%d", create_task_result);


    // NVS領域　初期設定
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    que_process_shindo = xQueueCreate(8, sizeof(int64_t));
    que_shindo_res = xQueueCreate(8, sizeof(int32_t));
    s_shindo_fft_event_group = xEventGroupCreate();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    create_task_result = xTaskCreatePinnedToCore(task_process_shindo_fft, "process shindo", 4096, NULL, 5, NULL, 1);
    if (create_task_result != pdPASS) ESP_LOGI("task", "Failed to create task%d", create_task_result);
    
    xEventGroupWaitBits(s_shindo_fft_event_group, BIT_TASK_PROCESS_SHINDO_READY, pdFALSE, pdFALSE, portMAX_DELAY);
    
    // 無線LAN 初期設定
    wifi_init();
    httpd_init();


    improv_set_info(firmware_name, firmware_version,sensor_name, device_name, monitor_url);
    create_task_result = xTaskCreatePinnedToCore(task_improv, "Task improv serial", 8192, NULL, 5, NULL, 0);
    if (create_task_result != pdPASS) ESP_LOGI("task", "Failed to create task%d", create_task_result);


    // websocket 送信開始
    que_bufCount = xQueueCreate(8, 10);
    create_task_result = xTaskCreatePinnedToCore(task_ws_send_data, "Task Send websocket", 8192, NULL, 1, NULL, 0);
    if (create_task_result != pdPASS) ESP_LOGI("task", "Failed to create task%d", create_task_result);

    xEventGroupSetBits(s_shindo_fft_event_group, BIT_TASK_PROCESS_SHINDO_RUN);


    create_task_result = xTaskCreatePinnedToCore(task_acc_read_fft, "Task Read ACC", 8192, NULL, 20, NULL, 1);
    if (create_task_result != pdPASS) ESP_LOGI("task", "Failed to create task%d", create_task_result);

    vTaskDelay(100 / portTICK_PERIOD_MS);
    print_heap_info();


    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
