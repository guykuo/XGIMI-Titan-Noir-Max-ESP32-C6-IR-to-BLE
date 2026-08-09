#pragma once

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble_server/ble_characteristic.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

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
  void set_tap_duration_ms(uint16_t duration_ms) { this->tap_duration_ms_ = duration_ms; }
  void set_wake_token(const std::vector<uint8_t> &token) { this->wake_token_ = token; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  void start_wake_counter(uint8_t counter);
  void set_wake_counter_dwell_ms(uint16_t dwell_ms) {
    this->wake_counter_dwell_ms_ = dwell_ms < 1500 ? 1500 : dwell_ms;
  }
  void set_wake_advertisement_gap_ms(uint16_t gap_ms) {
    this->wake_advertisement_gap_ms_ = gap_ms < 500 ? 500 : gap_ms;
  }
  void set_single_wake_duration_ms(uint16_t duration_ms) {
    this->single_wake_duration_ms_ = duration_ms < 20 ? 20 : duration_ms;
  }
  void request_power_on();
  void request_immediate_power_off();
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
  bool desired_power_state() const { return this->desired_power_on_; }
  bool actual_power_state() const { return this->connected_; }
  bool has_last_wake_counter() const { return this->last_wake_counter_valid_; }
  uint8_t last_wake_counter() const { return this->last_wake_counter_; }
  uint32_t wake_values_sent() const { return this->wake_values_sent_; }
  uint32_t wake_cycles_completed() const { return this->wake_cycles_completed_; }

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                           esp_ble_gatts_cb_param_t *param);

 protected:
  enum class PowerCommand : uint8_t {
    NONE,
    POWER_ON,
    POWER_OFF,
  };

  void set_advertised_name_(const char *name);
  bool matches_peer_(const esp_bd_addr_t address) const;
  void set_desired_power_(bool on);
  void adopt_actual_power_as_desired_();
  void process_actual_power_state_(bool on);
  void reconcile_power_state_();
  void queue_power_command_(PowerCommand command);
  void clear_power_command_();
  void start_wake_sweep_();
  void stop_wake_advertising_();
  void begin_wake_advertising_gap_();
  void advertise_next_wake_counter_();
  void restore_hid_subscriptions_();
  void release_keyboard_tap_();
  void release_consumer_tap_();
  void release_held_keyboard_();
  void notify_keyboard_(const uint8_t data[8]);
  void notify_consumer_(const uint8_t data[6]);
  std::vector<uint8_t> make_wake_packet_(uint8_t counter) const;
  void record_last_wake_counter_(uint8_t counter, bool persist);
  void persist_last_wake_counter_();

  struct SavedWakeCounter {
    uint8_t magic;
    uint8_t counter;
  };

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
  bool single_wake_active_{false};
  bool single_wake_gap_active_{false};
  bool wake_advertising_gap_active_{false};
  uint8_t single_wake_counter_{0};
  uint32_t single_wake_until_ms_{0};
  uint32_t next_wake_counter_at_ms_{0};
  uint16_t wake_counter_dwell_ms_{1500};
  uint16_t wake_advertisement_gap_ms_{500};
  uint16_t single_wake_duration_ms_{4000};
  uint32_t wake_values_sent_{0};
  uint32_t wake_cycles_completed_{0};
  std::vector<uint8_t> wake_token_{};
  bool last_wake_counter_valid_{false};
  uint8_t last_wake_counter_{0};
  bool persisted_wake_counter_valid_{false};
  uint8_t persisted_wake_counter_{0};
  ESPPreferenceObject wake_counter_pref_{};

  bool held_keyboard_active_{false};
  uint8_t held_keyboard_usage_{0};
  uint32_t held_release_ms_{0};
  uint16_t tap_duration_ms_{100};
  bool keyboard_tap_active_{false};
  uint32_t keyboard_tap_release_ms_{0};
  bool consumer_tap_active_{false};
  uint32_t consumer_tap_release_ms_{0};

  bool desired_power_on_{false};
  bool desired_power_initialised_{false};
  bool deferred_desired_request_{false};
  bool actual_power_known_{false};
  bool processed_actual_power_on_{false};
  bool actual_off_since_valid_{false};
  uint32_t actual_off_since_ms_{0};
  bool startup_observation_pending_{false};
  uint32_t startup_observation_at_ms_{0};
  bool actual_off_debounce_pending_{false};
  uint32_t actual_off_debounce_at_ms_{0};
  PowerCommand power_command_{PowerCommand::NONE};
  bool power_command_sent_{false};
  uint32_t power_command_not_before_ms_{0};
  bool power_off_retry_pending_{false};
  uint32_t power_off_retry_at_ms_{0};
  uint32_t power_off_attempts_{0};
};

}  // namespace esphome::xgimi_remote
