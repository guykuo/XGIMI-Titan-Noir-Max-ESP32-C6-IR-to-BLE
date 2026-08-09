#include "xgimi_remote.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_bt_device.h>
#include <esp_err.h>
#include <cstring>
#include <vector>

namespace esphome::xgimi_remote {

static const char *const TAG = "xgimi_remote";
static constexpr uint16_t IMMEDIATE_POWER_OFF_HOLD_MS = 1500;
static constexpr uint32_t POWER_OFF_RETRY_INTERVAL_MS = 500;
static constexpr uint32_t POWER_ON_AFTER_OFF_SETTLE_MS = 15000;
static constexpr uint32_t STARTUP_OBSERVATION_GRACE_MS = 5000;
static constexpr uint32_t ACTUAL_OFF_DEBOUNCE_MS = 500;
static constexpr uint32_t WAKE_COUNTER_PREF_KEY = 0x58474331;
static constexpr uint8_t WAKE_COUNTER_PREF_MAGIC = 0xA7;

void XgimiRemote::setup() {
  this->wake_counter_pref_ =
      global_preferences->make_preference<SavedWakeCounter>(WAKE_COUNTER_PREF_KEY, true);
  SavedWakeCounter saved{};
  if (this->wake_counter_pref_.load(&saved) && saved.magic == WAKE_COUNTER_PREF_MAGIC) {
    this->last_wake_counter_valid_ = true;
    this->last_wake_counter_ = saved.counter;
    this->persisted_wake_counter_valid_ = true;
    this->persisted_wake_counter_ = saved.counter;
    ESP_LOGI(TAG, "Restored last wake counter %u (0x%02X)", saved.counter, saved.counter);
  }
  ESP_LOGI(TAG, "Initialising captured XGIMI remote emulation");
}

void XgimiRemote::loop() {
  if (!this->ble_ready_ && this->ble_ != nullptr && this->ble_->is_active()) {
    this->ble_ready_ = true;
    this->set_advertised_name_(this->remote_name_.c_str());
    this->startup_observation_pending_ = true;
    this->startup_observation_at_ms_ = millis() + STARTUP_OBSERVATION_GRACE_MS;
  }

  if (this->held_keyboard_active_ &&
      static_cast<int32_t>(millis() - this->held_release_ms_) >= 0) {
    this->release_held_keyboard_();
  }

  if (this->keyboard_tap_active_ &&
      static_cast<int32_t>(millis() - this->keyboard_tap_release_ms_) >= 0) {
    this->release_keyboard_tap_();
  }

  if (this->consumer_tap_active_ &&
      static_cast<int32_t>(millis() - this->consumer_tap_release_ms_) >= 0) {
    this->release_consumer_tap_();
  }

  if (this->actual_off_debounce_pending_ &&
      static_cast<int32_t>(millis() - this->actual_off_debounce_at_ms_) >= 0) {
    this->actual_off_debounce_pending_ = false;
    if (!this->connected_)
      this->process_actual_power_state_(false);
  }

  if (this->startup_observation_pending_ && this->ble_ready_ &&
      static_cast<int32_t>(millis() - this->startup_observation_at_ms_) >= 0) {
    this->startup_observation_pending_ = false;
    if (!this->actual_power_known_) {
      ESP_LOGI(TAG, "Startup observation grace elapsed; projector HID link is disconnected");
      this->process_actual_power_state_(false);
    }
  }

  if (this->power_command_ == PowerCommand::POWER_OFF && this->power_command_sent_ &&
      this->power_off_retry_pending_ && this->connected_ &&
      this->authenticated_ && this->is_keyboard_subscribed() && !this->held_keyboard_active_ &&
      static_cast<int32_t>(millis() - this->power_off_retry_at_ms_) >= 0) {
    this->power_off_retry_pending_ = false;
    this->power_off_attempts_++;
    ESP_LOGI(TAG, "Retrying immediate Power Off attempt %u because desired is OFF and actual remains ON",
             static_cast<unsigned>(this->power_off_attempts_));
    this->hold_keyboard(0x7F);
  }

  if (this->power_command_ != PowerCommand::NONE && !this->power_command_sent_ &&
      static_cast<int32_t>(millis() - this->power_command_not_before_ms_) >= 0) {
    if (this->power_command_ == PowerCommand::POWER_ON && !this->connected_ && this->ble_ready_ &&
        this->server_ != nullptr) {
      this->power_command_sent_ = true;
      ESP_LOGI(TAG, "Transmitting queued Power On command");
      this->start_wake_sweep_();
    } else if (this->power_command_ == PowerCommand::POWER_OFF && this->connected_ && this->authenticated_ &&
               this->is_keyboard_subscribed()) {
      this->power_command_sent_ = true;
      this->power_off_attempts_ = 1;
      ESP_LOGI(TAG, "Transmitting immediate Power Off attempt 1");
      this->hold_keyboard(0x7F);
    }
  }

  if (!this->wake_active_ || this->server_ == nullptr)
    return;

  if (this->single_wake_active_) {
    if (static_cast<int32_t>(millis() - this->single_wake_until_ms_) < 0)
      return;

    if (this->single_wake_gap_active_) {
      this->single_wake_gap_active_ = false;
      this->server_->set_manufacturer_data(this->make_wake_packet_(this->single_wake_counter_));
      this->record_last_wake_counter_(this->single_wake_counter_, true);
      this->single_wake_until_ms_ = millis() + this->single_wake_duration_ms_;
      ESP_LOGI(TAG, "Advertising exact XGIMI wake counter %u (0x%02X) for %u ms after an off-air gap",
               this->single_wake_counter_, this->single_wake_counter_,
               static_cast<unsigned>(this->single_wake_duration_ms_));
      return;
    }

    this->stop_wake_advertising_();
    ESP_LOGI(TAG, "Completed exact-counter wake advertisement");
    return;
  }

  if (static_cast<int32_t>(millis() - this->next_wake_counter_at_ms_) < 0)
    return;

  if (this->wake_advertising_gap_active_) {
    this->wake_advertising_gap_active_ = false;
    this->advertise_next_wake_counter_();
  } else {
    this->begin_wake_advertising_gap_();
  }
}

void XgimiRemote::dump_config() {
  ESP_LOGCONFIG(TAG,
                "XGIMI remote clone:\n"
                "  BLE HID name: %s\n"
                "  Wake advertisement name: %s\n"
                "  Wake token length: %u bytes\n"
                "  Tap duration: %u ms\n"
                "  Immediate power-off hold: %u ms\n"
                "  Power-off retry interval after release: %u ms\n"
                "  Power-on settle after an ON-to-OFF transition: %u ms\n"
                "  Actual-off debounce: %u ms\n"
                "  Power-off retry: once transmitted, continuous until actual becomes OFF\n"
                "  Power-on retry: once transmitted, continuous until HID connects\n"
                "  Wake-counter dwell: %u ms\n"
                "  Off-air gap between wake-counter advertisements: %u ms\n"
                "  Exact-counter advertisement duration: %u ms\n"
                "  Last wake counter: %s\n"
                "  Captured buttons: 19",
                this->remote_name_.c_str(), this->remote_name_.c_str(),
                static_cast<unsigned>(this->wake_token_.size()),
                static_cast<unsigned>(this->tap_duration_ms_),
                static_cast<unsigned>(IMMEDIATE_POWER_OFF_HOLD_MS),
                static_cast<unsigned>(POWER_OFF_RETRY_INTERVAL_MS),
                static_cast<unsigned>(POWER_ON_AFTER_OFF_SETTLE_MS),
                static_cast<unsigned>(ACTUAL_OFF_DEBOUNCE_MS),
                static_cast<unsigned>(this->wake_counter_dwell_ms_),
                static_cast<unsigned>(this->wake_advertisement_gap_ms_),
                static_cast<unsigned>(this->single_wake_duration_ms_),
                this->last_wake_counter_valid_ ? str_sprintf("%u (0x%02X)", this->last_wake_counter_,
                                                              this->last_wake_counter_).c_str()
                                               : "not yet sent");
}

void XgimiRemote::adopt_actual_power_as_desired_() {
  this->desired_power_on_ = this->processed_actual_power_on_;
  this->desired_power_initialised_ = true;
  this->deferred_desired_request_ = false;
  ESP_LOGI(TAG, "Adopted uncommanded actual Power as desired Power: %s", ONOFF(this->desired_power_on_));
}

void XgimiRemote::set_desired_power_(bool on) {
  const bool changed = !this->desired_power_initialised_ || this->desired_power_on_ != on;
  this->desired_power_on_ = on;
  this->desired_power_initialised_ = true;

  if (changed) {
    ESP_LOGI(TAG, "Desired Power changed to %s", ONOFF(this->desired_power_on_));
  } else {
    ESP_LOGI(TAG, "Desired Power is already %s", ONOFF(this->desired_power_on_));
  }

  if (!this->actual_power_known_) {
    this->deferred_desired_request_ = true;
    ESP_LOGI(TAG, "Deferring desired Power request until the initial actual state is known");
    return;
  }

  if (this->power_command_ != PowerCommand::NONE && !this->power_command_sent_) {
    if (this->desired_power_on_ == this->processed_actual_power_on_) {
      ESP_LOGI(TAG, "Cancelling unsent Power command because the replacement desired state already matches actual");
      this->clear_power_command_();
      return;
    }

    const PowerCommand replacement = this->desired_power_on_ ? PowerCommand::POWER_ON : PowerCommand::POWER_OFF;
    if (replacement != this->power_command_) {
      ESP_LOGI(TAG, "Replacing unsent Power command with the latest desired state");
      this->queue_power_command_(replacement);
    }
    return;
  }

  if (this->power_command_ != PowerCommand::NONE && this->power_command_sent_) {
    ESP_LOGI(TAG, "A transmitted Power command is still awaiting its actual-state acknowledgement; latest desired state retained");
    return;
  }

  this->reconcile_power_state_();
}

void XgimiRemote::process_actual_power_state_(bool on) {
  this->startup_observation_pending_ = false;

  if (!this->actual_power_known_) {
    this->actual_power_known_ = true;
    this->processed_actual_power_on_ = on;
    ESP_LOGI(TAG, "Initial actual Power state observed: %s", ONOFF(on));
    if (this->deferred_desired_request_) {
      this->deferred_desired_request_ = false;
      this->reconcile_power_state_();
    } else {
      this->adopt_actual_power_as_desired_();
    }
    return;
  }

  if (this->processed_actual_power_on_ == on)
    return;

  this->processed_actual_power_on_ = on;
  if (on) {
    this->actual_off_since_valid_ = false;
  } else {
    this->actual_off_since_valid_ = true;
    this->actual_off_since_ms_ = millis();
  }
  ESP_LOGI(TAG, "Observed Power changed to %s", ONOFF(on));

  if (this->power_command_ == PowerCommand::NONE) {
    ESP_LOGI(TAG, "Observed Power changed without a pending command; treating it as an external change");
    this->adopt_actual_power_as_desired_();
    return;
  }

  const bool expected_on = this->power_command_ == PowerCommand::POWER_ON;
  if (on == expected_on) {
    ESP_LOGI(TAG, "Observed Power acknowledged the pending %s command",
             expected_on ? "Power On" : "Power Off");
    this->clear_power_command_();
    this->reconcile_power_state_();
    return;
  }

  ESP_LOGW(TAG, "Observed Power changed contrary to the pending command; treating the observation as authoritative");
  this->clear_power_command_();
  this->adopt_actual_power_as_desired_();
}

void XgimiRemote::reconcile_power_state_() {
  if (!this->desired_power_initialised_ || !this->actual_power_known_ || this->power_command_ != PowerCommand::NONE)
    return;

  if (this->desired_power_on_ == this->processed_actual_power_on_) {
    ESP_LOGI(TAG, "Desired and actual Power already match (%s)", ONOFF(this->processed_actual_power_on_));
    return;
  }

  this->queue_power_command_(this->desired_power_on_ ? PowerCommand::POWER_ON : PowerCommand::POWER_OFF);
}

void XgimiRemote::queue_power_command_(PowerCommand command) {
  const uint32_t now = millis();
  this->power_command_ = command;
  this->power_command_sent_ = false;
  this->power_command_not_before_ms_ = now;
  if (command == PowerCommand::POWER_ON && this->actual_off_since_valid_) {
    const uint32_t elapsed = now - this->actual_off_since_ms_;
    if (elapsed < POWER_ON_AFTER_OFF_SETTLE_MS)
      this->power_command_not_before_ms_ = now + (POWER_ON_AFTER_OFF_SETTLE_MS - elapsed);
  }
  this->power_off_retry_pending_ = false;
  this->power_off_retry_at_ms_ = 0;
  this->power_off_attempts_ = 0;
  ESP_LOGI(TAG, "Queued %s to reconcile desired and actual Power%s",
           command == PowerCommand::POWER_ON ? "Power On" : "Power Off",
           this->power_command_not_before_ms_ == now
               ? ""
               : str_sprintf(" after %u ms post-off settling",
                             static_cast<unsigned>(this->power_command_not_before_ms_ - now)).c_str());
}

void XgimiRemote::clear_power_command_() {
  this->power_command_ = PowerCommand::NONE;
  this->power_command_sent_ = false;
  this->power_command_not_before_ms_ = 0;
  this->power_off_retry_pending_ = false;
  this->power_off_retry_at_ms_ = 0;
  this->power_off_attempts_ = 0;
}

void XgimiRemote::start_wake_sweep_() {
  if (!this->ble_ready_ || this->server_ == nullptr) {
    ESP_LOGW(TAG, "Cannot start wake-counter sweep before BLE is ready");
    return;
  }
  if (this->connected_) {
    ESP_LOGI(TAG, "Projector Power On ignored: HID link is already connected");
    return;
  }
  if (this->wake_active_) {
    ESP_LOGI(TAG, "Projector Power On ignored: wake advertising is already in progress");
    return;
  }
  this->set_advertised_name_(this->remote_name_.c_str());
  this->single_wake_active_ = false;
  this->single_wake_gap_active_ = false;
  this->wake_advertising_gap_active_ = false;
  this->wake_active_ = true;
  this->wake_values_sent_ = 0;
  this->wake_cycles_completed_ = 0;
  ESP_LOGI(TAG, "Starting continuous XGIMI wake-counter sweep after %s with %u ms dwell",
           this->last_wake_counter_valid_
               ? str_sprintf("%u (0x%02X)", this->last_wake_counter_, this->last_wake_counter_).c_str()
               : "no previous counter",
           static_cast<unsigned>(this->wake_counter_dwell_ms_));
  this->advertise_next_wake_counter_();
}

void XgimiRemote::stop_wake_advertising_() {
  if (this->server_ != nullptr)
    this->server_->set_manufacturer_data({0x46, 0x00});
  this->set_advertised_name_(this->remote_name_.c_str());
  this->single_wake_active_ = false;
  this->single_wake_gap_active_ = false;
  this->wake_advertising_gap_active_ = false;
  this->wake_active_ = false;
  this->next_wake_counter_at_ms_ = 0;
}

void XgimiRemote::begin_wake_advertising_gap_() {
  const esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Could not stop wake advertising for the inter-press gap: %s", esp_err_to_name(err));
  }
  this->wake_advertising_gap_active_ = true;
  this->next_wake_counter_at_ms_ = millis() + this->wake_advertisement_gap_ms_;
}

