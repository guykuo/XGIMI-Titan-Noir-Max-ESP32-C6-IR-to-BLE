#pragma once
#include "esphome/core/component.h"
#include "ST77922_Touch.h"

// We inherit directly from the core Component layer to guarantee isolated loop executions
class ST77922TouchBridge : public esphome::Component {
 public:
  ST77922_TOUCH touch_engine;
  std::string last_action = "";
  uint32_t last_trigger = 0;

  void setup() override {
    // Exact initialization routine working in your native Arduino environment
    touch_engine.init();
    touch_engine.Set_Rotation(0);
  }

  void loop() override {
    if (touch_engine.Get_Touch()) {
      // Pull coordinates cleanly out of the active hardware arrays
      uint16_t tx = touch_engine.touch.x[0];
      uint16_t ty = touch_engine.touch.y[0];

      // Ignore any contact resting inside the top logging telemetry matrix window
      if (ty < 144) return;

      int col = tx / 80;          
      int row = (ty - 144) / 42;  
      std::string action = "";
      bool is_monostable = false;

      // Remote UI Layout Grid Router Mapping
      if (row == 0) { action = (col < 2) ? "power_on" : "power_off"; is_monostable = true; }
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
        if (col == 0 || col == 1) is_monostable = true;
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
        is_monostable = true;
      }
      else if (row == 7) {
        if (col == 0) action = "token_sniff";
        if (col == 1) action = "token_clear";
        if (col == 2) action = "token_recall";
        is_monostable = true;
      }

      if (action == "") return;

      // Throttle multi-click event floods gracefully without blocking background tasks
      if (last_action != action || (millis() - last_trigger > 250)) {
        if (last_action != action) {
          execute_action_pipeline(action, KEY_EVENT_DOWN, 0xEFFF);
        } else if (!is_monostable) {
          execute_action_pipeline(action, KEY_EVENT_HOLDING, 0xEFFF);
        }
        last_action = action;
        last_trigger = millis();
      }
    } else {
      // Clear out states instantly when finger breaks terminal glass surface contact
      if (last_action != "") {
        execute_action_pipeline("release_all", KEY_EVENT_UP, 0xEFFF);
        last_action = "";
      }
    }
  }
};
