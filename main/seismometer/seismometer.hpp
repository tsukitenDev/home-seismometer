#pragma once

#include <cstdio>
#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <esp_timer.h>
#include <time.h>
#include "data_structure.hpp"
#include "shindo_iir_filter.hpp"
#include "../sensor/sensor_base.hpp"


constexpr uint32_t RT_SHINDO_LENGTH = 4096; // 震度計算期間　　RT_SHINDO_LENGTH < LONG_BUF_SIZE
constexpr uint32_t RT_SHINDO_BUF_INIT_VAL = 5;
constexpr uint32_t RT_SHINDO_BUF_OFFSET = 30;


struct seismometer_result{
    int64_t cnt;
    bool is_varid_intensity = false;
    bool is_stabilized = true;
    std::array<float, 3> gal_raw;
    std::array<float, 3> gal_hpf;
    int32_t rt_intensity;
};

class seismometer {
    private:

    static const  uint32_t LONG_BUF_SIZE = 8192;


    std::vector<std::tuple<int64_t, int32_t>> intensity_int10x_history;

    Sensor_ACC& sensor;

    // デジタルフィルタ
    IIRFilter2 hpf_x;
    IIRFilter2 hpf_y;
    IIRFilter2 hpf_z;
    shindo_filter filter_x;
    shindo_filter filter_y;
    shindo_filter filter_z;
    // 震度算出用BIT
    BITree bitree;
    TickType_t xLastTime;
    int64_t start_time;
    int64_t cnt = 0;
    const char *TAG = "SEIS";


    bool is_stabilized = false;
    bool is_start_shindo = false;

    void init();

    public:

    seismometer(Sensor_ACC& accelerometer) :
        intensity_int10x_history(LONG_BUF_SIZE, {0, RT_SHINDO_BUF_INIT_VAL}), 
        sensor(accelerometer),
        hpf_x(hpf0_05_coef()),
        hpf_y(hpf0_05_coef()),
        hpf_z(hpf0_05_coef()),
        bitree(200)
        { 
            init();
        };
    
    seismometer_result read();
};





void seismometer::init(){

    bitree.add(RT_SHINDO_BUF_INIT_VAL+RT_SHINDO_BUF_OFFSET, RT_SHINDO_LENGTH-1);

    xLastTime = xTaskGetTickCount();
}

seismometer_result seismometer::read(){
        int64_t now = esp_timer_get_time();
        std::array<float, 3> gal = sensor.Read_XYZ_gal();

        if(cnt == 0) start_time = now;
        
        std::array<float, 3> gal1;
        std::array<float, 3> gal2;

        gal1[0] = hpf_x.add_sample(gal[0]);
        gal1[1] = hpf_y.add_sample(gal[1]);
        gal1[2] = hpf_z.add_sample(gal[2]);

        int32_t intensity_max30 = -20;

        if(is_start_shindo) {
            gal2[0] = filter_x.add_sample(gal1[0]);
            gal2[1] = filter_y.add_sample(gal1[1]);
            gal2[2] = filter_z.add_sample(gal1[2]);

            // 計測震度に換算
            float gal3 = sqrt(gal2[0] * gal2[0] + gal2[1] * gal2[1] + gal2[2] * gal2[2]);
            float intensity = 2 * log10f(gal3) + 0.94;
            int32_t intensity_int10x = round(intensity * 100) / 10;

            intensity_int10x = std::min(intensity_int10x, (int32_t)100);
            intensity_int10x = std::max(intensity_int10x, (int32_t)-20);
            
            // 計測震度を格納
            intensity_int10x_history[cnt & (intensity_int10x_history.size() - 1)] = {now, intensity_int10x};
            bitree.add(intensity_int10x + RT_SHINDO_BUF_OFFSET, 1);

            // 上位30位を取り出す
            intensity_max30 = bitree.lower_bound(RT_SHINDO_LENGTH - 30) - RT_SHINDO_BUF_OFFSET;

            // BITの中を SHINDO_LENGTH-1 個に保つ
            int32_t before_30_int = std::get<1>(intensity_int10x_history[(cnt - (RT_SHINDO_LENGTH -1)) & (intensity_int10x_history.size() - 1)]);
            bitree.add(before_30_int + RT_SHINDO_BUF_OFFSET, -1);
        }
        else {
            if(cnt % 100 == 0) {
                //ESP_LOGI(TAG, "cnt %lld, calc time: %lld, X: % 7.1fgal  Y: % 7.1fgal  Z: % 7.1fgal", cnt, esp_timer_get_time() - now, gal1[0], gal1[1], gal1[2]);
                if((esp_timer_get_time() - start_time) > 45 * 1000 * 1000) {
                    ESP_LOGI(TAG, "stabilized");
                    is_start_shindo = true;
                    is_stabilized = true;

                }
            }
        }

    seismometer_result res;
    res.cnt = cnt;
    res.gal_raw = gal;
    res.gal_hpf = gal1;
    res.is_varid_intensity = is_start_shindo;
    res.is_stabilized = is_stabilized;
    res.rt_intensity = intensity_max30;

    
    cnt++;

    return res;
}