void XgimiRemote::advertise_next_wake_counter_() {
  const uint8_t counter = this->last_wake_counter_valid_
                              ? static_cast<uint8_t>(this->last_wake_counter_ + 1)
                              : 0;
  this->server_->set_manufacturer_data(this->make_wake_packet_(counter));
  this->record_last_wake_counter_(counter, true);
  this->wake_values_sent_++;
  if ((this->wake_values_sent_ & 0xFFU) == 0) {
    this->wake_cycles_completed_++;
    ESP_LOGI(TAG, "Completed wake-counter sweep cycle %u; HID remains disconnected",
             static_cast<unsigned>(this->wake_cycles_completed_));
  }
  this->next_wake_counter_at_ms_ = millis() + this->wake_counter_dwell_ms_;
}

void XgimiRemote::start_wake_counter(uint8_t counter) {
  if (!this->ble_ready_ || this->server_ == nullptr) {
    ESP_LOGW(TAG, "Cannot send an exact wake counter before BLE is ready");
    return;
  }
  if (this->connected_) {
    ESP_LOGI(TAG, "Exact-counter Power On ignored: HID link is already connected");
    return;
  }
  if (this->wake_active_) {
    ESP_LOGI(TAG, "Exact-counter Power On ignored: wake advertising is already in progress");
    return;
  }

  this->set_advertised_name_(WAKE_NAME);
  const esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK)
    ESP_LOGW(TAG, "Could not stop advertising before the exact-counter press: %s", esp_err_to_name(err));
  this->single_wake_counter_ = counter;
  this->single_wake_active_ = true;
  this->single_wake_gap_active_ = true;
  this->wake_advertising_gap_active_ = false;
  this->single_wake_until_ms_ = millis() + 100;
  this->wake_active_ = true;
  ESP_LOGI(TAG, "Starting exact-counter wake press %u (0x%02X) with a %u ms off-air gap",
           counter, counter, 100U);
}

