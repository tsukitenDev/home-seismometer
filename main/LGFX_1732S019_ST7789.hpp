#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "driver/spi_master.h"

class LGFX_1732S019_ST7789 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;
public:
  LGFX_1732S019_ST7789(void) {
    {
      auto cfg = _bus_instance.config();

      // SPIバスの設定
      cfg.spi_host = SPI3_HOST;  // 変更
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 12;  // 変更
      cfg.pin_mosi = 13;  // 変更
      cfg.pin_miso = -1;  // 変更
      cfg.pin_dc = 11;    // 変更

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs = 10;    // 変更
      cfg.pin_rst = 1;    // 変更
      cfg.pin_busy = -1;  // 変更

      cfg.panel_width = 170;  // 変更
      cfg.panel_height = 320;
      cfg.offset_x = 35;  // 変更
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;  // 変更

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();

      cfg.pin_bl = 14;  // 変更
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};