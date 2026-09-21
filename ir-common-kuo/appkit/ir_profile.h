#pragma once
#include "esphome.h"
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <ArduinoJson.h> 

// --- ARDUINOJSON VERSION COMPATIBILITY BRIDGE ---
#if ARDUINOJSON_VERSION_MAJOR >= 7
  using JsonDocType = JsonDocument;
  #define ALLOCATE_JSON_DOC(doc, size) JsonDocType doc; doc.shrinkToFit();
#else
  using JsonDocType = DynamicJsonDocument;
  #define ALLOCATE_JSON_DOC(doc, size) JsonDocType doc(size);
#endif

// --- FIXED-SIZE TYPE ALIASES (Bypasses Markdown Bracket Stripping Completely) ---
typedef char ButtonNameStr[32];
typedef char ProfileNameStr[32];
typedef char ProtocolNameStr[16];
typedef char ComponentBufferStr[128];

// --- CORE STRUCT DEFINITIONS ---
struct IRCommand {
  ButtonNameStr name; // Fixed 32-byte char array via type alias
  esphome::button::Button* button_obj; 
};

struct IRProfile {
  std::string profile_name;
  std::string protocol;
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  std::vector<std::pair<uint32_t, IRCommand>> cmd_codes; 
};

// CENTRAL CONFIGURATION FOR LEARNED PROFILES
inline constexpr uint16_t MAX_LEARNED_PROFILES = 5; 

// --- GLOBALS ---
inline std::vector<IRProfile> remote_profiles;
inline bool flash_hydration_complete = false; 
inline size_t factory_count = 0; 

// --- FIXED FLASH-SAVABLE STRUCT LAYOUTS ---
struct FlashStoredKey {
  uint32_t hex_code; 
  ButtonNameStr target_button_id; 
};

struct FlashStoredProfile {
  ProfileNameStr profile_name;
  ProtocolNameStr protocol;
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  uint16_t total_keys;
  FlashStoredKey keys[85]; // Hardcoded directly for safety
};

#include <span> 

// Resolves an ESPHome button component reference from its clean text identifier string
inline esphome::button::Button* resolve_button(const char* name) {
  for (auto* btn : esphome::App.get_buttons()) {
    ComponentBufferStr buffer = {0}; 
    std::span<char, 128> buf_span(buffer);
    esphome::StringRef id_ref = btn->get_object_id_to(buf_span);
    
    if (std::strcmp(id_ref.c_str(), name) == 0) {
      ESP_LOGD("Linker", "Live IR Match! Bound string ID '%s' securely to component pointer.", name);
      return btn;
    }
  }
  return nullptr; 
}

// Converts the dynamic vector into isolated chunks and commits them sequentially
inline void commit_database_to_flash() {
  uint16_t saved_count = 0;

  for (size_t i = factory_count; i < remote_profiles.size(); i++) {
    if (saved_count >= MAX_LEARNED_PROFILES) break; 

    const auto& runtime_p = remote_profiles[i];
    uint64_t slot_nvs_key = 1948204712ULL + saved_count;
    auto pref_obj = esphome::global_preferences->make_preference<FlashStoredProfile>(slot_nvs_key);

    static FlashStoredProfile flash_p;
    std::memset(&flash_p, 0, sizeof(flash_p));

    std::strncpy(flash_p.profile_name, runtime_p.profile_name.c_str(), sizeof(flash_p.profile_name) - 1);
    std::strncpy(flash_p.protocol, runtime_p.protocol.c_str(), sizeof(flash_p.protocol) - 1);
    flash_p.device_address = runtime_p.device_address;
    flash_p.cmd_clear_token_arm = runtime_p.cmd_clear_token_arm;
    flash_p.cmd_clear_token_fire = runtime_p.cmd_clear_token_fire;

    uint16_t k_idx = 0;
    for (const auto& kv_pair : runtime_p.cmd_codes) {
      if (k_idx >= 85) break; 
      flash_p.keys[k_idx].hex_code = kv_pair.first;
      std::strncpy(flash_p.keys[k_idx].target_button_id, kv_pair.second.name, sizeof(flash_p.keys[k_idx].target_button_id) - 1);
      k_idx++;
    }
    flash_p.total_keys = k_idx;
    
    pref_obj.save(&flash_p);
    saved_count++;
  }
  
  esphome::global_preferences->sync();
  ESP_LOGI("ir_hub", "Successfully synchronized %d custom profiles to isolated NVS tracks.", saved_count);
}