void XgimiRemote::request_power_on() {
  this->set_desired_power_(true);
}

void XgimiRemote::request_immediate_power_off() {
  this->set_desired_power_(false);
}

void XgimiRemote::start_pairing_mode() {
  if (!this->ble_ready_ || this->server_ == nullptr) {
    ESP_LOGW(TAG, "Cannot start pairing mode before BLE is ready");
    return;
  }
  this->stop_wake_advertising_();
  ESP_LOGI(TAG, "Advertising BLE HID device as %s", this->remote_name_.c_str());
}

void XgimiRemote::notify_keyboard_(const uint8_t data[8]) {
  if (this->server_ == nullptr || this->keyboard_report_ == nullptr ||
      this->server_->get_connected_client_count() == 0) {
    ESP_LOGW(TAG, "Keyboard command ignored: projector HID link is not connected");
    return;
  }
  ESP_LOGI(TAG, "Sending keyboard HID report %02X%02X%02X%02X%02X%02X%02X%02X (subscribers=%u)", data[0],
           data[1], data[2], data[3], data[4], data[5], data[6], data[7],
           static_cast<unsigned>(this->keyboard_report_->get_notify_client_count()));
  this->keyboard_report_->set_value(std::vector<uint8_t>(data, data + 8));
  this->keyboard_report_->notify();
}

