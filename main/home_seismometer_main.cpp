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
#include "driver/uart.h"


#include "sensor/lsm6dso.hpp"
#include "sensor/adxl355.hpp"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <LGFX_EQIS1_SSD1306.hpp>
#include <LGFX_1732S019_ST7789.hpp>
#include "driver/spi_master.h"


#include "network/network.hpp"

//#include "seismometer/seismometer.hpp"
#include "seismometer/seismometer_void.hpp"
#include "seismometer/shindo_fft_processor.hpp"


#include "network/wlan.hpp"


// ボード設定
#if CONFIG_SEIS_BOARD_355_1732S019
    #define BOARD_355_1732S019 1
#elif CONFIG_SEIS_BOARD_EQIS1
    #define BOARD_EQIS1 1
#endif

static const std::string firmware_version = "v0.1.0";

static constexpr uint32_t LONG_BUF_SIZE = 8192; 


ringBuffer<std::tuple<int64_t, std::array<float, 3>>> raw_acc_buffer(LONG_BUF_SIZE);
ringBuffer<std::tuple<int64_t, std::array<float, 3>>> hpf_acc_buffer(LONG_BUF_SIZE);

ringBuffer<std::tuple<int64_t, float>> shindo_history(LONG_BUF_SIZE);


QueueHandle_t que_bufCount;

static constexpr uint64_t ws_send_period = 20;

static constexpr uint32_t fft_calc_period = 100;

bool is_start_shindo = false;

void task_ws_send_data(void * pvParameters){
    int64_t cnt;
    while(true){
        xQueueReceive(que_bufCount, &cnt, portMAX_DELAY);
        // websocket送信
        ws_send_data("acc_hpf", hpf_acc_buffer.buffer_, cnt, ws_send_period);
        ws_send_data("acc_raw", raw_acc_buffer.buffer_, cnt, ws_send_period);
        if(is_start_shindo){
            ws_send_data("shindo", shindo_history.buffer_, cnt, ws_send_period);
        }
    }
}

#if BOARD_355_1732S019
    // ピン指定
static constexpr gpio_num_t BUILTIN_LED  = GPIO_NUM_21;
static constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_4;
static constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_5;
static constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_6;
static constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_7;
static constexpr gpio_num_t PIN_NUM_INT1 = GPIO_NUM_15;
static constexpr gpio_num_t PIN_NUM_INT2 = GPIO_NUM_16;

static constexpr gpio_num_t PIN_VOLUME  = GPIO_NUM_2;

#elif BOARD_EQIS1

    // ピン指定
    static constexpr gpio_num_t BUILTIN_LED  = GPIO_NUM_21;
    static constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_8;
    static constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_9;
    static constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_4;
    static constexpr gpio_num_t PIN_NUM_INT1 = GPIO_NUM_3;
    static constexpr gpio_num_t PIN_NUM_INT2 = GPIO_NUM_44;
    static constexpr gpio_num_t PIN_BUTTON1  = GPIO_NUM_43;
    static constexpr gpio_num_t PIN_BUTTON2  = GPIO_NUM_2;

#endif

// LovyanGFX
#if BOARD_355_1732S019
static LGFX_1732S019_ST7789 lcd;
static int32_t shindo_threshold = 5;
static std::string firmware_name = "home-seismometer";
static std::string device_name = "CYD";
static std::string sensor_name = "ADXL355";
std::string mdns_hostname =  "adxl";
std::string mdns_instancename =  "CYD ESP32 Webserver";
static std::string monitor_url = "adxl.local/monitor";

#elif BOARD_EQIS1
static LGFX_EQIS1_SSD1306 lcd;
static int32_t shindo_threshold = 15;
static std::string firmware_name = "home-seismometer";
static std::string device_name = "EQIS-1";
static std::string sensor_name = "LSM6DSO";
std::string mdns_hostname =  "eqis-1";
std::string mdns_instancename =  "EQIS-1 ESP32 Webserver";
static std::string monitor_url = "eqis-1.local/monitor";


