#pragma once

#include <string>

#include "driver/uart.h"

#include <improv.h>

class ImprovSerial {
    public:
        ImprovSerial(const std::string firmware,
          const std::string version,
          const std::string variant,
          const std::string name,
          const std::string url,
          const uart_port_t uart_num) {
            setup(firmware, version, variant, name, url, uart_num);
        }
        void setup(const std::string firmware,
                   const std::string version,
                   const std::string variant,
                   const std::string name,
                   const std::string url,
                   const uart_port_t uart_num);
        bool loop(bool timeout = false);
        improv::State get_state();
        improv::Command get_command();
      
    protected:
        bool parse_improv_serial_byte_(uint8_t byte);
        bool parse_improv_payload_(improv::ImprovCommand &command);

        void set_state_(improv::State state);
        void set_error_(improv::Error error);
        void send_response_(std::vector<uint8_t> &response);
        void on_wifi_connect_timeout_();

        bool is_connected();
        bool wifi_set_ap(std::string ssid, std::string password);
        

        std::vector<uint8_t> build_rpc_settings_response_(improv::Command command);
        std::vector<uint8_t> build_version_info_();

        //uint8_t read_byte_();
        void write_data_(std::vector<uint8_t> &data);

        int64_t last_read_byte_{0};
        improv::State state_;
        improv::ImprovCommand command_;

        bool connectFailed = false;
    
        std::string firmware_name_;
        std::string firmware_version_;
        std::string hardware_variant_;
        std::string device_name_;

        std::string device_url_;

        uart_port_t uart_num_;

        std::vector<uint8_t> rx_serial_buffer_;
        std::vector<uint8_t> rx_buffer_;
};

