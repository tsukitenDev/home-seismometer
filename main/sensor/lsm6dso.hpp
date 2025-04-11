#pragma once

#include <array>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "sensor_base.hpp"

//namespace LSM6DSO_REG{
    constexpr uint8_t FIFO_CTRL1         = 0x07;
    constexpr uint8_t FIFO_CTRL2         = 0x08;
    constexpr uint8_t FIFO_CTRL3         = 0x09;
    constexpr uint8_t FIFO_CTRL4         = 0x0A;
    constexpr uint8_t COUNTER_BDR_REG1   = 0x0B;
    constexpr uint8_t COUNTER_BDR_REG2   = 0x0C;
    constexpr uint8_t INT1_CTRL          = 0x0D;
    constexpr uint8_t INT2_CTRL          = 0x0E;
    constexpr uint8_t WHO_AM_I           = 0x0F;
    constexpr uint8_t CTRL1_XL           = 0x10;
    constexpr uint8_t CTRL2_G            = 0x11;
    constexpr uint8_t CTRL3_C            = 0x12;
    constexpr uint8_t CTRL7_G            = 0x16;
    constexpr uint8_t CTRL8_XL           = 0x17;
    constexpr uint8_t OUTX_L_G           = 0x22;
    constexpr uint8_t OUTX_L_A           = 0x28;
    constexpr uint8_t FIFO_STATUS1       = 0x3A;
    constexpr uint8_t FIFO_STATUS2       = 0x3B;
    constexpr uint8_t INTERNAL_FREQ_FINE = 0x63;
    constexpr uint8_t X_OFS_USR          = 0x73;
    constexpr uint8_t Y_OFS_USR          = 0x74;
    constexpr uint8_t Z_OFS_USR          = 0x75;
    constexpr uint8_t FIFO_DATA_OUT_TAG  = 0x78;
    constexpr uint8_t FIFO_DATA_OUT_X_L  = 0x79;
//};

namespace LSM6DSO_VAL{
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


class LSM6DSO: public Sensor_ACC {
    public:
        LSM6DSO(void);
        void init(spi_host_device_t host, gpio_num_t pin_cs);
        void IO_Write(const uint8_t RegisterAddr, const uint8_t data) const;
        void IO_Read(const uint8_t RegisterAddr, uint8_t& data) const;
        void IO_Read_LH(const uint8_t RegisterAddr, int16_t* arr, const uint8_t len) const;
        void IO_Read_FIFO(const uint8_t RegisterAddr, uint8_t* arr, const uint8_t len) const;
        std::array<int32_t, 3> Read_XYZ_RAW(void) const override;
        
    private:
        spi_device_handle_t spi;
        
};
