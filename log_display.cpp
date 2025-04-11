#include "log_display.hpp"
#include <cstdio>
#include <string>
#include <algorithm>


#define START_DISP_BIT BIT0


static EventGroupHandle_t s_log_disp_event_group;

static LogDisplay* log_display_instance;
static bool log_initialized = false;
static bool display = false;

static void strip_ansi_codes(std::string& str) {
  // ANSIエスケープコードを削除
  size_t ansi_start = 0;
  while ((ansi_start = str.find("\033[", ansi_start)) != std::string::npos) {
      size_t ansi_end = str.find('m', ansi_start);
      if (ansi_end != std::string::npos) {
          str.erase(ansi_start, ansi_end - ansi_start + 1);
      } else {
          str.erase(ansi_start);
          break;
      }
  }
  
  size_t timestamp_start = str.find(" (");
  if (timestamp_start != std::string::npos) {
      size_t timestamp_end = str.find(") ", timestamp_start);
      if (timestamp_end != std::string::npos) {
          // タイムスタンプ部分のみ削除
          str.erase(timestamp_start - 1, timestamp_end - timestamp_start + 3);
      }
  }
}


static esp_log_level_t extract_log_level(const std::string& fmt) {
    if (fmt.find("E (") != std::string::npos) return ESP_LOG_ERROR;
    if (fmt.find("W (") != std::string::npos) return ESP_LOG_WARN;
    if (fmt.find("I (") != std::string::npos) return ESP_LOG_INFO;
    if (fmt.find("D (") != std::string::npos) return ESP_LOG_DEBUG;
    if (fmt.find("V (") != std::string::npos) return ESP_LOG_VERBOSE;
    return ESP_LOG_INFO;
}

static int custom_log_output(const char* format, va_list args) {
    // 標準出力
    int result = vprintf(format, args);
    
    if (!log_initialized) {
        return result;
    }
    
    char log_buffer[MAX_LOG_LENGTH];
    vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    std::string log_msg(log_buffer);
    
    strip_ansi_codes(log_msg);
    
    // 改行を空白に置換
    std::replace(log_msg.begin(), log_msg.end(), '\n', ' ');
    std::replace(log_msg.begin(), log_msg.end(), '\r', ' ');
    
    esp_log_level_t level = extract_log_level(format);
    
    log_display_instance->add_message(log_msg, level);
    
    return result;
}

static void log_display_task(void* pvParameters) {    

    
    // ESP_LOGの出力をリダイレクト
    esp_log_set_vprintf(custom_log_output);

    while (1) {
        EventBits_t bits;
        bits = xEventGroupWaitBits(s_log_disp_event_group,
        START_DISP_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);
        display = true;

        while(1) {
            if(!display) break;
            log_display_instance->refresh();
            vTaskDelay(pdMS_TO_TICKS(100)); // 100msごとに更新
        }
    }
}


LogDisplay::LogDisplay(lgfx::LGFX_Device& lcd, int x, int y, int width, int height, 
    uint32_t bg_color, uint32_t border_color)
    : _x(x), _y(y), 
    _mutex(xSemaphoreCreateMutex()), _SpriteMutex(xSemaphoreCreateMutex()), 
      _bg_color(TFT_BLACK), _border_color(TFT_DARKGREY), _sprite(&lcd) {

    _sprite.createSprite(width, height);

    for (int i = 0; i < 2; i++) {
        _lines[i].message = "";
        _lines[i].level = ESP_LOG_INFO;
    }

    _sprite.createSprite(width, height);
    ESP_LOGI("logdisplay", "constructed");

}


uint16_t LogDisplay::_get_log_level_color(esp_log_level_t level) {
    switch (level) {
        case ESP_LOG_ERROR:   return TFT_RED;
        case ESP_LOG_WARN:    return TFT_YELLOW;
        case ESP_LOG_INFO:    return TFT_GREEN;     // UARTと同じ緑色に変更
        case ESP_LOG_DEBUG:   return TFT_GREEN;
        case ESP_LOG_VERBOSE: return TFT_CYAN;
        default:              return TFT_WHITE;
    }
}

void LogDisplay::add_message(const std::string& message, esp_log_level_t level) {
    
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE("LogDisplay", "Failed to take mutex in add_message");
        return;
    }
  
    // 既存メッセージを1行上にシフト
    _lines[0] = _lines[1];
    _lines[1] = _lines[2];
    _lines[2] = _lines[3];
    
    // 新しいメッセージを2行目に設定
    _lines[3].message = message.substr(0, MAX_LOG_LENGTH - 1);
    _lines[3].level = level;

    // ミューテックスを解放
    xSemaphoreGive(_mutex);
    
    // 表示を更新
    //_update_display();
}

void LogDisplay::_update_display() {    
    // ミューテックスを獲得
    if (xSemaphoreTake(_SpriteMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        xSemaphoreGive(_SpriteMutex);
        return;
    }
    std::vector<log_entry_t> lines;
    for(auto& line: _lines) lines.push_back(line);
    xSemaphoreGive(_mutex);

    // スプライトをクリア
    _sprite.fillSprite(_bg_color);
    
    // スプライトに境界線を描画
    //_sprite.drawRect(0, 0, _width, _height, _border_color);
    
    int y_pos = 2;

    _sprite.setCursor(0, y_pos);
    
    // フォントサイズとテキスト表示位置を設定
    _sprite.setTextSize(1);
    _sprite.setTextWrap(true);
    _sprite.setTextScroll(true);

    
    // 各行を描画
    for (int i = 0; i < 4; i++) {
        if (lines[i].message.empty()) continue;
        // 行数を計算
        _sprite.setTextColor(_get_log_level_color(lines[i].level));
        _sprite.println(lines[i].message.c_str());
        //y_pos += line_height;
    }
    
    // スプライトをディスプレイに転送
    _sprite.pushSprite(_x, _y);
    
    // ミューテックスを解放
    xSemaphoreGive(_SpriteMutex);
}

void LogDisplay::refresh() {
    _update_display();
}


bool setup_log_display(lgfx::LGFX_Device& lcd, int x, int y, int width, int height) {

    if (log_initialized) {
        return true;
    }
    s_log_disp_event_group = xEventGroupCreate();
 
    // 初期化
    log_display_instance = new LogDisplay(lcd, x, y, width, height);
    

    BaseType_t task_created = xTaskCreate(
        log_display_task,
        "log_display",
        2048,
        NULL,
        1,
        NULL
    );
    
    
    log_initialized = true;
    ESP_LOGI("abc", "initialized");
    
    return true;
}

void push_log_display(){
    log_display_instance->refresh();
}

void start_log_display(){
    xEventGroupSetBits(s_log_disp_event_group, START_DISP_BIT);
}

void stop_log_display(){
    display = false;
}