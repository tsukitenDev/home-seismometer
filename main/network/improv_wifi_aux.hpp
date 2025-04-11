#pragma once

#include <vector>
#include <string>


struct ap_info {
    std::string ssid;
    std::string rssi;
    bool encrypted;
};
  

bool improv_is_wifi_connected();

bool improv_connect_to_ap(std::string ssid, std::string password);

std::vector<ap_info> improv_wifi_scan();