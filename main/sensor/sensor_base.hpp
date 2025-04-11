#pragma once

#include <cstdio>
#include <array>

class Sensor_ACC {
    protected:
        float scale_factor; // g / LSB
    public:
        Sensor_ACC(void) : scale_factor(0.0) {};
        virtual std::array<int32_t, 3> Read_XYZ_RAW(void) const = 0;
        std::array<float, 3> Read_XYZ_gal(void) const {
            std::array<int32_t, 3> raw = Read_XYZ_RAW();
            std::array<float, 3> res;
            // raw -> g -> m/s2 -> gal (cm/s2)
            for(int i=0; i<3; i++) res[i] = (float)raw[i] * scale_factor * 9.80665 * 100;
            return res;
    }
};
