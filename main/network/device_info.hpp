#pragma once

#include <string>

struct device_info_t {
  std::string firmware_name;
  std::string firmware_version;
  std::string device_name;
  std::string sensor_name;
  std::string mdns_hostname;
  std::string mdns_instancename;
  std::string monitor_url;
};

