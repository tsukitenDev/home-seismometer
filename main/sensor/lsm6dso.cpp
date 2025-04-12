#include "lsm6dso.hpp"
#include "esp_log.h"

LSM6DSO::LSM6DSO(){
    // 0.061mg/LSB
    scale_factor = 0.000'061; // g/LSB
};

bool LSM6DSO::init(spi_host_device_t host, gpio_num_t pin_cs){
    bool res = false;
    // spi_device_handleを取得するための設定
    spi_device_interface_config_t devcfg_lsm6dso = {
        .command_bits = 8,
        .mode = 0,
        .clock_speed_hz = 5 * 1000 * 1000, //5MHz (Max 10MHz)
        .spics_io_num = pin_cs,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 7,
        .pre_cb = NULL,
        .post_cb = NULL
    };            
    esp_err_t ret = spi_bus_add_device(host, &devcfg_lsm6dso, &spi);
    ESP_ERROR_CHECK(ret);
    // 接続確認
    uint8_t dat;
    IO_Read(WHO_AM_I, dat);
    if(dat == 0b01101100u){
        res = true;
        ESP_LOGI("LSM6DSO", "connection OK");
    }
    IO_Write(CTRL1_XL, uint8_t(0b01000010u)); // CTRL1_XL 104hz, 2g, LPF2_XL 有効
    //IO_Write(LSM6DSO_REG::CTRL1_XL, uint8_t(0b01100010u)); 
    //IO_Read(LSM6DSO_REG::CTRL1_XL, dat);
    //assert(dat == 0b01100010u) 
    IO_Write(CTRL8_XL, 0); // LPF2: ODR / 4
    //IO_Write(LSM6DSO_REG::CTRL8_XL, 0b0100'0000u); // LPF2: ODR / 20
    return res;
};


void LSM6DSO::IO_Write(const uint8_t RegisterAddr, uint8_t data) const {
            spi_transaction_t t = {
                .flags = SPI_TRANS_USE_TXDATA,
                .cmd = RegisterAddr,
                .length = 8,              // データ部は8bits
                .tx_data = {data}
            };
            ESP_LOGI("LSM6DSO_IO", "write: 0x%x, %x", RegisterAddr, data);
            //esp_err_t ret = 
            spi_device_polling_transmit(this->spi, &t);
            //assert(ret == ESP_OK);
}
void LSM6DSO::IO_Read(const uint8_t RegisterAddr, uint8_t& data) const {
            uint8_t cmd = 0x80 | RegisterAddr; // 読取はMSBを立てる
            spi_transaction_t t = {
                .flags = SPI_TRANS_USE_RXDATA,
                .cmd = cmd,
                .rxlength = 8    
            };              // データ部は8bits
            //ESP_LOGI("LSM6DSO", "send: %x", cmd);
            //esp_err_t err = 
            spi_device_polling_transmit(this->spi, &t);
            //assert(err == ESP_OK);
            data = t.rx_data[0];
}


void LSM6DSO::IO_Read_LH(const uint8_t RegisterAddr, int16_t* arr, const uint8_t len) const {
            // LHペアになっているレジスタの値を読み込む
            uint8_t cmd = 0x80 | RegisterAddr; // 読取はMSBを立てる
            spi_transaction_t t = {
                .cmd = cmd,
                .rxlength = size_t(len * 2 * 8),  // bit数
                .rx_buffer = arr
            };
            //ESP_LOGI("LSM6DSO", "send: %x", cmd);
            //esp_err_t err = 
            spi_device_polling_transmit(this->spi, &t);
            //assert(err == ESP_OK);
}

void LSM6DSO::IO_Read_FIFO(const uint8_t RegisterAddr, uint8_t* arr, const uint8_t len) const {
            uint8_t cmd = 0x80 | RegisterAddr; // 読取はMSBを立てる
            spi_transaction_t t = {
                .cmd = cmd,
                .rxlength = size_t(len * 8),  // bit数
                .rx_buffer = arr
            };
            //ESP_LOGI("LSM6DSO", "send: %x", cmd);
            //esp_err_t err = 
            spi_device_polling_transmit(this->spi, &t);
            //assert(err == ESP_OK);
}


std::array<int32_t, 3> LSM6DSO::Read_XYZ_RAW(void) const {
    int16_t raw[3];
    std::array<int32_t, 3> res;

    IO_Read_LH(OUTX_L_A, raw, 3);

    res[0] = raw[0];
    res[1] = raw[1];
    res[2] = raw[2];

    return res;
}