// Rehydrates dynamic custom slots out of individual Flash NVS blocks safely
inline void load_saved_flash_profiles() {
  size_t max_total_capacity = factory_count + MAX_LEARNED_PROFILES;
  if (remote_profiles.size() < max_total_capacity) {
    remote_profiles.resize(max_total_capacity, { "Placeholder Layout", "NEC", 0, 0, 0, {} });
  }

  for (uint16_t slot = 0; slot < MAX_LEARNED_PROFILES; slot++) {
    uint64_t slot_nvs_key = 1948204712ULL + slot;
    auto pref_obj = esphome::global_preferences->make_preference<FlashStoredProfile>(slot_nvs_key);
    
    static FlashStoredProfile flash_p;
    std::memset(&flash_p, 0, sizeof(flash_p));

    if (pref_obj.load(&flash_p)) {
      if (flash_p.total_keys == 0 || std::strlen(flash_p.profile_name) == 0) {
        continue; 
      }
      
      size_t target_vector_index = factory_count + slot;
      auto& target_profile = remote_profiles[target_vector_index];

      target_profile.profile_name = flash_p.profile_name;
      target_profile.protocol = flash_p.protocol;
      target_profile.device_address = flash_p.device_address;
      target_profile.cmd_clear_token_arm = flash_p.cmd_clear_token_arm;
      target_profile.cmd_clear_token_fire = flash_p.cmd_clear_token_fire;

      // ANTI-FRAGMENTATION RESIZE: Wiping vector storage footprint directly to target size 
      // without using clear() + reserve(), which prevents dynamic mid-loop memory shifts.
      target_profile.cmd_codes.resize(flash_p.total_keys);

      for (uint16_t k = 0; k < flash_p.total_keys; k++) {
        auto& kv_pair = target_profile.cmd_codes[k];
        kv_pair.first = flash_p.keys[k].hex_code;
        
        std::strncpy(kv_pair.second.name, flash_p.keys[k].target_button_id, sizeof(kv_pair.second.name) - 1);
        kv_pair.second.name[sizeof(kv_pair.second.name) - 1] = '\0';
        kv_pair.second.button_obj = nullptr;
      }
    }
  }
  
  flash_hydration_complete = true;
}

inline void link_hardware_buttons() {
    for (size_t idx = 0; idx < remote_profiles.size(); idx++) {
        for (auto& kv_pair : remote_profiles[idx].cmd_codes) {
            kv_pair.second.button_obj = resolve_button(kv_pair.second.name);
        }
    }
}

// Parses an incoming JSON file string using safe, contiguous ArduinoJson structures
inline bool import_profiles_from_json(const std::string& json_str) {
  size_t dynamic_doc_size = json_str.length() + 1024; 
  ALLOCATE_JSON_DOC(doc, dynamic_doc_size);

  DeserializationError error = deserializeJson(doc, json_str);
  if (error) {
    ESP_LOGE("json_import", "JSON Deserialization failed: %s", error.c_str());
    return false;
  }

  JsonArray root_array = doc.as<JsonArray>();
  if (root_array.isNull()) {
    return false;
  }

  size_t profiles_to_import = root_array.size();
  size_t total_required_slots = factory_count + profiles_to_import;

  if (total_required_slots > (factory_count + MAX_LEARNED_PROFILES)) {
    ESP_LOGW("json_import", "Incoming payload profile count truncated to MAX_LEARNED_PROFILES.");
    total_required_slots = factory_count + MAX_LEARNED_PROFILES;
  }

  if (remote_profiles.size() < total_required_slots) {
    remote_profiles.resize(total_required_slots, { "Placeholder Layout", "NEC", 0, 0, 0, {} });
  } else if (remote_profiles.size() > total_required_slots) {
    remote_profiles.resize(total_required_slots);
  }

  size_t current_slot_idx = factory_count;

  for (JsonObject p_obj : root_array) {
    if (current_slot_idx >= total_required_slots) break;

    const char* name = p_obj["name"];
    const char* protocol = p_obj["protocol"];
    const char* address_str = p_obj["address"];

    if (!name || !protocol || !address_str) {
      continue;
    }

    auto& target_profile = remote_profiles[current_slot_idx];

    target_profile.profile_name = name;
    target_profile.protocol = protocol;
    target_profile.device_address = std::stoul(address_str, nullptr, 16);
    
    const char* clear_arm = p_obj["clear_arm"];
    const char* clear_fire = p_obj["clear_fire"];
    target_profile.cmd_clear_token_arm = (clear_arm) ? std::stoul(clear_arm, nullptr, 16) : 0;
    target_profile.cmd_clear_token_fire = (clear_fire) ? std::stoul(clear_fire, nullptr, 16) : 0;

    JsonArray keys_array = p_obj["keys"].as<JsonArray>();
    if (!keys_array.isNull()) {
      // Force resizing instantly to lock layout footprint 
      target_profile.cmd_codes.resize(keys_array.size());

      size_t k_idx = 0;
      for (JsonObject k_obj : keys_array) {
        const char* code_str = k_obj["code"];
        const char* btn_str = k_obj["button"];

        if (code_str && btn_str) {
          auto& kv_pair = target_profile.cmd_codes[k_idx];
          kv_pair.first = std::stoul(code_str, nullptr, 16);
          
          std::strncpy(kv_pair.second.name, btn_str, sizeof(kv_pair.second.name) - 1);
          kv_pair.second.name[sizeof(kv_pair.second.name) - 1] = '\0';
          kv_pair.second.button_obj = resolve_button(btn_str);
          k_idx++;
        }
      }
    }

    current_slot_idx++;
  }

  commit_database_to_flash();
  return true;
}
