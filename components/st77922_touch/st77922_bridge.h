#pragma once
#include "esphome/core/component.h"
#include "esphome/components/light/light_state.h"
#include "ST77922_Touch.h" 

#ifndef KEY_EVENT_DOWN
#define KEY_EVENT_DOWN    0
#endif
#ifndef KEY_EVENT_UP
#define KEY_EVENT_UP      2
#endif

void execute_action_pipeline(const std::string& cmd_name, int event_state, uint32_t raw_ir_hex = 0);

#ifndef INPUT_SOURCES_DEFINED
#define INPUT_SOURCES_DEFINED
enum InputSource : uint32_t {
    SOURCE_WEB        = 0x0000, 
    SOURCE_TOUCH_WAKE = 0x9999, 
    SOURCE_TOUCH_PAD  = 0xBBBB, 
};
#endif

namespace st77922_touch {

class ST77922TouchBridge : public esphome::Component {
 public:
  ST77922_TOUCH touch_engine;
  std::string last_action = "";
  uint32_t last_trigger = 0;
  
  bool wake_prevent_latch = false;
  uint32_t wake_timestamp = 0; 
  uint32_t continuous_touch_start = 0;

  // ─── 🟢 NATIVE PHYSICAL DEBOUNCE TRACKING FIELDS ───
  uint32_t touch_down_timestamp = 0;   // Marks touchdown to handle typematic initial delay
  uint32_t last_packet_sent_time = 0;  // Paces streaming hold updates
  bool is_down_stroke_fired = false;   // Strict lock guarantees exactly ONE press edge per tap
  uint32_t soft_debounce_lift_time = 0;// Re-touch cooling window tracker

  void setup() override {
    touch_engine.init();
    touch_engine.Set_Rotation(0);
  }

  void loop() override {
    uint32_t now = esphome::millis();

    if (touch_engine.Get_Touch()) {
      // Soft Safety Watchdog Guard
      if (continuous_touch_start == 0) {
        continuous_touch_start = now; 
      } else if (now - continuous_touch_start > 10000) { 
        ESP_LOGW("TOUCH_WATCHDOG", "Sustained press limit reached. Executing soft clear reset...");
        last_action = "";
        is_down_stroke_fired = false;
        wake_prevent_latch = false;
        continuous_touch_start = 0;
        soft_debounce_lift_time = now;
        return; 
      }

      // Check backlight/sleep state natively
      esphome::light::LightState *display_backlight = nullptr;
      for (auto *light : esphome::App.get_lights()) {
        if (light->get_name() == "Display Backlight") { 
          display_backlight = light;
          break;
        }
      }
      
      bool is_backlight_off = (display_backlight != nullptr && 
                              (!display_backlight->remote_values.is_on() || display_backlight->remote_values.get_brightness() == 0.0f));

      if (!wake_prevent_latch && is_backlight_off) {
        if (display_backlight != nullptr) {
          auto call = display_backlight->make_call();
          call.set_state(true);
          call.perform();
        }
        execute_action_pipeline("keep_alive", KEY_EVENT_DOWN, SOURCE_TOUCH_WAKE);
        wake_prevent_latch = true; 
        wake_timestamp = now; 
        last_action = "WAKE_GUARD";
        soft_debounce_lift_time = now;
        return; 
      }

      if (wake_prevent_latch || last_action == "WAKE_GUARD") {
        if (now - wake_timestamp < 600) return;
      }

      // ─── HARDWARE RE-TOUCH DEBOUNCE FILTER ───
      if (last_action == "" && (now - soft_debounce_lift_time < 200)) {
        return; // Absorbs physical glass liftoff micro-chatter instantly
      }

      uint16_t tx = touch_engine.touch.x[0];
      uint16_t ty = touch_engine.touch.y[0];
      if (ty < 144) return;
      
      int col = tx / 80;          
      int row = (ty - 144) / 42;  
      std::string action = "";
      
      if (row == 0)      { action = (col < 2) ? "power_on" : "power_off"; }
      else if (row == 1) {
        if (col == 0) action = "home";
        if (col == 1) action = "cursor_up";
        if (col == 2) action = "game_menu";
        if (col == 3) action = "volume_up";
      }
      else if (row == 2) {
        if (col == 0) action = "cursor_left";
        if (col == 1) action = "cursor_enter";
        if (col == 2) action = "cursor_right";
        if (col == 3) action = "volume_down";
      }
      else if (row == 3) {
        if (col == 0) action = "back";
        if (col == 1) action = "cursor_down";
        if (col == 2) action = "settings_menu";
        if (col == 3) action = "mute";
      }
      else if (row == 4) {
        if (col == 0) action = "focus_manual";
        if (col == 1) action = "focus_auto";
        if (col == 2) action = "picture";
        if (col == 3) action = "input";
      }
      else if (row == 5) {
        if (col == 0) action = "shortcut_1";
        if (col == 1) action = "shortcut_2";
        if (col == 2) action = "shortcut_3";
        if (col == 3) action = "shortcut_4";
      }
      else if (row == 6) {
        if (col == 0) action = "macro_record";
        if (col == 1) action = "macro_play";
        if (col == 2) action = "";
        if (col == 3) action = "ble_pair";
      }
      else if (row == 7) {
        if (col == 0) action = "token_sniff";
        if (col == 1) action = "token_clear";
        if (col == 2) action = "token_recall";
        if (col == 3) action = "ble_clear";
      }

      if (action == "") return;

      // ─── 🟢 TRUE INFRARED PHYSICAL EMULATION ENGINE ───
      if (last_action != action) {
        // If sliding across keys, reset flags instantly
        is_down_stroke_fired = false;
        last_action = action;
        touch_down_timestamp = now;
      }

      if (!is_down_stroke_fired) {
        // 🚀 CRITICAL DISCRETE PRESS LOCK: Fires exactly once on touchdown.
        // This guarantees step counts log cleanly and macro_record never loops or self-cancels!
        execute_action_pipeline(action, KEY_EVENT_DOWN, SOURCE_TOUCH_PAD);
        is_down_stroke_fired = true;
        last_packet_sent_time = now;
      } 
      else {
        // Continuous held press processing
        uint32_t held_duration = now - touch_down_timestamp;
        
        // Match standard physical remotes: Typematic initial hold delay window of 500ms
        if (held_duration >= 500) {
          // Pace repeating update strides cleanly at a 120ms cadence interval
          if (now - last_packet_sent_time >= 120) {
            execute_action_pipeline(action, KEY_EVENT_DOWN, SOURCE_TOUCH_PAD);
            last_packet_sent_time = now;
          }
        }
      }

    } else {
      // Finger has left the glass surface entirely
      continuous_touch_start = 0;
      
      if (wake_prevent_latch || last_action != "") {
        if (wake_prevent_latch || last_action == "WAKE_GUARD") {
          if (now - wake_timestamp >= 600) {
            wake_prevent_latch = false;
            last_action = "";
            soft_debounce_lift_time = now;
          }
          return;
        }

        // Clean slate tracking clear
        is_down_stroke_fired = false;
        last_action = ""; 
        soft_debounce_lift_time = now; // Initialize our 200ms re-touch cooling window guard
      }
    }
  }
}; 

} // namespace st77922_touch
