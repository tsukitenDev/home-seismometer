#include "improv_wifi_handler.hpp"

#include <vector>
#include <string>
#include <set>

#include "esp_timer.h"

#include "improv_wifi_aux.hpp"

#include "driver/usb_serial_jtag.h"
#include "esp_vfs_usb_serial_jtag.h"

void ImprovSerial::setup(const std::string firmware,
                         const std::string version,
                         const std::string variant,
                         const std::string name,
                         const std::string url,
                         const uart_port_t uart_num){
    this->firmware_name_    = firmware;
    this->firmware_version_ = version;
    this->hardware_variant_ = variant;
    this->device_name_      = name;

    this->device_url_       = url;                        
  
    this->uart_num_ = uart_num;

    this->state_ = improv::State(improv::STATE_AUTHORIZED);
    this->command_ = improv::ImprovCommand({improv::Command::UNKNOWN, "", ""});

    this->rx_serial_buffer_.resize(1024);

}


improv::State ImprovSerial::get_state(){
  return this->state_;
}

improv::Command ImprovSerial::get_command(){
  return this->command_.command;
}

bool ImprovSerial::loop(bool timeout){
  #if CONFIG_ESP_CONSOLE_UART_DEFAULT
  if(true) rx_buffer_.clear();
  int32_t length = 0;
  ESP_ERROR_CHECK(uart_get_buffered_data_len(this->uart_num_, (size_t*)&length));
  while(length > 1){
    uint8_t byte;
    uart_read_bytes(this->uart_num_, &byte, 1, 0);
    if(this->parse_improv_serial_byte_(byte)){
        this->last_read_byte_ = esp_timer_get_time();
    }else{
      this->rx_buffer_.clear();
    }
    ESP_ERROR_CHECK(uart_get_buffered_data_len(this->uart_num_, (size_t*)&length));
  }
  #elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  if(true) rx_buffer_.clear();
  int32_t length = 0;
  int32_t len = usb_serial_jtag_read_bytes(rx_serial_buffer_.data(), 1024 - 1, 0);
  int32_t cursor = 0;
  while(cursor < len) {
    if(this->parse_improv_serial_byte_(rx_serial_buffer_[cursor])){
      this->last_read_byte_ = esp_timer_get_time();
    }else{
      this->rx_buffer_.clear();
    }
    cursor++;
    if(cursor == len) {
      len = usb_serial_jtag_read_bytes(rx_serial_buffer_.data(), 1024 - 1, 20 / portTICK_PERIOD_MS);
      if(len > 0) cursor = 0;
    }
  }

  #endif
  // improv wifi経由の操作の結果、STATE_PROVISIONINGになる
  // onConnected
  if (this->state_ == improv::STATE_PROVISIONING) {
    if (this->is_connected()) {
      this->set_state_(improv::STATE_PROVISIONED);

      std::vector<uint8_t> url = this->build_rpc_settings_response_(improv::WIFI_SETTINGS);
      this->send_response_(url);
      return true;
    }
    else if(this->connectFailed) {
      this->connectFailed = false;
      this->on_wifi_connect_timeout_();
    }
  }
  return false;
}


void ImprovSerial::write_data_(std::vector<uint8_t> &data) {
  data.push_back('\n');
  #if CONFIG_ESP_CONSOLE_UART_DEFAULT
  uart_write_bytes(this->uart_num_, data.data() , data.size());
  #elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  usb_serial_jtag_write_bytes(data.data(), data.size(), 20 / portTICK_PERIOD_MS);
  #endif
}


std::vector<uint8_t> ImprovSerial::build_rpc_settings_response_(improv::Command command) {
  std::vector<std::string> urls;
  urls.push_back(this->device_url_);
  std::vector<uint8_t> data = improv::build_rpc_response(command, urls, false);
  return data;
};

std::vector<uint8_t> ImprovSerial::build_version_info_() {
  std::vector<std::string> infos = {this->firmware_name_, this->firmware_version_, this->hardware_variant_, this->device_name_};
  std::vector<uint8_t> data = improv::build_rpc_response(improv::GET_DEVICE_INFO, infos, false);
  return data;
};


