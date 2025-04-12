#pragma once

#include <array>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "sensor_base.hpp"

//namespace LSM6DSO_REG{
    constexpr uint8_t DEVID_AD           = 0x00;
    constexpr uint8_t DEVID_MST          = 0x01;
    constexpr uint8_t XDATA3             = 0x08;
    constexpr uint8_t YDATA3             = 0x0B;
    constexpr uint8_t ZDATA3             = 0x0E;
    constexpr uint8_t FILTER             = 0x28;
    constexpr uint8_t RANGE              = 0x2C;
    constexpr uint8_t POWER_CTL          = 0x2D;


//};

namespace ADXL355_VAL{
    // INT1_CTRL INT2_CTRL
    constexpr uint8_t INT_FIFO_FULL     = 0b0010'0000;
    constexpr uint8_t INT_FIFO_OVR      = 0b0001'0000;
    constexpr uint8_t INT_FIFO_TH       = 0b0000'1000;
    constexpr uint8_t INT_DRDY_XL       = 0b0000'0001;
    
    // CTRL1_XL
    constexpr uint8_t ODR_XL_104        = 0b0100'0000;
    constexpr uint8_t ODR_XL_208        = 0b0101'0000;
    constexpr uint8_t ODR_XL_416        = 0b0110'0000;
    constexpr uint8_t ODR_XL_833        = 0b0111'0000;
    constexpr uint8_t LPF2_XL_EN        = 0b0000'0010;

    // CTRL7_G
    constexpr uint8_t XL_USR_OFF_EN     = 0b0000'0010; // Enable accelerometer user offset correction block

    // CTRL8_XL
    constexpr uint8_t HPCF_XL_div_4     = 0b000 << 5;
    constexpr uint8_t HPCF_XL_div_10    = 0b001 << 5;
    constexpr uint8_t HPCF_XL_div_20    = 0b010 << 5;
    constexpr uint8_t HPCF_XL_div_45    = 0b011 << 5;
    
}




class ADXL355: public Sensor_ACC {
    public:
        ADXL355(void);
        bool init(spi_host_device_t host, gpio_num_t pin_cs);
        void IO_Write(const uint8_t RegisterAddr, const uint8_t data) const;
        void IO_Read(const uint8_t RegisterAddr, uint8_t& data) const;
        void IO_Read_LH(const uint8_t RegisterAddr, int16_t* arr, const uint8_t len) const;
        void IO_Read_FIFO(const uint8_t RegisterAddr, uint8_t* arr, const uint8_t len) const;
        std::array<int32_t, 3> Read_XYZ_RAW(void) const override;
        
    private:
        spi_device_handle_t spi;
        
};