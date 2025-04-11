#include "improv_wifi_aux.hpp"

#include <cstdio>
#include <string>
#include <string.h>

#include "wlan.hpp"

bool improv_is_wifi_connected() {
    return wifi_is_connected();
}

bool improv_connect_to_ap(std::string ssid, std::string password){
    return wifi_connect_to_ap(ssid, password);
}

std::vector<ap_info> improv_wifi_scan(){
    std::vector<ap_record_t> ap_list = wifi_scan();
    std::vector<ap_info> res;
    for(auto& ap: ap_list) {
        res.push_back({.ssid = ap.ssid, .rssi = std::to_string(ap.rssi), .encrypted = (ap.authmode != "WIFI_AUTH_WEP")});
    }
    return res;
}
