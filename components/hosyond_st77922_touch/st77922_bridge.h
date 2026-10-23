#pragma once
#include "esphome/core/component.h"
#include "esphome/components/light/light_state.h"
#include "ST77922_Touch.h" 

#ifndef INPUT_SOURCES_DEFINED
#define INPUT_SOURCES_DEFINED

enum InputSource : uint32_t {
    SOURCE_WEB        = 0x0000, // Browser console injection clicks
    SOURCE_TOUCH_WAKE = 0x9999, // Screen wake/illumination touch pass
    SOURCE_TOUCH_PAD  = 0xBBBB, // Standard button grid interaction taps
};

#endif

void execute_action_pipeline(const std::string& cmd_name, int event_state, uint32_t raw_ir_hex = 0);

namespace st77922_touch {

class ST77922TouchBridge : public esphome::Component {
 public:
  ST77922_TOUCH touch_engine;
  std::string last_action = "";
  uint32_t last_trigger = 0;
  
  bool wake_prevent_latch = false;
  uint32_t wake_timestamp = 0; 
  uint32_t continuous_touch_start = 0;

  uint32_t touch_down_timestamp = 0;   
  uint32_t last_packet_sent_time = 0;  
  bool is_down_stroke_fired = false;   
  uint32_t soft_debounce_lift_time = 0;

  void setup() override {
    touch_engine.init();
    touch_engine.Set_Rotation(0);
  }

  void loop() override {
    uint32_t now = esphome::millis();

    if (touch_engine.Get_Touch()) {
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
        execute_action_pipeline("keep_alive", 0, SOURCE_TOUCH_WAKE);
        wake_prevent_latch = true; 
        wake_timestamp = now; 
        last_action = "WAKE_GUARD";
        soft_debounce_lift_time = now;
        return; 
      }

      if (wake_prevent_latch || last_action == "WAKE_GUARD") {
        if (now - wake_timestamp < 600) return;
      }

      // GLOBAL RELEASE LOCKOUT GATE
      if (!is_down_stroke_fired && (now - soft_debounce_lift_time < 200)) {
        return; 
      }

      // ─── ✅ ACCURATE SINGLE-TOUCH ARRAY BUFFERS INDEX CODES ───
      uint16_t tx = touch_engine.touch.x[0];
      uint16_t ty = touch_engine.touch.y[0];
      if (ty < 144) return;
      
      int col = tx / 80;          
      int row = (ty - 144) / 42;  
      
      // 🛠️ THE SCOPING MATRIX FIX: Declare action right here so both view gates see it!
      std::string action = "";

      // GLOBAL PRESS SETTLING NOISE FILTER
      if (!is_down_stroke_fired) {
        if (touch_down_timestamp == 0) {
          touch_down_timestamp = now; 
          return;
        }
        if (now - touch_down_timestamp < 35) {
          return; 
        }
      }

      if (!is_down_stroke_fired) {
        ESP_LOGI("TOUCH_COORD", "DEBOUNCED CONTACT -> x: %u, y: %u | GRID -> col: %d, row: %d | VIEW_LOCK: %s", 
                 tx, ty, col, row, last_action.c_str());
      }

      if (last_action == "PROFILE_MENU_LOCK") {
        if (row == 7) {
          action = "go_to_remote_page"; 
        }
        else { 
          bool is_left_column = (tx < 182); 
          int profile_idx = -1;
          
          if (row == 0) profile_idx = is_left_column ? 0  : 10; 
          if (row == 1) profile_idx = is_left_column ? 1  : 11; 
          if (row == 2) profile_idx = is_left_column ? 2  : 12; 
          if (row == 3) profile_idx = is_left_column ? 3  : 13; 
          if (row == 4) profile_idx = is_left_column ? 7  : 14; 
          if (row == 5) profile_idx = is_left_column ? 8  : 15; 
          if (row == 6) profile_idx = is_left_column ? 9  : -1; 
          
          if (profile_idx != -1) {
            action = "set_p_" + std::to_string(profile_idx);
          }
        }
        
        if (action != "") {
          if (!is_down_stroke_fired) {
            execute_action_pipeline(action, 0, SOURCE_TOUCH_PAD);
            is_down_stroke_fired = true; 
            touch_down_timestamp = 0;
            soft_debounce_lift_time = now; 
            
            // ─── ✅ FIXED THE VIEWPORT TRANSITION HANDSHAKE GATES ───
            // Enforce a strict release lock for BOTH options so that trailing
            // contact frames cannot fall through to the standard remote layouts.
            last_action = "PROFILE_RELEASE_PENDING";
          }
        }
        
        if (action == "") {
          is_down_stroke_fired = false;
        }
        return; 
      }

      // 🛠️ THE TRAILING SHIELD: If we are waiting for the user to lift their finger after choosing a profile,
      // hard stop here. This blocks the loop from falling through to standard remote rows!
      if (last_action == "PROFILE_RELEASE_PENDING") {
          return;
      }

      // ─── BRANCH B: STANDARD REMOTE CONTROL LAYOUT GRID DETECTION MATRIX ───
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
        if (col == 2) action = "ble_pair";
        if (col == 3) action = "ble_clear";       
      }
      else if (row == 7) {
        if (col == 0) action = "token_sniff";
        if (col == 1) action = "token_clear";      
        if (col == 2) action = "token_recall";
        if (col == 3) action = "go_to_profiles_page"; 
      }

      if (action == "") return;

      if (last_action != action) {
        if (is_down_stroke_fired && !last_action.empty() && last_action != "WAKE_GUARD") {
            execute_action_pipeline(last_action, 2, SOURCE_TOUCH_PAD); 
        }
        is_down_stroke_fired = false;
        last_action = action;
        last_packet_sent_time = 0; 
      }

      if (!is_down_stroke_fired) {
        execute_action_pipeline(action, 0, SOURCE_TOUCH_PAD); 
        is_down_stroke_fired = true;
        last_packet_sent_time = now;
      } 
      else {
        uint32_t held_duration = now - touch_down_timestamp;
        if (held_duration >= 500) {
          if (now - last_packet_sent_time >= 75) {
            execute_action_pipeline(action, 1, SOURCE_TOUCH_PAD); 
            last_packet_sent_time = now;
          }
        }
      }

    } else {
      // ─── HARDWARE RELEASE/LIFTOFF HANDLER ───
      continuous_touch_start = 0;
      touch_down_timestamp = 0; 
      
      if (is_down_stroke_fired) {
          if (!last_action.empty() && last_action != "PROFILE_MENU_LOCK" && last_action != "PROFILE_RELEASE_PENDING" && last_action != "WAKE_GUARD") {
              execute_action_pipeline(last_action, 2, SOURCE_TOUCH_PAD); 
          }
          
          // 🛠️ THE RELEASE FLUSHER: Clear out your custom layout tracking state variables
          // only after your hand has fully left the display glass panel surface!
          if (last_action == "PROFILE_RELEASE_PENDING") {
              last_action = "";
          }
          
          is_down_stroke_fired = false;
          soft_debounce_lift_time = now; 
      }
            
      if (wake_prevent_latch || last_action != "") {
        if (wake_prevent_latch || last_action == "WAKE_GUARD") {
          if (now - wake_timestamp >= 600) {
            wake_prevent_latch = false;
            last_action = "";
            soft_debounce_lift_time = now;
          }
          return;
        }
        
        if (last_action != "PROFILE_MENU_LOCK" && last_action != "go_to_profiles_page") {
           last_action = ""; 
        }
      }
    }
  }
}; 

} // namespace st77922_touch
