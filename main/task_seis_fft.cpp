#include "task_seis_fft.hpp"

#include <tuple>
#include <array>

#include "freertos/FreeRTOS.h"

#include "sensor/lsm6dso.hpp"
#include "sensor/adxl355.hpp"

#include "driver/spi_master.h"

//#include "seismometer/seismometer.hpp"
#include "seismometer/seismometer_void.hpp"
#include "seismometer/shindo_fft_processor.hpp"


#include "task_ws_send.hpp"

#include "board_def.hpp"



extern ringBuffer<std::tuple<int64_t, std::array<float, 3>>> raw_acc_buffer;
extern ringBuffer<std::tuple<int64_t, std::array<float, 3>>> hpf_acc_buffer;

extern ringBuffer<std::tuple<int64_t, float>> shindo_history;



extern EventGroupHandle_t s_shindo_fft_event_group;

extern QueueHandle_t que_bufCount;
extern QueueHandle_t que_process_shindo;
extern QueueHandle_t que_shindo_res;


extern const uint32_t WS_SEND_PERIOD;
extern const uint32_t WS_SEND_OFFSET;
const uint32_t FFT_CALC_PERIOD = 100;

SENSOR_STATE acc_sensor_state = SENSOR_STATE::NOT_CONNECTED;

SEISMOMETER_STATE seis_state = SEISMOMETER_STATE::NOT_STARTED;

extern bool is_start_shindo;

const int32_t shindo_stabilize_thr = 20;

int64_t last_cnt;

const int32_t FFT_CALC_LEN = 4096;



void task_process_shindo_fft(void * pvParameters){

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

        //int64_t st_a = esp_timer_get_time();

        int32_t res = processor.calc(cnt);

        //int64_t st_b = esp_timer_get_time();
        //ESP_LOGW("shindo", "complete 3 FFT in %'lldus", st_b - st_a);

        xQueueSend(que_shindo_res, &res, 0);
    }
}



void task_acc_read_fft(void * pvParameters){
    const char *TAG = "acc";

    // 加速度センサー設定
    #if SENSOR_ADXL355
    ADXL355 sensor;
    bool ret_sensor_init = sensor.init(SPI2_HOST, PIN_NUM_CS);

    #elif SENSOR_LSM6DSO
    LSM6DSO sensor;
    bool ret_sensor_init = sensor.init(SPI2_HOST, PIN_NUM_CS);

    #endif

    if(ret_sensor_init) acc_sensor_state = SENSOR_STATE::CONNECTED;
    else acc_sensor_state = SENSOR_STATE::CONNECT_FAILED;

    // 初期化
    seismometer_void processer(sensor);

    //int64_t start_time = esp_timer_get_time();
    //int64_t last_read = esp_timer_get_time();

    TickType_t xLastTime;
    xLastTime = xTaskGetTickCount();

    int64_t cnt;

    int32_t intensity_int10x = -20;


    seis_state = SEISMOMETER_STATE::HPF_STABILIZING;
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
            if(seis_state == SEISMOMETER_STATE::HPF_STABILIZING) {
              seis_state = SEISMOMETER_STATE::SHINDO_STABILIZING;
              is_start_shindo = true; // task_ws_send用
            }
            intensity_int10x = fft_res_intensity;
            if(seis_state == SEISMOMETER_STATE::SHINDO_STABILIZING &&
                 intensity_int10x < shindo_stabilize_thr) {
              seis_state = SEISMOMETER_STATE::SHINDO_STABILIZED;
            }
        }
        if(seis_state == SEISMOMETER_STATE::SHINDO_STABILIZING ||
           seis_state == SEISMOMETER_STATE::SHINDO_STABILIZED) {
            shindo_history[cnt] = {now, intensity_int10x};
        }

        if(seis_state == SEISMOMETER_STATE::SHINDO_STABILIZING ||
           seis_state == SEISMOMETER_STATE::SHINDO_STABILIZED){
            if(cnt % FFT_CALC_PERIOD == 0){
                //int64_t diff = esp_timer_get_time() - last_read // 10,000us→10ms
                ESP_LOGI(TAG, "shindo: %.1f", (double)intensity_int10x / 10);
            }
            if(cnt % WS_SEND_PERIOD == WS_SEND_OFFSET){
                // websocket 送信
                xQueueSend(que_bufCount, &cnt, 0);
            }
        }
        else {
            if(cnt % FFT_CALC_PERIOD == 0) {
                ESP_LOGI(TAG, "%lld, X %6.1fgal, Y %6.1fgal  Z %6.1fgal",
                                cnt,
                                res.gal_hpf[0],
                                res.gal_hpf[1],
                                res.gal_hpf[2]);
            }
            if(cnt % WS_SEND_PERIOD == WS_SEND_OFFSET) {
                // websocket 送信
                xQueueSend(que_bufCount, &cnt, 0);
            }
        }
        if(res.is_stabilized){
            if(cnt % FFT_CALC_PERIOD == 0){
                xQueueSend(que_process_shindo, &cnt, 0);
            }
        }
        // last_read = now;
        last_cnt = cnt;
        xTaskDelayUntil(&xLastTime, 10 / portTICK_PERIOD_MS);
    }
}