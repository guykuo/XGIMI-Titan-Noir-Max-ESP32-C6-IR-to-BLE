#include "CST3530_Touch.h"
#include "esp_log.h"

static const char *TAG = "CST3530_Driver";

CST3530_TOUCH::CST3530_TOUCH(void) {
    width = TOUCH_WIDTH;
    height = TOUCH_HEIGHT;
    rotation = 0;
}

void CST3530_TOUCH::init(void) {
    // 1. CONFIGURE NATIVE HARDWARE PORT PERIPHERALS
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = TOUCH_SDA;
    conf.scl_io_num = TOUCH_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    conf.clk_flags = 0;

    i2c_param_config((i2c_port_t)I2C_NUM, &conf);
    i2c_driver_install((i2c_port_t)I2C_NUM, conf.mode, 0, 0, 0);

    // 2. CONFIGURE RESET CONTROL PIN
    gpio_config_t rst_gpio_config = {
        .pin_bit_mask = (1ULL << TOUCH_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rst_gpio_config);

    reset();
}

void CST3530_TOUCH::reset(void) {
    gpio_set_level((gpio_num_t)TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
}

void CST3530_TOUCH::Set_Rotation(uint8_t r) {
    rotation = r;
}

bool CST3530_TOUCH::Get_Touch(void) {
    uint8_t buf[9] = {0};
    uint8_t touch_cnt = 0;

    // Read the core 9-byte data packet from the CST3530 coordinates register
    esp_err_t err = touch_i2c_read(0xD0070000, buf, 9);
    if (err != ESP_OK) return false;

    // Extract finger press counts and validate bounding limits
    if ((buf[3] & 0x0F) == 0x00 || (buf[8] & 0xF0) == 0x00) {
        touch_i2c_write(0xD00002AB, NULL, 0); // Fire ACK to unlock tracking cache
        return false;
    }

    touch_cnt = buf[3] & 0x0F;
    if (touch_cnt > 5 || touch_cnt == 0) {
        touch_i2c_write(0xD00002AB, NULL, 0);
        return false;
    }

    // Process and latch point 0 coordinates straight out of the byte array alignment layout
    uint16_t rx = (uint16_t)(((buf[7] & 0x0F) << 8) + buf[4]);
    uint16_t ry = (uint16_t)(((buf[7] & 0xF0) << 4) + buf[5]);

    // Mandatory frame execution acknowledge handshake sequence pass
    touch_i2c_write(0xD00002AB, NULL, 0);

    if (rx < TOUCH_WIDTH && ry < TOUCH_HEIGHT) {
        touch.x = rx;
        touch.y = ry;
        return true;
    }

    return false;
}

// ─── PRIVATE I2C MAPPING UTILITIES (FROM WAVESHARE V2 ENGINE SPEC) ───

esp_err_t CST3530_TOUCH::touch_i2c_read(uint32_t reg, uint8_t *data, uint8_t len) {
    uint8_t reg_buf[4];
    reg_buf[0] = (uint8_t)((reg >> 24) & 0xFF);
    reg_buf[1] = (uint8_t)((reg >> 16) & 0xFF);
    reg_buf[2] = (uint8_t)((reg >> 8) & 0xFF);
    reg_buf[3] = (uint8_t)(reg & 0xFF);

    return i2c_master_write_read_device((i2c_port_t)I2C_NUM, TOUCH_ADDR, reg_buf, sizeof(reg_buf), data, len, pdMS_TO_TICKS(1000));
}

esp_err_t CST3530_TOUCH::touch_i2c_write(uint32_t reg, uint8_t *data, uint8_t len) {
    uint8_t tx_buf[20] = {0};
    if (len > 16) return ESP_ERR_INVALID_SIZE;

    tx_buf[0] = (uint8_t)((reg >> 24) & 0xFF);
    tx_buf[1] = (uint8_t)((reg >> 16) & 0xFF);
    tx_buf[2] = (uint8_t)((reg >> 8) & 0xFF);
    tx_buf[3] = (uint8_t)(reg & 0xFF);

    if ((data != NULL) && (len > 0)) {
        memcpy(&tx_buf[4], data, len);
    }

    return i2c_master_write_to_device((i2c_port_t)I2C_NUM, TOUCH_ADDR, tx_buf, 4 + len, pdMS_TO_TICKS(1000));
}
