#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "driver/i2c_master.h"

/// 独自の設定を行うクラスを、LGFX_Deviceから派生して作成します。
class LGFX_EQIS1_SSD1306 : public lgfx::LGFX_Device {
  // 接続するパネルの型にあったインスタンスを用意します。
  lgfx::Panel_SSD1306     _panel_instance;

  // パネルを接続するバスの種類にあったインスタンスを用意します。
  lgfx::Bus_I2C       _bus_instance;   // I2Cバスのインスタンス (ESP32のみ)

public:
  // コンストラクタを作成し、ここで各種設定を行います。
  // クラス名を変更した場合はコンストラクタも同じ名前を指定してください。
  LGFX_EQIS1_SSD1306(void) {
    { // バス制御の設定を行います。
      auto cfg = _bus_instance.config();    // バス設定用の構造体を取得します。
      // I2Cバスの設定
      cfg.i2c_port    = I2C_NUM_0;  // 使用するI2Cポートを選択 (0 or 1)
      cfg.freq_write  = 400000;     // 送信時のクロック
      cfg.freq_read   = 400000;     // 受信時のクロック
      cfg.pin_sda     = GPIO_NUM_5; // SDAを接続しているピン番号
      cfg.pin_scl     = GPIO_NUM_6; // SCLを接続しているピン番号
      cfg.i2c_addr    = 0x3C;       // I2Cデバイスのアドレス

      _bus_instance.config(cfg);    // 設定値をバスに反映します。
      _panel_instance.setBus(&_bus_instance);      // バスをパネルにセットします。
    }
    { // 表示パネル制御の設定を行います。
      auto cfg = _panel_instance.config();    // 表示パネル設定用の構造体を取得します。
      cfg.panel_width      =   128;  // 実際に表示可能な幅
      cfg.panel_height     =   64;  // 実際に表示可能な高さ
      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance); // 使用するパネルをセットします。
  }
};