#endif


static LGFX_Sprite canvas(&lcd);



#if BOARD_355_1732S019
uint8_t brightness = 255;

static QueueHandle_t gpio_evt_queue = NULL;
/*
static void IRAM_ATTR button_pushed(void* args)
{  
    // 割り込み内では簡単な処理のみ
    uint32_t gpio_num = PIN_VOLUME;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if(io_num == PIN_VOLUME){
                if(brightness==255) brightness = 0;
                else brightness += 51;
                lcd.setBrightness(brightness);
                ESP_LOGI("lcd", "brightness %d", brightness);
            }
            vTaskDelay(80/portTICK_PERIOD_MS); // チャタリング防止
            xQueueReset(gpio_evt_queue);
        }
    }
}
*/
#endif

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


#include "driver/usb_serial_jtag.h"
#include "network/improv_wifi_handler.hpp"

void task_improv(void * pvParameters){
    uart_port_t uart_port_num = UART_NUM_0;
    #if CONFIG_ESP_CONSOLE_UART_DEFAULT
    QueueHandle_t queue_uart;
    uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
    };
    const int buf_size = (1024*2);
    uart_driver_install(uart_port_num, buf_size, buf_size, 20, &queue_uart, 0);
    uart_param_config(uart_port_num, &uart_config);
    uart_set_pin(uart_port_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    uart_flush(uart_port_num);
    #elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 1024
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    vTaskDelay(200 / portTICK_PERIOD_MS);
    #endif

    ESP_LOGI("improv", "start");
    ImprovSerial improv(firmware_name, firmware_version, sensor_name, device_name, std::string("http://") + monitor_url, uart_port_num);
    while(1) {
        improv.loop();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}





EventGroupHandle_t s_shindo_fft_event_group;

#define BIT_TASK_PROCESS_SHINDO_READY BIT0
#define BIT_TASK_PROCESS_SHINDO_RUN BIT1
#define BIT_TASK_PROCESS_SHINDO_RUNNING BIT2


QueueHandle_t que_process_shindo;
QueueHandle_t que_shindo_res;


void task_process_shindo_fft(void * pvParameters){

    const int32_t FFT_CALC_LEN = 4096;
    shindo_processor processor(FFT_CALC_LEN, hpf_acc_buffer);

    ESP_LOGI("calc-shindo-fft", "ready");
    xEventGroupSetBits(s_shindo_fft_event_group, BIT_TASK_PROCESS_SHINDO_READY);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    xEventGroupWaitBits(s_shindo_fft_event_group, BIT_TASK_PROCESS_SHINDO_RUN, pdFALSE, pdFALSE, portMAX_DELAY);
    xEventGroupSetBits(s_shindo_fft_event_group, BIT_TASK_PROCESS_SHINDO_RUNNING);

    ESP_LOGI("calc-shindo-fft", "start");
    int64_t cnt;

    while(true){
        BaseType_t result = xQueueReceive(que_process_shindo, &cnt, portMAX_DELAY);
        if(result != pdPASS) continue;

        int64_t st_a = esp_timer_get_time();

        int32_t res = processor.calc(cnt);
        
        int64_t st_b = esp_timer_get_time();
        //ESP_LOGW("shindo", "complete 3 FFT in %'lldus", st_b - st_a);
        
        xQueueSend(que_shindo_res, &res, 0);
    }
}


bool is_sensor_connected = false;
bool is_sensor_connect_failed = false;

bool is_shindo_stabilized = false;
int32_t min_shindo = 100;
int64_t last_cnt = 0;

void task_acc_read_fft(void * pvParameters){
    const char *TAG = "acc";

    // 加速度センサー設定
    #if BOARD_355_1732S019
    ADXL355 sensor;
    is_sensor_connected = sensor.init(SPI2_HOST, PIN_NUM_CS);
    
    #elif BOARD_EQIS1
    LSM6DSO sensor;
    is_sensor_connected = sensor.init(SPI2_HOST, PIN_NUM_CS);
    
    #endif

    is_sensor_connect_failed = !is_sensor_connected;

    // 初期化
    seismometer_void processer(sensor);

    int64_t start_time = esp_timer_get_time();
    TickType_t xLastTime;
    xLastTime = xTaskGetTickCount();

    int64_t cnt;

    int32_t intensity_int10x = -20;

    int64_t last_read = esp_timer_get_time();

    

    ESP_LOGI(TAG, "START");

    while(1){
        int64_t now = esp_timer_get_time();

        auto res = processer.read();
        cnt = res.cnt;
        
        // 履歴保存
        raw_acc_buffer[cnt] = {now, res.gal_raw};
        hpf_acc_buffer[cnt] = {now, res.gal_hpf}; 

        int32_t fft_res_intensity;
        BaseType_t shindo_fft_res = xQueueReceive(que_shindo_res, &fft_res_intensity, 0);
        if(shindo_fft_res == pdPASS){
            is_start_shindo = true;
            intensity_int10x = fft_res_intensity;
            if(intensity_int10x < min_shindo) min_shindo = intensity_int10x;
            if(min_shindo < 20) is_shindo_stabilized = true;
        }
        if(is_start_shindo) shindo_history[cnt] = {now, intensity_int10x};  

        
        if(is_start_shindo){
            if(cnt % fft_calc_period == 0){
                //int64_t diff = esp_timer_get_time() - last_read // 10,000us→10ms
                ESP_LOGI(TAG, "shindo: %.1f", (double)intensity_int10x / 10);
            }
            if(cnt % ws_send_period == 0){
                // websocket 送信
                xQueueSend(que_bufCount, &cnt, 0);
            }
        }
        else {
            if(cnt % fft_calc_period == 0) {
                ESP_LOGI(TAG, "%lld, X %6.1fgal, Y %6.1fgal  Z %6.1fgal",
                                cnt,
                                res.gal_hpf[0],
                                res.gal_hpf[1],
                                res.gal_hpf[2]);
                
                
            }
            if(cnt % ws_send_period == 0) {
                // websocket 送信
                xQueueSend(que_bufCount, &cnt, 0);
            }
        }
        if(res.is_stabilized){
            if(cnt % fft_calc_period == 0){
                xQueueSend(que_process_shindo, &cnt, 0);
            }
        }
        last_read = now;
        last_cnt = cnt;
        xTaskDelayUntil(&xLastTime, 10 / portTICK_PERIOD_MS);
    }
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

        if(is_sensor_connected) sensor_status = "Connected";
        else if(is_sensor_connect_failed) sensor_status = "Connect Failed";
        else sensor_status = "";

        if(!is_sensor_connected) seis_status = "";
        else if(is_shindo_stabilized) seis_status = "Stabilized";
        else seis_status = "Stabilizing";


        cnt = last_cnt;
        int intensity_int10x = std::get<1>(shindo_history[cnt]);

        // 最大震度を表示する
        if(display_intensity < intensity_int10x) display_intensity = intensity_int10x;

        if(display_mode == 0) {
            if(is_shindo_stabilized && cnt > 120 * 100) display_mode = 2;
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
            if(is_shindo_stabilized) canvas.printf("shindo %.1f", shindo);    

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
    #if BOARD_355_1732S019
    lcd.setRotation(1);
    #elif BOARD_EQIS1
    lcd.setRotation(2);
    #endif


    BaseType_t create_task_result;



    #if BOARD_355_1732S019
    /*
    // 輝度調整　割り込み設定
    gpio_evt_queue = xQueueCreate(10, 8);
    create_task_result = xTaskCreate(gpio_task, "gpio task", 2048, NULL, 0, NULL);

    gpio_set_direction(PIN_VOLUME, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_VOLUME, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(PIN_VOLUME, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_VOLUME, button_pushed, (void *)PIN_VOLUME);
    */
    #endif

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
