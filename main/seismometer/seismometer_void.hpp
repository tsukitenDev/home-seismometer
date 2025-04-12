#pragma once

#include <cstdio>
#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <esp_timer.h>
#include <time.h>
#include "data_structure.hpp"
#include "../sensor/sensor_base.hpp"


#include "iir.hpp"


struct seismometer_void_result{
    int64_t cnt;
    bool is_stabilized = false;
    std::array<float, 3> gal_raw;
    std::array<float, 3> gal_hpf;
};

class seismometer_void {
    private:

    Sensor_ACC& sensor;

    // デジタルフィルタ
    IIRFilter2 hpf_x;
    IIRFilter2 hpf_y;
    IIRFilter2 hpf_z;

    TickType_t xLastTime;
    int64_t start_time;
    int64_t cnt = 0;
    const char *TAG = "SEIS";


    bool is_stabilized = false;

    void init();

    public:

    seismometer_void(Sensor_ACC& accelerometer) :
        sensor(accelerometer),
        hpf_x(hpf0_05_coef()),
        hpf_y(hpf0_05_coef()),
        hpf_z(hpf0_05_coef())
        { 
            init();
        };
    
    seismometer_void_result read();
};





void seismometer_void::init(){

    xLastTime = xTaskGetTickCount();
}

seismometer_void_result seismometer_void::read(){
        int64_t now = esp_timer_get_time();
        std::array<float, 3> gal = sensor.Read_XYZ_gal();

        if(cnt == 0) start_time = now;
        
        std::array<float, 3> gal1;

        gal1[0] = hpf_x.add_sample(gal[0]);
        gal1[1] = hpf_y.add_sample(gal[1]);
        gal1[2] = hpf_z.add_sample(gal[2]);


        if(is_stabilized) {

        }
        else {
            if(cnt % 100 == 0) {
                //ESP_LOGI(TAG, "cnt %lld, calc time: %lld, X: % 7.1fgal  Y: % 7.1fgal  Z: % 7.1fgal", cnt, esp_timer_get_time() - now, gal1[0], gal1[1], gal1[2]);
                if((esp_timer_get_time() - start_time) > 45 * 1000 * 1000) {
                    ESP_LOGI(TAG, "stabilized");
                    is_stabilized = true;

                }
            }
        }

    seismometer_void_result res;
    res.cnt = cnt;
    res.gal_raw = gal;
    res.gal_hpf = gal1;
    res.is_stabilized = is_stabilized;

    
    cnt++;

    return res;
}