void XgimiRemote::notify_consumer_(const uint8_t data[6]) {
  if (this->server_ == nullptr || this->consumer_report_ == nullptr ||
      this->server_->get_connected_client_count() == 0) {
    ESP_LOGW(TAG, "Consumer command ignored: projector HID link is not connected");
    return;
  }
  ESP_LOGI(TAG, "Sending consumer HID report %02X%02X%02X%02X%02X%02X (subscribers=%u)", data[0], data[1],
           data[2], data[3], data[4], data[5],
           static_cast<unsigned>(this->consumer_report_->get_notify_client_count()));
  this->consumer_report_->set_value(std::vector<uint8_t>(data, data + 6));
  this->consumer_report_->notify();
}

void XgimiRemote::press_keyboard(uint8_t usage) {
  if (this->held_keyboard_active_) {
    ESP_LOGI(TAG, "Keyboard tap 0x%02X ignored while held key 0x%02X is active", usage,
             this->held_keyboard_usage_);
    return;
  }
  uint8_t press[8] = {0x00, 0x00, usage, 0x00, 0x00, 0x00, 0x00, 0x00};
  if (this->keyboard_tap_active_)
    this->release_keyboard_tap_();
  this->notify_keyboard_(press);
  this->keyboard_tap_active_ = true;
  this->keyboard_tap_release_ms_ = millis() + this->tap_duration_ms_;
  ESP_LOGI(TAG, "Holding keyboard tap 0x%02X for %u ms", usage,
           static_cast<unsigned>(this->tap_duration_ms_));
}

