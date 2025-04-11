#include "adxl355.hpp"

#include "esp_log.h"


ADXL355::ADXL355(void){
    // 3.9ug/LSB
    scale_factor = 0.000'003'9; // g/LSB
};

void ADXL355::init(spi_host_device_t host, gpio_num_t pin_cs){
            // spi_device_handleを取得するための設定
            spi_device_interface_config_t devcfg_adxl355 = {
                .command_bits = 8,
                .mode = 0,
                .clock_speed_hz = 5 * 1000 * 1000, //5MHz (Max 10MHz)
                .spics_io_num = pin_cs,
                .flags = SPI_DEVICE_HALFDUPLEX,
                .queue_size = 7,
                .pre_cb = NULL,
                .post_cb = NULL
            };            
            esp_err_t ret = spi_bus_add_device(host, &devcfg_adxl355, &spi);
            ESP_ERROR_CHECK(ret);
            // 接続確認
            uint8_t dat;
            IO_Read(DEVID_AD, dat);
            if(dat == 0xAD){
                ESP_LOGI("ADXL355", "connection OK");
            }else{
                ESP_LOGI("ADXL355", "connection NG");
                ESP_LOGI("ADXL355", "%x", dat);
            }
            IO_Write(FILTER, uint8_t(0b000'0101u)); 
            IO_Write(POWER_CTL, uint8_t(0));
};


void ADXL355::IO_Write(const uint8_t RegisterAddr, uint8_t data) const {
            spi_transaction_t t = {
                .flags = SPI_TRANS_USE_TXDATA,
                .cmd = (uint8_t)((uint16_t)RegisterAddr << 1),
                .length = 8,              // データ部は8bits
                .tx_data = {data}
            };
            ESP_LOGI("ADXL355_IO", "write: 0x%x, %x", RegisterAddr, data);
            //esp_err_t ret = 
            spi_device_polling_transmit(this->spi, &t);
            //assert(ret == ESP_OK);
}
void ADXL355::IO_Read(const uint8_t RegisterAddr, uint8_t& data) const {
            uint8_t cmd = (RegisterAddr << 1) | 1; // 読取はLSBを立てる
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


void ADXL355::IO_Read_LH(const uint8_t RegisterAddr, int16_t* arr, const uint8_t len) const {
            // LHペアになっているレジスタの値を読み込む
            uint8_t cmd = (uint8_t)((uint16_t)RegisterAddr << 1) | 1; // 読取はLSBを立てる
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

void ADXL355::IO_Read_FIFO(const uint8_t RegisterAddr, uint8_t* arr, const uint8_t len) const {
    uint8_t cmd = (uint8_t)((uint16_t)RegisterAddr << 1) | 1; // 読取はLSBを立てる
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

std::array<int32_t, 3> ADXL355::Read_XYZ_RAW(void) const {
        uint8_t raw[9];
        std::array<int32_t, 3> res;

        IO_Read_FIFO(XDATA3, raw, 9);

        for(int i=0; i<3; i++){
            int32_t raw2 = ((uint32_t)raw[i*3+0] << 12) | ((uint32_t)raw[i*3+1] << 4) | ((uint32_t)raw[i*3+2] >> 4);
            if(raw2 & (1 << 19)) raw2 |= 0xFF'F0'00'00;
            res[i] = raw2;
        }
        return res;
}

/*--

std::array<float, 3> Sensor_ACC::Read_XYZ_gal(void) const{
            std::array<int32_t, 3> raw = Read_XYZ_RAW();
            std::array<float, 3> res;
            for(int i=0; i<3; i++) res[i] = raw[i] * scale_factor * 9.80665 * 100;
            return res;
}
*/
