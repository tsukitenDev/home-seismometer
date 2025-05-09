#include "task_improv.hpp"

#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "sdkconfig.h"

#include "driver/uart.h"

#include "driver/usb_serial_jtag.h"
#include "network/improv_wifi_handler.hpp"

static std::string firmware_name;
static std::string firmware_version;
static std::string sensor_name;
static std::string device_name;
static std::string monitor_url;

static const char *TAG = "improv";

void improv_set_info(std::string improv_firmware_name,
                      std::string improv_firmware_version,
                      std::string improv_sensor_name,
                      std::string improv_device_name,
                      std::string improv_monitor_url) {
    firmware_name = improv_firmware_name;
    firmware_version = improv_firmware_version;
    sensor_name = improv_sensor_name;
    device_name = improv_device_name;
    monitor_url = improv_monitor_url;
}

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

    ESP_LOGI(TAG, "start");
    
    ImprovSerial improv(firmware_name, firmware_version, sensor_name, device_name, std::string("http://") + monitor_url, uart_port_num);
    
    while(1) {
        improv.loop();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}