void XgimiRemote::hold_keyboard(uint8_t usage) {
  if (!this->connected_ || !this->authenticated_ || !this->is_keyboard_subscribed()) {
    ESP_LOGI(TAG, "Held keyboard command ignored: authenticated HID link is not ready");
    return;
  }
  if (this->held_keyboard_active_) {
    ESP_LOGI(TAG, "Held keyboard command 0x%02X ignored while key 0x%02X is active", usage,
             this->held_keyboard_usage_);
    return;
  }
  if (this->keyboard_tap_active_)
    this->release_keyboard_tap_();

  uint8_t press[8] = {0x00, 0x00, usage, 0x00, 0x00, 0x00, 0x00, 0x00};
  this->notify_keyboard_(press);
  this->held_keyboard_usage_ = usage;
  this->held_keyboard_active_ = true;
  this->held_release_ms_ = millis() + IMMEDIATE_POWER_OFF_HOLD_MS;
  ESP_LOGI(TAG, "Holding keyboard usage 0x%02X for %u ms", usage,
           static_cast<unsigned>(IMMEDIATE_POWER_OFF_HOLD_MS));
}

void XgimiRemote::release_keyboard_tap_() {
  uint8_t release[8] = {};
  this->notify_keyboard_(release);
  this->keyboard_tap_active_ = false;
}

