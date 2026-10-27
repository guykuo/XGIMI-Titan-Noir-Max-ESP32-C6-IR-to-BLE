#pragma once

#include <stdint.h>
#include <stddef.h>
#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "driver/i2c.h" // Swapped to legacy driver to match Waveshare V2 source

#define TOUCH_ADDR 0x58 
#define I2C_NUM    1    // V2 maps exclusively to I2C Port 1

#define TOUCH_WIDTH  240
#define TOUCH_HEIGHT 320

// Official Waveshare V2 Board Pin Specifications
#define TOUCH_SCL  (GPIO_NUM_3)
#define TOUCH_SDA  (GPIO_NUM_1)
#define TOUCH_RST  (GPIO_NUM_2)
#define TOUCH_INT  (GPIO_NUM_4)

class CST3530_TOUCH {
public:
    CST3530_TOUCH(void);
    void init(void);
    void reset(void);
    void Set_Rotation(uint8_t r);
    bool Get_Touch(void);

    struct _touch_dev {
        uint16_t x;
        uint16_t y;
    };
    _touch_dev touch;

private:
    uint16_t width, height;
    uint8_t rotation;
    esp_err_t touch_i2c_read(uint32_t reg, uint8_t *data, uint8_t len);
    esp_err_t touch_i2c_write(uint32_t reg, uint8_t *data, uint8_t len);
};
