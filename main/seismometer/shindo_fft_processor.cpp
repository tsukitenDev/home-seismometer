#include "shindo_fft_processor.hpp"

#include <cstdio>
#include <cmath>
#include <array>

#include <vector>
#include <string>
#include <algorithm>

#include <sstream>
#include <fstream>

#include <ios>
#include <iomanip>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "esp_dsp.h"

#include <sys/stat.h>

#include "data_structure.hpp"

extern ringBuffer<std::tuple<int64_t, std::array<float, 3>>> raw_acc_buffer;
extern ringBuffer<std::tuple<int64_t, std::array<float, 3>>> hpf_acc_buffer;

extern ringBuffer<std::tuple<int64_t, float>> shindo_history;



std::vector<std::string> split(std::string& input, char delimiter)
{
    std::istringstream stream(input);
    std::string field;
    std::vector<std::string> result;
    while (getline(stream, field, delimiter)) {
        result.push_back(field);
    }
    return result;
}


// x, y, z 形式ののcsvをringbufferに展開
void read_csv(){
    const std::string fpath = "/spiflash/MYZH112408081643.csv";
    std::ifstream ifs(fpath);
    std::string line;
    int i = 0;

    if(ifs.fail()){
        ESP_LOGW("read_csv", "file does not exist");
    }

    while(getline(ifs, line)){
        std::vector<std::string> strvec = split(line, ',');
        if(strvec.size() != 3) ESP_LOGW("read_csv", "error");
        if(i < 3){
            ESP_LOGW("example", "%s %s %s", strvec[0].c_str(), strvec[1].c_str(), strvec[2].c_str());
        }
        hpf_acc_buffer[i] = {i, {stof(strvec[0]), stof(strvec[1]), stof(strvec[2])}};
        i++;
    }
    ESP_LOGW("read_csv", "read %d rows", i);
    ifs.close();
}





float jma_filter(float f) {
    if (f <= 0) return 0;
    float fl = sqrtf(1.0f - expf(-powf(f / 0.5f, 3)));

    float y = f / 10.0f;
    float fh = 1.0f / sqrtf(1.0f + 0.694f * y*y + 0.241f * powf(y, 4) +
    0.0557f * powf(y, 6) + 0.009664f * powf(y, 8) +
    0.00134f * powf(y, 10) + 0.000155f * powf(y, 12));
    
    float ff = 1.0f / sqrtf(f);
    
    return fh * fl * ff;
}




void init_jma_filter(float* arr, int32_t len){
    arr[0] = 0;
    arr[len - 1] = 0;
    float dt = 1.0 / 100;
    for(int i=1; i < len/2; i++){
        float freq = (float)i / (len * dt);
        arr[i] = jma_filter(freq);
        arr[len - i - 1] = jma_filter(freq);
    }
}

void init_win_hann(float* arr, int32_t len){
    const float PI = acosf(-1.0f);
    for(int i=0; i<len; i++){
        arr[i] = 0.5 - 0.5 * cosf(2.0f * PI * i / (len-1));
    }
}




void fft(float* arr, int32_t len){
    ESP_ERROR_CHECK(dsps_fft2r_fc32(arr, len));
    ESP_ERROR_CHECK(dsps_bit_rev_fc32(arr, len));
    //ESP_ERROR_CHECK(dsps_cplx2reC_fc32(arr, len));
}

void ifft(float* arr, int32_t len){
    dsps_mulc_f32(&arr[1], &arr[1], len, -1.0f, 2, 2);
    //for(int i=0; i<len; i++){
    //    arr[2*i+1] *= -1;
    //}
    ESP_ERROR_CHECK(dsps_fft2r_fc32(arr, len));
    ESP_ERROR_CHECK(dsps_bit_rev_fc32(arr, len));
    
    dsps_mulc_f32(&arr[0], &arr[0], len, 1.0f/len, 2, 2);
    dsps_mulc_f32(&arr[1], &arr[1], len, -1.0f/len, 2, 2);
    //for(int i=0; i<len; i++){
    //    arr[2*i] /= len;
    //    arr[2*i+1] /= len;
    //    arr[2*i+1] *= -1;
    //}
}





