#pragma once
#include "esphome.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <ArduinoJson.h>

// --- CORE STRUCT DEFINITIONS ---
struct IRCommand {
  std::string name;
  esphome::button::Button* button_obj; // Changed from TemplateButton* to base Button*
};


struct IRProfile {
  std::string profile_name;
  std::string protocol;
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  std::map<uint32_t, IRCommand> cmd_codes; // Optimized 32-bit container maps for Sony stability
};

// --- GLOBALS (Using 'inline' for safe multi-lambda cross-compilation) ---
inline std::vector<IRProfile> remote_profiles;
inline bool flash_hydration_complete = false; // <=== ADD THIS LINE HERE


// --- FIXED FLASH-SAVABLE STRUCT LAYOUTS (Max 5 profiles, 30 keys each) ---
struct FlashStoredKey {
  uint32_t hex_code; 
  char target_button_id[32]; 
};

// This fits perfectly within individual preference limits!
struct FlashStoredProfile {
  char profile_name[32];
  char protocol[16];
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  uint16_t total_keys;
  FlashStoredKey keys[85]; 
};

struct FlashDatabase {
  uint16_t total_uploaded_profiles;
  FlashStoredProfile profiles[5]; 
};

// --- HELPER & SYSTEM FUNCTIONS ---
#include <span> // Ensure span header is available

// Resolves an ESPHome button component reference from its clean text identifier string
inline esphome::button::Button* resolve_button(const std::string& name) {
  // Loop through all compiled button components registered on the device
  for (auto* btn : esphome::App.get_buttons()) {
    
    // Allocate the exact 128-byte footprint required by your ESPHome compiler base
    char buffer[128] = {0}; 
    std::span<char, 128> buf_span(buffer);
    
    // Capture the return value of get_object_id_to securely
    esphome::StringRef id_ref = btn->get_object_id_to(buf_span);
    
    // FIX: Use .c_str() and .size() to correctly initialize the standard string
    std::string internal_id(id_ref.c_str(), id_ref.size());
    
    // Strict comparison prevents alphabetical or prefix overflows between your token keys
    if (internal_id == name) {
      ESP_LOGD("Linker", "Live IR Match! Bound string ID '%s' securely to component pointer.", name.c_str());
      return btn;
    }
  }
  
  return nullptr; 
}



// Converts the dynamic vector into isolated chunks and commits them sequentially
inline void commit_database_to_flash() {
  uint16_t saved_count = 0;

  for (size_t i = 13; i < remote_profiles.size(); i++) {
    if (saved_count >= 5) break; 

    const auto& runtime_p = remote_profiles[i];
    
    // Generate the exact matching storage key for this specific slot index position
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
    
    // Save this clean, bite-sized profile object directly into its own NVS slot
    pref_obj.save(&flash_p);
    saved_count++;
  }
  
  // Explicitly request an immediate storage controller cache sync
  esphome::global_preferences->sync();
  ESP_LOGI("ir_hub", "Successfully synchronized %d custom profiles to isolated NVS tracks.", saved_count);
}


// Rehydrates dynamic custom slots out of individual Flash NVS blocks safely
inline void load_saved_flash_profiles() {
  // Step 1: Enforce fixed array padding rules BEFORE parsing NVS memory streams
  // This guarantees that if this runs, slots 0 through 12 exist as a baseline matrix
  if (remote_profiles.size() < 13) {
    ESP_LOGW("ir_hub", "Database structure safety check: Padding missing factory slots...");
    while (remote_profiles.size() < 13) {
      remote_profiles.push_back({ "Placeholder Layout", "NEC", 0, 0, 0, {} });
    }
  }

  // Step 2: Sequentially read learned layout profiles out of NVS storage slots
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

      // Securely lock custom configurations into indices 13 through 17
      size_t target_vector_index = 13 + slot;
      while (remote_profiles.size() <= target_vector_index) {
        remote_profiles.push_back({ "Placeholder", "NEC", 0, 0, 0, {} });
      }

      remote_profiles[target_vector_index] = restored_profile;
      loaded_custom_count++;
      ESP_LOGI("ir_hub", "NVS Slot [%d] successfully mapped to Database Index [%d]: %s", 
               slot, (int)target_vector_index, restored_profile.profile_name.c_str());
    }
  }
  
  ESP_LOGI("ir_hub", "Database hydration pass complete. Total profiles loaded: %zu (%d active custom configs)", 
           remote_profiles.size(), loaded_custom_count);
           
  flash_hydration_complete = true;
}



