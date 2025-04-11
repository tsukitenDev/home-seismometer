#pragma once

#include <string>
#include <vector>



struct ap_record_t {
  std::string ssid;
  int8_t  rssi;
  std::string authmode;
};


void wifi_init();

void start_mdns_service();


bool wifi_is_connected();
bool wifi_connect_to_ap(std::string ssid, std::string password);
void wifi_get_status();

std::vector<ap_record_t> wifi_scan();



