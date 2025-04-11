#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <LovyanGFX.hpp>

#include <string>

#define MAX_LOG_LENGTH 64

typedef struct {
    std::string message;
    esp_log_level_t level;
} log_entry_t;


class LogDisplay {

public:
    LogDisplay(lgfx::LGFX_Device& lcd, int x, int y, int width, int height, 
        uint32_t bg_color = TFT_BLACK, uint32_t border_color = TFT_DARKGREY);
    

    bool init();
    void refresh();
    void pushSprite();
    void add_message(const std::string& message, esp_log_level_t level);

private:
    //lgfx::LGFX_Device& _lcd;     
    int _x, _y;  // 表示位置とサイズ
    log_entry_t _lines[4];        // ログ保持用バッファ
    SemaphoreHandle_t _mutex;    
    SemaphoreHandle_t _SpriteMutex;
    //bool _initialized;            // 初期化フラグ
    uint32_t _bg_color;           // 背景色
    uint32_t _border_color;       // 境界線の色
    LGFX_Sprite _sprite;         // スプライト

    
    
    uint16_t _get_log_level_color(esp_log_level_t level);
    
    
    void _update_display();
};


bool setup_log_display(lgfx::LGFX_Device& lcd, int x, int y, int width, int height);


void start_log_display();

void stop_log_display();

void push_log_display();
