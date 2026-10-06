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

  uint32_t now = millis();

  // 🎹 LIVE INJECTION: Continuous Keyboard report repeat generator (Every 15ms)
  if (this->held_keyboard_active_) {
    if (now - this->held_keyboard_last_tx_ >= 15) {
      uint8_t report[8] = {0x00, 0x00, this->held_keyboard_usage_, 0x00, 0x00, 0x00, 0x00, 0x00};
      this->notify_keyboard_(report);
      this->held_keyboard_last_tx_ = now;
    }
  }

  // 📺 LIVE INJECTION: Continuous Consumer report repeat generator (Every 15ms)
  if (this->held_consumer_active_) {
    if (now - this->held_consumer_last_tx_ >= 15) {
      uint16_t usage = this->held_consumer_usage_;
      uint8_t report[6] = {
        static_cast<uint8_t>(usage & 0xFF), 
        static_cast<uint8_t>((usage >> 8) & 0xFF), 
        0x00, 0x00, 0x00, 0x00
      };
      this->notify_consumer_(report);
      this->held_consumer_last_tx_ = now;
    }
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
    // Suppress spam logs during active programmatic bursts
    return;
  }
  this->keyboard_report_->set_value(std::vector<uint8_t>(data, data + 8));
  this->keyboard_report_->notify();
}

void XgimiRemote::notify_consumer_(const uint8_t data[6]) {
  if (this->server_ == nullptr || this->consumer_report_ == nullptr ||
      this->server_->get_connected_client_count() == 0) {
    return;
  }
  this->consumer_report_->set_value(std::vector<uint8_t>(data, data + 6));
  this->consumer_report_->notify();
}

void XgimiRemote::press_keyboard(uint8_t usage) {
  if (this->held_keyboard_active_) return;
  uint8_t press[8] = {0x00, 0x00, usage, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t release[8] = {};
  this->notify_keyboard_(press);
  this->notify_keyboard_(release);
}

void XgimiRemote::hold_keyboard(uint8_t usage) {
  if (!this->connected_ || !this->authenticated_ || !this->is_keyboard_subscribed()) return;
  if (this->held_keyboard_active_ && this->held_keyboard_usage_ == usage) return;

  this->held_keyboard_usage_ = usage;
  this->held_keyboard_last_tx_ = millis();
  this->held_keyboard_active_ = true;

  uint8_t press[8] = {0x00, 0x00, usage, 0x00, 0x00, 0x00, 0x00, 0x00};
  this->notify_keyboard_(press);
}

void XgimiRemote::release_held_keyboard_() {
  if (!this->held_keyboard_active_) return;
  uint8_t release[8] = {};
  this->notify_keyboard_(release);
  ESP_LOGD(TAG, "Released held keyboard usage 0x%02X", this->held_keyboard_usage_);
  this->held_keyboard_active_ = false;
  this->held_keyboard_usage_ = 0;
}

void XgimiRemote::press_consumer(uint16_t usage) {
  if (this->held_consumer_active_) return;
  uint8_t press[6] = {static_cast<uint8_t>(usage & 0xFF), static_cast<uint8_t>((usage >> 8) & 0xFF), 0x00, 0x00, 0x00, 0x00};
  uint8_t release[6] = {};
  this->notify_consumer_(press);
  this->notify_consumer_(release);
}

void XgimiRemote::hold_consumer(uint16_t usage) {
  if (!this->connected_ || !this->authenticated_ || !this->is_consumer_subscribed()) return;
  if (this->held_consumer_active_ && this->held_consumer_usage_ == usage) return;

  this->held_consumer_usage_ = usage;
  this->held_consumer_last_tx_ = millis();
  this->held_consumer_active_ = true;

  uint8_t press[6] = {static_cast<uint8_t>(usage & 0xFF), static_cast<uint8_t>((usage >> 8) & 0xFF), 0x00, 0x00, 0x00, 0x00};
  this->notify_consumer_(press);
}

void XgimiRemote::release_held_consumer_() {
  if (!this->held_consumer_active_) return;
  uint8_t release[6] = {};
  this->notify_consumer_(release);
  ESP_LOGD(TAG, "Released held consumer usage 0x%04X", this->held_consumer_usage_);
  this->held_consumer_active_ = false;
  this->held_consumer_usage_ = 0;
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
        this->held_consumer_active_ = false;
        this->held_consumer_usage_ = 0;
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
        if (this->authenticated_) {
          this->restore_hid_subscriptions_();
          
          esp_ble_conn_update_params_t conn_params;
          std::memcpy(conn_params.bda, this->peer_address_, sizeof(esp_bd_addr_t));
          
          conn_params.min_int = 0x06;      // Min Interval: 7.5ms
          conn_params.max_int = 0x0C;      // Max Interval: 15.0ms
          conn_params.latency = 0x00;      
          conn_params.timeout = 0x0190;    // 4-second link loss supervision timeout
          
          esp_err_t param_err = esp_ble_gap_update_conn_params(&conn_params);
          if (param_err == ESP_OK) {
            ESP_LOGI(TAG, "Hyper-speed low-latency BLE connection parameters loaded successfully.");
          } else {
            ESP_LOGW(TAG, "Could not update connection parameters: %s", esp_err_to_name(param_err));
          }
        }
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
