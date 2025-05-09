#pragma once

#include <string>


// Wi-Fiが初期化されていること
void task_improv(void * pvParameters);

void improv_set_info(std::string improv_firmware_name,
  std::string improv_firmware_version,
  std::string improv_sensor_name,
  std::string improv_device_name,
  std::string improv_monitor_url);