bool ImprovSerial::parse_improv_serial_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  //ESP_LOGI(TAG, "Improv Serial byte: 0x%02X", byte);
  const uint8_t *raw = &this->rx_buffer_[0];
  if (at == 0)
    return byte == 'I';
  if (at == 1)
    return byte == 'M';
  if (at == 2)
    return byte == 'P';
  if (at == 3)
    return byte == 'R';
  if (at == 4)
    return byte == 'O';
  if (at == 5)
    return byte == 'V';

  if (at == 6)
    // improv sdkとは別に実装しているから本来はクラス内で設定するべき？
    return byte == improv::IMPROV_SERIAL_VERSION;

  if (at == 7)
    return true;
  uint8_t type = raw[7];

  if (at == 8)
    return true;
  uint8_t data_len = raw[8];

  if (at < 8 + data_len)
    return true;

  if (at == 8 + data_len)
    return true;

  if (at == 8 + data_len + 1) {
    uint8_t checksum = 0x00;
    for (uint8_t i = 0; i < at; i++)
      checksum += raw[i];

    if (checksum != byte) {
      //ESP_LOGW(TAG, "Error decoding Improv payload");
      this->set_error_(improv::ERROR_INVALID_RPC);
      return false;
    }

    if (type == improv::TYPE_RPC) {
      this->set_error_(improv::ERROR_NONE);
      auto command = improv::parse_improv_data(&raw[9], data_len, false);
      return this->parse_improv_payload_(command);
    }
  }

  // If we got here then the command coming is improv, but not an RPC command
  return false;
}


bool ImprovSerial::parse_improv_payload_(improv::ImprovCommand &command) {
  switch (command.command) {
    case improv::WIFI_SETTINGS: {
      //ESP_LOGI(TAG, "Received Improv wifi settings ssid=%s, password=%s", command.ssid.c_str(), command.password.c_str());
      this->set_state_(improv::STATE_PROVISIONING);
      this->command_.command  = command.command;
      this->command_.ssid     = command.ssid;
      this->command_.password = command.password;
      this->connectFailed = !wifi_set_ap(command.ssid, command.password);
      return true;
    }
    case improv::GET_CURRENT_STATE:
      this->set_state_(this->state_);
      if (this->state_ == improv::STATE_PROVISIONED) {
        std::vector<uint8_t> url = this->build_rpc_settings_response_(improv::GET_CURRENT_STATE);
        this->send_response_(url);
      }
      return true;
    case improv::GET_DEVICE_INFO: {
      std::vector<uint8_t> info = this->build_version_info_();
      this->send_response_(info);
      return true;
    }
    case improv::GET_WIFI_NETWORKS: {
      std::set<std::string> st_ssid;
      std::vector<ap_info> scan_result = improv_wifi_scan();
      for (auto ap: scan_result) {
        if (ap.ssid.empty()) continue;
        // Skip duplicates
        if (st_ssid.contains(ap.ssid)) continue;
        st_ssid.insert(ap.ssid);
        // Send each ssid separately to avoid overflowing the buffer
        std::vector<uint8_t> data = improv::build_rpc_response(improv::GET_WIFI_NETWORKS, {ap.ssid, ap.rssi, (ap.encrypted ? "YES" : "NO")}, false);
        this->send_response_(data);
      }
      // Send empty response to signify the end of the list.
      std::vector<uint8_t> data = improv::build_rpc_response(improv::GET_WIFI_NETWORKS, std::vector<std::string>{}, false);
      this->send_response_(data);
      return true;
    }
    default: {
      //ESP_LOGW(TAG, "Unknown Improv payload");
      this->set_error_(improv::ERROR_UNKNOWN_RPC);
      return false;
    }
  }
}

void ImprovSerial::set_state_(improv::State state) {
  this->state_ = state;

  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(11);
  data[6] = improv::IMPROV_SERIAL_VERSION;
  data[7] = improv::TYPE_CURRENT_STATE;
  data[8] = 1;
  data[9] = state;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[10] = checksum;

  this->write_data_(data);
}

void ImprovSerial::set_error_(improv::Error error) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(11);
  data[6] = improv::IMPROV_SERIAL_VERSION;
  data[7] = improv::TYPE_ERROR_STATE;
  data[8] = 1;
  data[9] = error;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[10] = checksum;
  this->write_data_(data);
}



void ImprovSerial::send_response_(std::vector<uint8_t> &response) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(9);
  data[6] = improv::IMPROV_SERIAL_VERSION;
  data[7] = improv::TYPE_RPC_RESPONSE;
  data[8] = response.size();
  data.insert(data.end(), response.begin(), response.end());

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data.push_back(checksum);

  this->write_data_(data);
}

void ImprovSerial::on_wifi_connect_timeout_() {
  this->set_error_(improv::ERROR_UNABLE_TO_CONNECT);
  this->set_state_(improv::STATE_AUTHORIZED);
  //ESP_LOGW(TAG, "Timed out trying to connect to given WiFi network");
  //WiFi.disconnect();
}


bool ImprovSerial::is_connected(){
  return improv_is_wifi_connected();
}
bool  ImprovSerial::wifi_set_ap(std::string ssid, std::string password){
  return improv_connect_to_ap(ssid, password);
  
}