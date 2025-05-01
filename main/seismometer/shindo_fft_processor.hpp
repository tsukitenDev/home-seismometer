#pragma once

#include <array>
#include <vector>
#include <tuple>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_system.h"

#include "data_structure.hpp"


class shindo_processor {
    public:
        shindo_processor(const int32_t len, ringBuffer<std::tuple<int64_t, std::array<float, 3>>>& hpf_arr) : LEN_FFT(len), hpf_arr(hpf_arr) { init(); };
        int32_t calc(const int64_t cnt, bool apply_win = true);

        float* pro_sig_X;
        float* pro_sig_Y;
        float* pro_sig_Z;
        std::vector<float> pro_sig_synth;

    private:
        const char* TAG = "shindo_processor";
        const int32_t LEN_FFT;
        float* fft2r_table_buff;
        float* fft_arr;

        const int32_t LEN_WIN_HANN = 200;
        float* win_hann;
        float* jma_spectrum_filter;

        float last_intensity;

        ringBuffer<std::tuple<int64_t, std::array<float, 3>>>& hpf_arr;

        void init();
        void process_one(bool apply_win = true);

};



void task_calc_shindo_fft_test(void * pvParameters);
