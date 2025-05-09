#include "task_ws_send.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <array>
#include <tuple>

#include "data_structure.hpp"

#include "network/network.hpp"


extern ringBuffer<std::tuple<int64_t, std::array<float, 3>>> raw_acc_buffer;
extern ringBuffer<std::tuple<int64_t, std::array<float, 3>>> hpf_acc_buffer;

extern ringBuffer<std::tuple<int64_t, float>> shindo_history;

extern bool is_start_shindo;
extern uint32_t ws_send_period;

extern QueueHandle_t que_bufCount;

void task_ws_send_data(void * pvParameters){
  int64_t cnt;
  while(true){
      xQueueReceive(que_bufCount, &cnt, portMAX_DELAY);
      // websocket送信
      ws_send_data("acc_hpf", hpf_acc_buffer.buffer_, cnt, ws_send_period);
      ws_send_data("acc_raw", raw_acc_buffer.buffer_, cnt, ws_send_period);
      if(is_start_shindo){
          ws_send_data("shindo", shindo_history.buffer_, cnt, ws_send_period);
      }
  }
}