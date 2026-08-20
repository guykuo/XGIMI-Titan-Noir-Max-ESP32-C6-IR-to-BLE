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
// static const char *const HID_NAME = "ESP32 xgimi remote";
static const char *const WAKE_NAME = "ESP32 xgimi remote";
static constexpr uint16_t IMMEDIATE_POWER_OFF_HOLD_MS = 1500;

void XgimiRemote::setup() {
  ESP_LOGI(TAG, "Initialising captured XGIMI remote emulation");
}

void XgimiRemote::loop() {
  if (!this->ble_ready_ && this->ble_ != nullptr && this->ble_->is_active()) {
    this->ble_ready_ = true;
    this->set_advertised_name_(this->remote_name_.c_str());
  }

  if (this->held_keyboard_active_ &&
      static_cast<int32_t>(millis() - this->held_release_ms_) >= 0) {
    this->release_held_keyboard_();
  }

  if (!this->wake_active_ || this->server_ == nullptr)
    return;

  if (this->wake_counter_ <= 255) {
    std::vector<uint8_t> packet;
    packet.reserve(3 + this->wake_token_.size());
    packet.push_back(0x46);
    packet.push_back(0x00);
    packet.push_back(static_cast<uint8_t>(this->wake_counter_));
    packet.insert(packet.end(), this->wake_token_.begin(), this->wake_token_.end());
    this->server_->set_manufacturer_data(packet);
    this->wake_counter_++;
    return;
  }

  this->server_->set_manufacturer_data({0x46, 0x00});
  this->set_advertised_name_(this->remote_name_.c_str());
  this->wake_active_ = false;
  ESP_LOGI(TAG, "Completed XGIMI wake burst (256 rolling counter values)");
}


void XgimiRemote::dump_config() {
  ESP_LOGCONFIG(TAG,
                "XGIMI remote clone:\n"
                "  BLE HID name: %s\n"
                "  Wake advertisement name: %s\n"
                "  Wake token length: %u bytes\n"
                "  Tap timing: no deliberate delays\n"
                "  Immediate power-off hold: %u ms\n"
                "  Captured buttons: 19",
                this->remote_name_.c_str(), this->remote_name_.c_str(), 
                static_cast<unsigned>(this->wake_token_.size()),
                static_cast<unsigned>(IMMEDIATE_POWER_OFF_HOLD_MS));
}

void XgimiRemote::start_wake_burst() {
  if (!this->ble_ready_ || this->server_ == nullptr) {
    ESP_LOGW(TAG, "Cannot start wake burst before BLE is ready");
    return;
  }
  if (this->connected_) {
    ESP_LOGI(TAG, "Projector Power On ignored: HID link is already connected");
    return;
  }
  if (this->wake_active_) {
    ESP_LOGI(TAG, "Projector Power On ignored: wake burst is already in progress");
    return;
  }
  this->set_advertised_name_(WAKE_NAME);
  this->wake_counter_ = 0;
  this->wake_active_ = true;
  ESP_LOGI(TAG, "Starting XGIMI wake burst with no deliberate inter-value delay");
}

void XgimiRemote::request_power_off() {
  if (!this->connected_ || !this->authenticated_ || !this->is_keyboard_subscribed()) {
    ESP_LOGI(TAG, "Projector Power Off ignored: authenticated HID link is not ready");
    return;
  }
  ESP_LOGI(TAG, "Sending projector power-off tap");
  this->press_keyboard(0x7F);
}

void XgimiRemote::start_pairing_mode() {
  if (!this->ble_ready_ || this->server_ == nullptr) {
    ESP_LOGW(TAG, "Cannot start pairing mode before BLE is ready");
    return;
  }
  this->set_advertised_name_(this->remote_name_.c_str());
  this->server_->set_manufacturer_data({0x46, 0x00});
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
  uint8_t release[8] = {};
  this->notify_keyboard_(press);
  this->notify_keyboard_(release);
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

  uint8_t press[8] = {0x00, 0x00, usage, 0x00, 0x00, 0x00, 0x00, 0x00};
  this->notify_keyboard_(press);
  this->held_keyboard_usage_ = usage;
  this->held_keyboard_active_ = true;
  this->held_release_ms_ = millis() + IMMEDIATE_POWER_OFF_HOLD_MS;
  ESP_LOGI(TAG, "Holding keyboard usage 0x%02X for %u ms", usage,
           static_cast<unsigned>(IMMEDIATE_POWER_OFF_HOLD_MS));
}

void XgimiRemote::release_held_keyboard_() {
  uint8_t release[8] = {};
  this->notify_keyboard_(release);
  ESP_LOGI(TAG, "Released held keyboard usage 0x%02X", this->held_keyboard_usage_);
  this->held_keyboard_active_ = false;
  this->held_keyboard_usage_ = 0;
}

void XgimiRemote::press_consumer(uint16_t usage) {
  if (this->held_keyboard_active_) {
    ESP_LOGI(TAG, "Consumer tap 0x%04X ignored while held keyboard key 0x%02X is active", usage,
             this->held_keyboard_usage_);
    return;
  }
  uint8_t press[6] = {static_cast<uint8_t>(usage & 0xFF), static_cast<uint8_t>((usage >> 8) & 0xFF),
                      0x00, 0x00, 0x00, 0x00};
  uint8_t release[6] = {};
  this->notify_consumer_(press);
  this->notify_consumer_(release);
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
      if (this->wake_active_) {
        this->wake_active_ = false;
        this->server_->set_manufacturer_data({0x46, 0x00});
        this->set_advertised_name_(this->remote_name_.c_str());
        ESP_LOGI(TAG, "Stopped wake burst because the projector connected");
      }
      ESP_LOGI(TAG, "Projector HID client connected; requesting bonded encryption");
      const esp_err_t err = esp_ble_set_encryption(this->peer_address_, ESP_BLE_SEC_ENCRYPT);
      if (err != ESP_OK)
        ESP_LOGW(TAG, "Could not request BLE encryption: %s", esp_err_to_name(err));
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
        ESP_LOGI(TAG, "Projector HID client disconnected");
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
