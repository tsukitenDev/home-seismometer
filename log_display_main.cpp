#include <LovyanGFX.hpp>
#include <LGFX_1732S019_ST7789.hpp>
#include "log_display.hpp"
#include "esp_log.h"

static const char* TAG = "Main";



static LGFX_1732S019_ST7789 lcd;



extern "C" void app_main() {
    lcd.init();
    lcd.setRotation(0);
    lcd.setBrightness(128);
    lcd.fillScreen(TFT_NAVY);
    
    setup_log_display(lcd, 0, lcd.height() - 80, lcd.width(), 40);
    
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.setCursor(10, 10);
    lcd.println("Main Application");
    ESP_LOGI(TAG, "Application started");
    ESP_LOGW(TAG, "This is a warning message");
    
    int counter = 0;
    while (1) {
        lcd.fillRect(10, 50, lcd.width()-10, 30, TFT_NAVY);
        lcd.setCursor(10, 50);
        lcd.printf("Count: %d", counter);
        
        if (counter % 10 == 0) {
            ESP_LOGI(TAG, "Count: %d", counter);
        }
        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