void XgimiRemote::release_held_keyboard_() {
  uint8_t release[8] = {};
  this->notify_keyboard_(release);
  ESP_LOGI(TAG, "Released held keyboard usage 0x%02X", this->held_keyboard_usage_);
  this->held_keyboard_active_ = false;
  this->held_keyboard_usage_ = 0;
  if (this->power_command_ == PowerCommand::POWER_OFF && this->power_command_sent_ &&
      this->processed_actual_power_on_ && this->connected_) {
    this->power_off_retry_pending_ = true;
    this->power_off_retry_at_ms_ = millis() + POWER_OFF_RETRY_INTERVAL_MS;
    ESP_LOGI(TAG, "Actual Power remains ON; next immediate Power Off attempt is due in %u ms",
             static_cast<unsigned>(POWER_OFF_RETRY_INTERVAL_MS));
  }
}

void XgimiRemote::press_consumer(uint16_t usage) {
  if (this->held_keyboard_active_) {
    ESP_LOGI(TAG, "Consumer tap 0x%04X ignored while held keyboard key 0x%02X is active", usage,
             this->held_keyboard_usage_);
    return;
  }
  uint8_t press[6] = {static_cast<uint8_t>(usage & 0xFF), static_cast<uint8_t>((usage >> 8) & 0xFF),
                      0x00, 0x00, 0x00, 0x00};
  if (this->consumer_tap_active_)
    this->release_consumer_tap_();
  this->notify_consumer_(press);
  this->consumer_tap_active_ = true;
  this->consumer_tap_release_ms_ = millis() + this->tap_duration_ms_;
  ESP_LOGI(TAG, "Holding consumer tap 0x%04X for %u ms", usage,
           static_cast<unsigned>(this->tap_duration_ms_));
}

void XgimiRemote::release_consumer_tap_() {
  uint8_t release[6] = {};
  this->notify_consumer_(release);
  this->consumer_tap_active_ = false;
}

std::vector<uint8_t> XgimiRemote::make_wake_packet_(uint8_t counter) const {
  std::vector<uint8_t> packet;
  packet.reserve(3 + this->wake_token_.size());
  packet.push_back(0x46);
  packet.push_back(0x00);
  packet.push_back(counter);
  packet.insert(packet.end(), this->wake_token_.begin(), this->wake_token_.end());
  return packet;
}

void XgimiRemote::record_last_wake_counter_(uint8_t counter, bool persist) {
  this->last_wake_counter_ = counter;
  this->last_wake_counter_valid_ = true;
  if (persist)
    this->persist_last_wake_counter_();
}

void XgimiRemote::persist_last_wake_counter_() {
  if (!this->last_wake_counter_valid_ ||
      (this->persisted_wake_counter_valid_ && this->persisted_wake_counter_ == this->last_wake_counter_))
    return;

  const SavedWakeCounter saved{WAKE_COUNTER_PREF_MAGIC, this->last_wake_counter_};
  if (this->wake_counter_pref_.save(&saved)) {
    this->persisted_wake_counter_valid_ = true;
    this->persisted_wake_counter_ = this->last_wake_counter_;
  } else {
    ESP_LOGW(TAG, "Could not queue last wake counter for persistent storage");
  }
}

void XgimiRemote::set_advertised_name_(const char *name) {
  const esp_err_t err = esp_ble_gap_set_device_name(name);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Could not set BLE device name to %s: %s", name, esp_err_to_name(err));
  }
}

bool XgimiRemote::matches_peer_(const esp_bd_addr_t address) const {
  return this->peer_known_ && memcmp(address, this->peer_address_, sizeof(esp_bd_addr_t)) == 0;
}

