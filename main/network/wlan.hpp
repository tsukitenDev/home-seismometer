#pragma once

#include <string>
#include <vector>



struct ap_record_t {
  std::string ssid;
  int8_t  rssi;
  std::string authmode;
};

struct ip_info_t {
  std::string ip;
};

void wifi_init();

void start_mdns_service();


bool wifi_is_connected();
bool wifi_connect_to_ap(std::string ssid, std::string password);

std::vector<ap_record_t> wifi_scan();


enum WIFI_STATE : uint8_t {
  NOT_CONFIGURED = 0x00,
  CONFIGURED     = 0x01,
  CONNECTING     = 0x02,
  CONNECTED       = 0x03,
  NOT_CONNECTED  = 0x04,
  CONNECT_FAILED = 0x05,
};

WIFI_STATE wifi_get_state();

//ap_record_t wifi_get_current_ap();

