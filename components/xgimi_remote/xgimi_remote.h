#pragma once

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble_server/ble_characteristic.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include "esphome/core/component.h"

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <string>
#include <vector>

namespace esphome::xgimi_remote {

class XgimiRemote : public Component {
 public:
  void set_ble(esp32_ble::ESP32BLE *ble) { this->ble_ = ble; }
  void set_server(esp32_ble_server::BLEServer *server) { this->server_ = server; }
  void set_keyboard_report(esp32_ble_server::BLECharacteristic *report) { this->keyboard_report_ = report; }
  void set_consumer_report(esp32_ble_server::BLECharacteristic *report) { this->consumer_report_ = report; }
  void set_remote_name(const std::string &name) { this->remote_name_ = name; }
  void set_wake_token(const std::vector<uint8_t> &token) { this->wake_token_ = token; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  void start_wake_burst();
  void request_power_off();
  void start_pairing_mode();
  void press_keyboard(uint8_t usage);
  void hold_keyboard(uint8_t usage);
  void press_consumer(uint16_t usage);
  void clear_bonds();

  bool is_connected() const { return this->connected_; }
  bool is_authenticated() const { return this->authenticated_; }
  bool is_keyboard_subscribed() const {
    return this->keyboard_report_ != nullptr && this->keyboard_report_->get_notify_client_count() > 0;
  }
  bool is_consumer_subscribed() const {
    return this->consumer_report_ != nullptr && this->consumer_report_->get_notify_client_count() > 0;
  }
  bool is_waking() const { return this->wake_active_; }

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                           esp_ble_gatts_cb_param_t *param);

 protected:
  void set_advertised_name_(const char *name);
  bool matches_peer_(const esp_bd_addr_t address) const;
  void restore_hid_subscriptions_();
  void release_held_keyboard_();
  void notify_keyboard_(const uint8_t data[8]);
  void notify_consumer_(const uint8_t data[6]);

  esp32_ble::ESP32BLE *ble_{nullptr};
  esp32_ble_server::BLEServer *server_{nullptr};
  esp32_ble_server::BLECharacteristic *keyboard_report_{nullptr};
  esp32_ble_server::BLECharacteristic *consumer_report_{nullptr};
  std::string remote_name_{"M5Stack Atom Lite"};

  bool ble_ready_{false};
  bool connected_{false};
  bool authenticated_{false};
  bool peer_known_{false};
  esp_bd_addr_t peer_address_{};
  uint16_t peer_conn_id_{0};

  bool wake_active_{false};
  uint16_t wake_counter_{0};
  std::vector<uint8_t> wake_token_{};

  bool held_keyboard_active_{false};
  uint8_t held_keyboard_usage_{0};
  uint32_t held_release_ms_{0};
};

}  // namespace esphome::xgimi_remote
