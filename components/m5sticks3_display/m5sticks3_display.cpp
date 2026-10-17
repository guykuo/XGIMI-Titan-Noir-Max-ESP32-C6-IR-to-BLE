#include "m5sticks3_display.h"
#include "esphome/core/log.h"

namespace esphome {
namespace m5sticks3_display {

static const char *const TAG = "m5sticks3_display";

void M5StickS3Display::setup() {
  auto cfg = M5.config();

//  cfg.serial_baudrate = 115200;
  cfg.clear_display = false;
  cfg.output_power = true;

  M5.begin(cfg);
  delay(150);

  M5.Display.begin();
  delay(120);

  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);

  int w = M5.Display.width();
  int h = M5.Display.height();

  // Full framebuffer sprite for flicker-free updates
  if (!canvas_.createSprite(w, h)) {
    ESP_LOGE(TAG, "Failed to allocate canvas sprite %dx%d", w, h);
    return;
  }

  canvas_.setColorDepth(16);
  canvas_.fillScreen(TFT_BLACK);
  canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(4, 4);
  canvas_.print("M5StickS3 M5Unified");

  canvas_.pushSprite(0, 0);

  initialized_ = true;
  ESP_LOGI(TAG, "M5Unified display initialized: %d x %d", w, h);
}

void M5StickS3Display::dump_config() {
  ESP_LOGCONFIG(TAG, "M5StickS3 Display (M5Unified/M5GFX)");
  LOG_UPDATE_INTERVAL(this);

  if (initialized_) {
    ESP_LOGCONFIG(TAG, "  Width: %d",  (int) M5.Display.width());
    ESP_LOGCONFIG(TAG, "  Height: %d", (int) M5.Display.height());
  } else {
    ESP_LOGCONFIG(TAG, "  Not initialized");
  }
}

int M5StickS3Display::get_width_internal() {
  return initialized_ ? M5.Display.width() : 135;
}

int M5StickS3Display::get_height_internal() {
  return initialized_ ? M5.Display.height() : 240;
}

void M5StickS3Display::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (!initialized_) return;
  uint16_t c = canvas_.color565(color.r, color.g, color.b);
  canvas_.drawPixel(x, y, c);
}

void M5StickS3Display::update() {
  if (!initialized_) return;

  // Let ESPHome render into the off-screen canvas
  this->do_update_();

  // Push finished frame all at once
  canvas_.pushSprite(0, 0);
}

}  // namespace m5sticks3_display
}  // namespace esphome
