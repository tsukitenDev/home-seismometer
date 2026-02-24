#pragma once

#include "driver/gpio.h"
#include "sdkconfig.h"


#if CONFIG_SEIS_BOARD_355_1732S019
    #define BOARD_355_1732S019 1
    #define SENSOR_ADXL355 1
#elif CONFIG_SEIS_BOARD_LSM6DSO_1732S019
    #define BOARD_LSM6DSO_1732S019 1
    #define SENSOR_LSM6DSO 1
#elif CONFIG_SEIS_BOARD_EQIS1
    #define BOARD_EQIS1 1
    #define SENSOR_LSM6DSO 1
#elif CONFIG_SEIS_BOARD_LSM6DSO_XIAO
    #define BOARD_LSM6DSO_XIAO 1
    #define SENSOR_LSM6DSO 1
#endif


#if BOARD_355_1732S019
    static constexpr gpio_num_t BUILTIN_LED  = GPIO_NUM_21;
    static constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_4;
    static constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_5;
    static constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_6;
    static constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_NUM_INT1 = GPIO_NUM_16;
    static constexpr gpio_num_t PIN_NUM_INT2 = GPIO_NUM_17;
    static constexpr gpio_num_t PIN_VOLUME   = GPIO_NUM_2;

#elif BOARD_LSM6DSO_1732S019
    static constexpr gpio_num_t BUILTIN_LED  = GPIO_NUM_15;
    static constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_4;
    static constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_5;
    static constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_6;
    static constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_NUM_INT1 = GPIO_NUM_16;
    static constexpr gpio_num_t PIN_NUM_INT2 = GPIO_NUM_17;
    static constexpr gpio_num_t PIN_BUTTON1  = GPIO_NUM_18;
    static constexpr gpio_num_t PIN_BUTTON2  = GPIO_NUM_19;

#elif BOARD_EQIS1
    static constexpr gpio_num_t BUILTIN_LED  = GPIO_NUM_21;
    static constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_8;
    static constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_9;
    static constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_4;
    static constexpr gpio_num_t PIN_NUM_INT1 = GPIO_NUM_3;
    static constexpr gpio_num_t PIN_NUM_INT2 = GPIO_NUM_44;
    static constexpr gpio_num_t PIN_BUTTON1  = GPIO_NUM_43;
    static constexpr gpio_num_t PIN_BUTTON2  = GPIO_NUM_2;

#elif BOARD_LSM6DSO_XIAO
    static constexpr gpio_num_t BUILTIN_LED  = GPIO_NUM_21;
    static constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_9;
    static constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_8;
    static constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_44;
    static constexpr gpio_num_t PIN_NUM_INT1 = GPIO_NUM_1;
    static constexpr gpio_num_t PIN_NUM_INT2 = GPIO_NUM_2;
    static constexpr gpio_num_t PIN_BUTTON1  = GPIO_NUM_3;
    static constexpr gpio_num_t PIN_BUTTON2  = GPIO_NUM_34;

#endif 