#pragma once

#include <string>
#include <vector>
#include <tuple>




void httpd_init(void);

void ws_send_data(std::string tag, std::vector<std::tuple<int64_t, std::array<float, 3>>>& data, uint64_t it, uint64_t n);

template<typename T>
void ws_send_data(std::string tag, std::vector<std::tuple<int64_t, T>>& data, uint64_t it, uint64_t n);