void shindo_processor::init() {
    esp_err_t ret;
    fft2r_table_buff = static_cast<float*>(heap_caps_aligned_alloc(16, FFT_LEN * sizeof(float), MALLOC_CAP_INTERNAL));
    if(fft2r_table_buff == NULL) ESP_LOGW(TAG, "Failed to allocate memory");
    ESP_ERROR_CHECK(dsps_fft2r_init_fc32(fft2r_table_buff, FFT_LEN));

    fft_arr = static_cast<float*>(heap_caps_aligned_alloc(16, 2 * FFT_LEN * sizeof(float), MALLOC_CAP_INTERNAL));
    if(fft_arr == NULL) ESP_LOGW(TAG, "Failed to allocate memory");
    

    pro_sig_X = static_cast<float*>(heap_caps_malloc(FFT_LEN * sizeof(float), MALLOC_CAP_SPIRAM));
    pro_sig_Y = static_cast<float*>(heap_caps_malloc(FFT_LEN * sizeof(float), MALLOC_CAP_SPIRAM));
    pro_sig_Z = static_cast<float*>(heap_caps_malloc(FFT_LEN * sizeof(float), MALLOC_CAP_SPIRAM));
    pro_sig_synth.assign(FFT_LEN, 0);

    if(pro_sig_X == NULL) ESP_LOGW(TAG, "Failed to allocate memory");
    if(pro_sig_Y == NULL) ESP_LOGW(TAG, "Failed to allocate memory");
    if(pro_sig_Z == NULL) ESP_LOGW(TAG, "Failed to allocate memory");
    //if(pro_sig_synth == NULL) ESP_LOGW(TAG, "Failed to allocate memory");
    
    win_hann = static_cast<float*>(heap_caps_aligned_alloc(16, win_len * sizeof(float), MALLOC_CAP_SPIRAM));
    if(win_hann == NULL) ESP_LOGW(TAG, "Failed to allocate memory");

    init_win_hann(win_hann, 200);

    jma_spectrum_filter = static_cast<float*>(heap_caps_aligned_alloc(16, FFT_LEN * sizeof(float), MALLOC_CAP_SPIRAM));
    if(jma_spectrum_filter == NULL) ESP_LOGW(TAG, "Failed to allocate memory");

    init_jma_filter(jma_spectrum_filter, FFT_LEN);

    ESP_LOGI(TAG, "init done");
}

int32_t shindo_processor::calc(int64_t cnt){

    for(int i=0; i<FFT_LEN; i++) pro_sig_synth[i] = 0;

    // X
    for(int i=0; i<FFT_LEN; i++){
        fft_arr[2*i] = std::get<1>(hpf_arr[cnt-(FFT_LEN-1)+i])[0];
        fft_arr[2*i+1] = 0;
    }
    
    process_one();
    
    for(int i=0; i<FFT_LEN; i++){
        pro_sig_X[i] = fft_arr[2*i];
    }
    // 2乗してpro_sig_synthに直接加算
    for(int i=0; i<FFT_LEN; i++){ pro_sig_synth[i] += fft_arr[2*i] * fft_arr[2*i]; }


    // Y
    for(int i=0; i<FFT_LEN; i++){
        fft_arr[2*i] = std::get<1>(hpf_arr[cnt-(FFT_LEN-1)+i])[1];
        fft_arr[2*i+1] = 0;
    }

    process_one();
    
    for(int i=0; i<FFT_LEN; i++){
        pro_sig_Y[i] = fft_arr[2*i];
    }
    // 2乗してpro_sig_synthに直接加算
    for(int i=0; i<FFT_LEN; i++){ pro_sig_synth[i] += fft_arr[2*i] * fft_arr[2*i]; }

    // Z
    for(int i=0; i<FFT_LEN; i++){
        fft_arr[2*i] = std::get<1>(hpf_arr[cnt-(FFT_LEN-1)+i])[2];
        fft_arr[2*i+1] = 0;
    }

    process_one();
    
    for(int i=0; i<FFT_LEN; i++){
        pro_sig_Z[i] = fft_arr[2*i];
    }
    // 2乗してpro_sig_synthに直接加算
    for(int i=0; i<FFT_LEN; i++){ pro_sig_synth[i] += fft_arr[2*i] * fft_arr[2*i]; }
    
    
    
    for(int i=0; i<FFT_LEN; i++){ pro_sig_synth[i] = sqrtf(pro_sig_synth[i]);}


    // 震度計算
    std::sort(pro_sig_synth.begin(), pro_sig_synth.end(), std::greater<float>());
    float max30_gal = pro_sig_synth[29];
    last_intensity = 2 * log10f(max30_gal) + 0.94;
    int32_t intensity_int10x = round(last_intensity * 100) / 10;
    return intensity_int10x;
}