inline void link_hardware_buttons() {
    ESP_LOGI("Linker", "Beginning global layout pointer synchronization across all %d profiles...", (int)remote_profiles.size());

    // Iterate through EVERY profile layout currently residing in RAM
    for (size_t idx = 0; idx < remote_profiles.size(); idx++) {
        int bound_count = 0;
        
        for (auto& pair : remote_profiles[idx].cmd_codes) {
            pair.second.button_obj = resolve_button(pair.second.name);
            
            if (pair.second.button_obj != nullptr) {
                bound_count++;
            }
        }
        
        ESP_LOGD("Linker", "Profile [%zu] '%s': Successfully bound %d buttons.", 
                 idx, remote_profiles[idx].profile_name.c_str(), bound_count);
    }

    // Direct safety validation check for the currently selected active profile
    int current_idx = id(active_remote_layout);
    if (current_idx >= 0 && current_idx < (int)remote_profiles.size()) {
        ESP_LOGI("Linker", "Global pointer mapping complete. Active Profile active: %s", 
                 remote_profiles[current_idx].profile_name.c_str());
    } else {
        ESP_LOGE("Linker", "Global pointer mapping warning: Current active profile index [%d] is out of bounds!", current_idx);
    }
}



// Serializes dynamic profile slots (Index 13+) into a single JSON string format with Hexadecimal notation (V7 API)
inline std::string export_profiles_to_json() {
  JsonDocument doc; 
  JsonArray profiles_arr = doc.to<JsonArray>();

  char buf[32]; // Temporary scratchpad buffer for formatting numbers to hex text strings

  for (size_t i = 13; i < remote_profiles.size(); i++) {
    const auto& p = remote_profiles[i];
    JsonObject p_obj = profiles_arr.add<JsonObject>(); 
    p_obj["name"] = p.profile_name;
    p_obj["protocol"] = p.protocol;
    
    // Convert Device Address to Hexadecimal string
    snprintf(buf, sizeof(buf), "0x%04X", (unsigned int)p.device_address);
    p_obj["address"] = std::string(buf);
    
    // Convert Token configuration keys to Hexadecimal strings
    snprintf(buf, sizeof(buf), "0x%04X", (unsigned int)p.cmd_clear_token_arm);
    p_obj["clear_arm"] = std::string(buf);
    
    snprintf(buf, sizeof(buf), "0x%04X", (unsigned int)p.cmd_clear_token_fire);
    p_obj["clear_fire"] = std::string(buf);

    JsonArray keys_arr = p_obj["keys"].to<JsonArray>();
    for (const auto& pair : p.cmd_codes) {
      JsonObject k_obj = keys_arr.add<JsonObject>(); 
      
      // Convert individual remote key Hex commands to Hexadecimal strings
      snprintf(buf, sizeof(buf), "0x%04X", (unsigned int)pair.first);
      k_obj["code"] = std::string(buf);
      
      k_obj["button"] = pair.second.name;
    }
  }

  std::string output;
  serializeJson(doc, output);
  return output;
}


// Parses an incoming JSON file string and rehydrates your dynamic slots from Hexadecimal strings (V7 API)
inline bool import_profiles_from_json(const std::string& json_str) {
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, json_str);
  if (error) return false;

  JsonArray profiles_arr = doc.as<JsonArray>();
  
  if (remote_profiles.size() > 13) {
    remote_profiles.resize(13);
  }

  for (JsonObject p_obj : profiles_arr) {
    IRProfile restored_p;
    restored_p.profile_name = p_obj["name"].as<std::string>();
    restored_p.protocol = p_obj["protocol"].as<std::string>();
    
    // HEX STRING CONVERSION: Extract address as text string and convert base-16 to uint32_t
    std::string addr_str = p_obj["address"].as<std::string>();
    restored_p.device_address = std::stoul(addr_str, nullptr, 16);
    
    // HEX STRING CONVERSION: Extract token mappings as text strings and convert base-16 to uint32_t
    std::string clear_arm_str = p_obj["clear_arm"].as<std::string>();
    restored_p.cmd_clear_token_arm = std::stoul(clear_arm_str, nullptr, 16);
    
    std::string clear_fire_str = p_obj["clear_fire"].as<std::string>();
    restored_p.cmd_clear_token_fire = std::stoul(clear_fire_str, nullptr, 16);

    JsonArray keys_arr = p_obj["keys"].as<JsonArray>();
    for (JsonObject k_obj : keys_arr) {
      // HEX STRING CONVERSION: Extract unique code hex and parse string token matrix
      std::string code_str = k_obj["code"].as<std::string>();
      uint32_t code = std::stoul(code_str, nullptr, 16);
      
      std::string b_id = k_obj["button"].as<std::string>();
      restored_p.cmd_codes[code] = { b_id, resolve_button(b_id) };
    }

    remote_profiles.push_back(restored_p);
  }

  commit_database_to_flash();
  return true;
}

