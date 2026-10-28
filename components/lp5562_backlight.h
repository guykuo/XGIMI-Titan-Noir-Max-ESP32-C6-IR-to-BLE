#pragma once
#include "esphome.h"

class LP5562Backlight : public esphome::Component, public esphome::FloatOutput {
 public:
  void setup() override {
    // 1. Wake and enable the LP5562 chip (Register 0x00 -> Value 0x40)
    Wire.beginTransmission(0x30);
    Wire.write(0x00);
    Wire.write(0x40);
    Wire.endTransmission();

    delay(10); // Short settling delay

    // 2. Configure the White channel to standard PWM direct execution mode (Register 0x08 -> Value 0x01)
    Wire.beginTransmission(0x30);
    Wire.write(0x08);
    Wire.write(0x01);
    Wire.endTransmission();
  }

  void write_state(float state) override {
    // 3. Map float state boundaries (0.0 to 1.0) into raw 8-bit integer data bytes (0 to 255)
    uint8_t pwm_byte = (uint8_t)(state * 255.0f);

    // 4. Pass the scaled brightness byte down to the physical White LED PWM register slot (0x0E)
    Wire.beginTransmission(0x30);
    Wire.write(0x0E);
    Wire.write(pwm_byte);
    Wire.endTransmission();
  }
};
