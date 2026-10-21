#ifndef _ST77922_TOUCH_H_
#define _ST77922_TOUCH_H_

#include <stdint.h>
#include <stddef.h>
#include "rom/ets_sys.h" // For ets_delay_us
#include "driver/gpio.h" // For ESP-IDF GPIO functions

#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "hal/gpio_ll.h"

#define TOUCH_ADDR 0x55
#define I2C_NUM 0
#define I2C_SPEED 100000

#define TOUCH_WIDTH  320
#define TOUCH_HEIGHT 480
#define MAX_TOUCH_POINTS  10

#define FW_VER     0x0000
#define STATUS     0x0001
#define DEVICE_CON 0x0002
#define MAX_X      0x0005
#define MAX_Y      0x0007
#define MAX_TOUCHES  0x0009
#define TOUCH_INFO   0x0010
#define GESTURE_INFO 0x0012
#define TOUCH_POINT0 0x0014
#define TOUCH_POINT1 0x001B
#define TOUCH_POINT2 0x0022
#define TOUCH_POINT3 0x0029
#define TOUCH_POINT4 0x0030
#define TOUCH_POINT5 0x0037
#define TOUCH_POINT6 0x003E
#define TOUCH_POINT7 0x0045
#define TOUCH_POINT8 0x004C
#define TOUCH_POINT9 0x0053

#define TOUCH_SCL  (GPIO_NUM_39)
#define TOUCH_SDA  (GPIO_NUM_38)
#define TOUCH_RST  48
#define TOUCH_INT  47


//Pin operation
// Fixed: Legacy direct register access swapped for hardware-optimized ESP-IDF driver macros
#define TOUCH_RST_LOW  gpio_set_level((gpio_num_t)TOUCH_RST, 0)
#define TOUCH_RST_HIGH gpio_set_level((gpio_num_t)TOUCH_RST, 1)



class ST77922_TOUCH
{
public:
	ST77922_TOUCH(void);
	void init(void);
	void reset(void);
	void Set_Rotation(uint8_t r);
	void Read_Data(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t* rbuf, size_t rlen);
	bool Get_Touch(void);
	uint8_t max_points;
	struct _touch_dev
	{
		uint8_t id[MAX_TOUCH_POINTS];
		uint16_t x[MAX_TOUCH_POINTS];
		uint16_t y[MAX_TOUCH_POINTS];
	};
	_touch_dev touch;
private:
	uint16_t width, height;
	uint8_t rotation;
};


#endif