void shindo_processor::process_one(){
    int64_t start_a = esp_timer_get_time();
    // 端に窓適用
    for(int i=0; i<win_len/2; i++){
        fft_arr[i*2] = fft_arr[i*2] * win_hann[i];
        fft_arr[(FFT_LEN-1-i)*2] = fft_arr[(FFT_LEN-1-i)*2] * win_hann[win_len-1-i];
    }

    int64_t start_b = esp_timer_get_time();
    fft(fft_arr, FFT_LEN);
    //ESP_LOGI("FFT", "%6.1f   %6.1f", fft_arr[0], fft_arr[1]);
    //ESP_LOGI("FFT", "%6.1f   %6.1f", fft_arr[2], fft_arr[3]);

    dsps_mul_f32(&fft_arr[0], &jma_spectrum_filter[0], &fft_arr[0], FFT_LEN, 2, 1, 2);
    dsps_mul_f32(&fft_arr[1], &jma_spectrum_filter[0], &fft_arr[1], FFT_LEN, 2, 1, 2);

    ifft(fft_arr, FFT_LEN);
    int64_t start_d = esp_timer_get_time();
    //ESP_LOGI("FFT", "total %lldus, fft %lldus", start_d - start_a, start_d - start_b);
    return;
}


EventGroupHandle_t s_seis_test_event_group;

void task_calc_shindo_fft_test(void * pvParameters){

    const int32_t FFT_CALC_LEN = 4096;
    shindo_processor processor(FFT_CALC_LEN, hpf_acc_buffer);

    ESP_LOGI("calc-shindo-fft", "ready");
    xEventGroupSetBits(s_seis_test_event_group, BIT_TASK_CALC_SHINDO_FFT_TEST_READY);
    xEventGroupWaitBits(s_seis_test_event_group, BIT_TASK_CALC_SHINDO_FFT_TEST_RUN, pdFALSE, pdFALSE, portMAX_DELAY);
    xEventGroupSetBits(s_seis_test_event_group, BIT_TASK_CALC_SHINDO_FFT_TEST_RUNNING);

    ESP_LOGI("calc-shindo-fft", "start");

    read_csv();

    /*
    for(int64_t i=0; i<100; i++){
        ESP_LOGI("data", "%lld, X %6.3fgal, Y %6.3fgal  Z %6.3fgal",
                        i,
                        
                        std::get<1>(hpf_acc_buffer[FFT_CALC_LEN-1-(FFT_CALC_LEN-1)+i])[0],
                        std::get<1>(hpf_acc_buffer[FFT_CALC_LEN-1-(FFT_CALC_LEN-1)+i])[1],
                        std::get<1>(hpf_acc_buffer[FFT_CALC_LEN-1-(FFT_CALC_LEN-1)+i])[2]);
    }
    */

    while(true){
        int64_t st_a = esp_timer_get_time();
        float res = processor.calc(FFT_CALC_LEN-1);
        ESP_LOGI("shindo", "%.2f", res);
        int64_t st_b = esp_timer_get_time();
        ESP_LOGW("shindo", "complete 3 FFT in %'lldus", st_b - st_a);


        ESP_LOGI("fs", "writing a csv file...");
        struct stat st;
        if (stat("/spiflash/data", &st) != 0) {
            ESP_LOGI("folder", "Creating data directory");
            mkdir("/spiflash/data", 0755);
        }

        std::ifstream ifs("/spiflash/data/output.csv");
        std::ofstream ofs;
        if(ifs.fail()){
            ofs.open("/spiflash/data/output.csv");
            // ヘッダー行を追加
            ofs << "i,x0,x,y0,y,z0,z,synth\n";
        }else{
            ifs.close();
            ofs.open("/spiflash/data/output.csv", std::ios_base::app);
        }
        
        for(int64_t i=0; i<FFT_CALC_LEN; i++){
            ofs << i << ","  // UNIX時刻z
                << std::get<1>(hpf_acc_buffer[FFT_CALC_LEN-1-(FFT_CALC_LEN-1)+i])[0] << ","
                << processor.pro_sig_X[i] << ","  // x
                << std::get<1>(hpf_acc_buffer[FFT_CALC_LEN-1-(FFT_CALC_LEN-1)+i])[1] << ","
                << processor.pro_sig_Y[i] << ","  // y
                << std::get<1>(hpf_acc_buffer[FFT_CALC_LEN-1-(FFT_CALC_LEN-1)+i])[2] << ","
                << processor.pro_sig_Z[i] << "," // z
                << processor.pro_sig_synth[i] << "\n";
        }
        ESP_LOGI("fs", "Data saved to %s", "/spiflash/data/output.csv");

        for(int64_t i=0; i<100; i++){
            ESP_LOGI("result", "%lld, X %6.1fgal, Y %6.1fgal  Z %6.1fgal",
                            i,
                            
                            processor.pro_sig_X[i],
                            processor.pro_sig_Y[i],
                            processor.pro_sig_Z[i]);
        }
        ofs.close();
    

        break;
    }
    while(true){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