void XgimiRemote::restore_hid_subscriptions_() {
  if (this->keyboard_report_ == nullptr || this->consumer_report_ == nullptr)
    return;

  // The bonded Android HID host assumes CCCD values survive a peripheral
  // restart and may not write them again when it reconnects. Restore the two
  // input-report subscriptions once the bond has authenticated.
  this->keyboard_report_->set_notify_for_client(this->peer_conn_id_, true);
  this->consumer_report_->set_notify_for_client(this->peer_conn_id_, true);
  ESP_LOGI(TAG, "Restored bonded HID notification subscriptions for client %u", this->peer_conn_id_);
}

void XgimiRemote::clear_bonds() {
  const int count = esp_ble_get_bond_device_num();
  if (count <= 0) {
    ESP_LOGI(TAG, "No Bluetooth bonds to clear");
    return;
  }

  std::vector<esp_ble_bond_dev_t> devices(count);
  int actual = count;
  if (esp_ble_get_bond_device_list(&actual, devices.data()) != ESP_OK) {
    ESP_LOGW(TAG, "Could not read Bluetooth bond list");
    return;
  }

  for (int i = 0; i < actual; i++)
    esp_ble_remove_bond_device(devices[i].bd_addr);

  this->authenticated_ = false;
  ESP_LOGI(TAG, "Cleared %d Bluetooth bond(s)", actual);
  this->start_pairing_mode();
}

void XgimiRemote::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                      esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CONNECT_EVT: {
      memcpy(this->peer_address_, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      this->peer_conn_id_ = param->connect.conn_id;
      this->peer_known_ = true;
      this->connected_ = true;
      this->authenticated_ = false;
      this->actual_off_debounce_pending_ = false;
      this->startup_observation_pending_ = false;
      if (this->wake_active_) {
        this->persist_last_wake_counter_();
        this->stop_wake_advertising_();
        ESP_LOGI(TAG, "Stopped wake-counter advertising because the projector connected after %u values and %u full cycles",
                 static_cast<unsigned>(this->wake_values_sent_),
                 static_cast<unsigned>(this->wake_cycles_completed_));
      }
      ESP_LOGI(TAG, "Projector HID client connected; requesting bonded encryption");
      const esp_err_t err = esp_ble_set_encryption(this->peer_address_, ESP_BLE_SEC_ENCRYPT);
      if (err != ESP_OK)
        ESP_LOGW(TAG, "Could not request BLE encryption: %s", esp_err_to_name(err));
      this->process_actual_power_state_(true);
      break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
      if (this->matches_peer_(param->disconnect.remote_bda)) {
        if (this->keyboard_report_ != nullptr)
          this->keyboard_report_->set_notify_for_client(param->disconnect.conn_id, false);
        if (this->consumer_report_ != nullptr)
          this->consumer_report_->set_notify_for_client(param->disconnect.conn_id, false);
        this->connected_ = false;
        this->authenticated_ = false;
        this->peer_known_ = false;
        this->held_keyboard_active_ = false;
        this->held_keyboard_usage_ = 0;
        this->keyboard_tap_active_ = false;
        this->consumer_tap_active_ = false;
        this->actual_off_debounce_pending_ = true;
        this->actual_off_debounce_at_ms_ = millis() + ACTUAL_OFF_DEBOUNCE_MS;
        ESP_LOGI(TAG, "Projector HID client disconnected; debouncing actual Power off for %u ms",
                 static_cast<unsigned>(ACTUAL_OFF_DEBOUNCE_MS));
      }
      break;
    default:
      break;
  }
}

void XgimiRemote::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT:
      ESP_LOGI(TAG, "Accepting BLE security request");
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (this->matches_peer_(param->ble_security.auth_cmpl.bd_addr)) {
        this->authenticated_ = param->ble_security.auth_cmpl.success;
        if (this->authenticated_)
          this->restore_hid_subscriptions_();
        if (this->authenticated_)
          ESP_LOGI(TAG, "Projector BLE bond authenticated successfully");
        else
          ESP_LOGW(TAG, "Projector BLE authentication failed, reason 0x%02X",
                   param->ble_security.auth_cmpl.fail_reason);
      }
      break;
    default:
      break;
  }
}

}  // namespace esphome::xgimi_remote
