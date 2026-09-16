#pragma once
#include "esphome.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <cJSON.h>

// --- CORE STRUCT DEFINITIONS ---
struct IRCommand {
  std::string name;
  esphome::button::Button* button_obj; 
};

struct IRProfile {
  std::string profile_name;
  std::string protocol;
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  std::map<uint32_t, IRCommand> cmd_codes; 
};

// --- GLOBALS ---
inline std::vector<IRProfile> remote_profiles;
inline bool flash_hydration_complete = false; 

// --- FIXED FLASH-SAVABLE STRUCT LAYOUTS (Max 5 profiles, 85 keys each) ---
struct FlashStoredKey {
  uint32_t hex_code; 
  char target_button_id[32]; 
};

struct FlashStoredProfile {
  char profile_name[32];
  char protocol[16];
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  uint16_t total_keys;
  FlashStoredKey keys[85]; 
};

#include <span> 

// Resolves an ESPHome button component reference from its clean text identifier string
inline esphome::button::Button* resolve_button(const std::string& name) {
  for (auto* btn : esphome::App.get_buttons()) {
    char buffer[128] = {0}; 
    std::span<char, 128> buf_span(buffer);
    esphome::StringRef id_ref = btn->get_object_id_to(buf_span);
    std::string internal_id(id_ref.c_str(), id_ref.size());
    
    if (internal_id == name) {
      ESP_LOGD("Linker", "Live IR Match! Bound string ID '%s' securely to component pointer.", name.c_str());
      return btn;
    }
  }
  return nullptr; 
}

inline size_t factory_count = 13; 

// Converts the dynamic vector into isolated chunks and commits them sequentially
inline void commit_database_to_flash() {
  uint16_t saved_count = 0;

  for (size_t i = factory_count; i < remote_profiles.size(); i++) {
    if (saved_count >= 5) break; 

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
    for (const auto& pair : runtime_p.cmd_codes) {
      if (k_idx >= 85) break; 
      flash_p.keys[k_idx].hex_code = pair.first;
      std::strncpy(flash_p.keys[k_idx].target_button_id, pair.second.name.c_str(), sizeof(flash_p.keys[k_idx].target_button_id) - 1);
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
  if (remote_profiles.size() < factory_count) {
    while (remote_profiles.size() < factory_count) {
      remote_profiles.push_back({ "Placeholder Layout", "NEC", 0, 0, 0, {} });
    }
  }

  int loaded_custom_count = 0;
  for (uint16_t slot = 0; slot < 5; slot++) {
    uint64_t slot_nvs_key = 1948204712ULL + slot;
    auto pref_obj = esphome::global_preferences->make_preference<FlashStoredProfile>(slot_nvs_key);
    
    static FlashStoredProfile flash_p;
    std::memset(&flash_p, 0, sizeof(flash_p));

    if (pref_obj.load(&flash_p)) {
      if (flash_p.total_keys == 0 || std::strlen(flash_p.profile_name) == 0) {
        continue; 
      }
      
      IRProfile restored_profile;
      restored_profile.profile_name = std::string(flash_p.profile_name);
      restored_profile.protocol = std::string(flash_p.protocol);
      restored_profile.device_address = flash_p.device_address;
      restored_profile.cmd_clear_token_arm = flash_p.cmd_clear_token_arm;
      restored_profile.cmd_clear_token_fire = flash_p.cmd_clear_token_fire;

      for (uint16_t k = 0; k < flash_p.total_keys; k++) {
        std::string b_id = std::string(flash_p.keys[k].target_button_id);
        restored_profile.cmd_codes[flash_p.keys[k].hex_code] = { b_id, nullptr };
      }

      size_t target_vector_index = factory_count + slot;
      while (remote_profiles.size() <= target_vector_index) {
        remote_profiles.push_back({ "Placeholder", "NEC", 0, 0, 0, {} });
      }

      remote_profiles[target_vector_index] = restored_profile;
      loaded_custom_count++;
    }
  }
  
  flash_hydration_complete = true;
}

inline void link_hardware_buttons() {
    for (size_t idx = 0; idx < remote_profiles.size(); idx++) {
        for (auto& pair : remote_profiles[idx].cmd_codes) {
            pair.second.button_obj = resolve_button(pair.second.name);
        }
    }
}

// Parses an incoming JSON file string using cJSON
inline bool import_profiles_from_json(const std::string& json_str) {
  cJSON *root_array = cJSON_Parse(json_str.c_str());
  if (root_array == nullptr) return false;

  if (!cJSON_IsArray(root_array)) {
    cJSON_Delete(root_array);
    return false;
  }

  if (remote_profiles.size() > factory_count) {
    remote_profiles.resize(factory_count);
  }

  cJSON *p_obj = nullptr;
  cJSON_ArrayForEach(p_obj, root_array) {
    cJSON *name = cJSON_GetObjectItemCaseSensitive(p_obj, "name");
    cJSON *protocol = cJSON_GetObjectItemCaseSensitive(p_obj, "protocol");
    cJSON *address = cJSON_GetObjectItemCaseSensitive(p_obj, "address");
    cJSON *clear_arm = cJSON_GetObjectItemCaseSensitive(p_obj, "clear_arm");
    cJSON *clear_fire = cJSON_GetObjectItemCaseSensitive(p_obj, "clear_fire");
    cJSON *keys = cJSON_GetObjectItemCaseSensitive(p_obj, "keys");

    if (!cJSON_IsString(name) || !cJSON_IsString(protocol) || !cJSON_IsString(address)) {
      continue;
    }

    IRProfile restored_p;
    restored_p.profile_name = name->valuestring;
    restored_p.protocol = protocol->valuestring;
    restored_p.device_address = std::stoul(address->valuestring, nullptr, 16);
    
    restored_p.cmd_clear_token_arm = (cJSON_IsString(clear_arm)) ? std::stoul(clear_arm->valuestring, nullptr, 16) : 0;
    restored_p.cmd_clear_token_fire = (cJSON_IsString(clear_fire)) ? std::stoul(clear_fire->valuestring, nullptr, 16) : 0;

    if (cJSON_IsArray(keys)) {
      cJSON *k_obj = nullptr;
      cJSON_ArrayForEach(k_obj, keys) {
        cJSON *code_obj = cJSON_GetObjectItemCaseSensitive(k_obj, "code");
        cJSON *btn_obj = cJSON_GetObjectItemCaseSensitive(k_obj, "button");

        if (cJSON_IsString(code_obj) && cJSON_IsString(btn_obj)) {
          uint32_t code = std::stoul(code_obj->valuestring, nullptr, 16);
          std::string b_id = btn_obj->valuestring;
          restored_p.cmd_codes[code] = { b_id, resolve_button(b_id) };
        }
      }
    }

    remote_profiles.push_back(restored_p);
  }

  cJSON_Delete(root_array);
  commit_database_to_flash();
  return true;
}
