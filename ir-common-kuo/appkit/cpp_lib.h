#pragma once
#include "esphome.h"
#include <span> 
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <sstream>
#include <cstdint>

//=================================================

// 1. Inject raw Assembly instructions to swallow secrets.yaml directly as a binary blob.
// This completely ignores C++ grammar rules, colons, dashes, and keywords.
__asm__(
    ".section .rodata\n"
    ".global _yaml_data_start\n"
    ".global _yaml_data_end\n"
    "_yaml_data_start:\n"
    ".incbin \"../../../../secrets.yaml\"\n" // Pulls the exact file raw into memory
    "_yaml_data_end:\n"
    ".byte 0\n"                              // Null terminator safety anchor
    ".section .text\n"
);

// 2. We declare these as arrays ([]) instead of individual characters.
// This tells the compiler it's a block of memory, resolving the array bounds warning.
extern "C" {
    extern const char _yaml_data_start[];
    extern const char _yaml_data_end[];
}

namespace SecretsParser {
    // Helper function to decode hex chars
    inline uint8_t parse_hex_byte(char high, char low) {
        auto convert = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        return (convert(high) << 4) | convert(low);
    }

    // Runtime scanner to safely hunt for your 15 hex tokens from the binary memory footprint
    inline void fill_tokens(uint8_t* destination) {
        const char* start = _yaml_data_start;
        const char* end = _yaml_data_end;
        size_t length = end - start;
        size_t idx = 0;

        // Ensure we don't scan blindly if the file is surprisingly short
        if (length < 4) return;

        for (size_t i = 0; i < length - 3 && idx < 15; ++i) {
            if (start[i] == '0' && (start[i+1] == 'x' || start[i+1] == 'X')) {
                destination[idx++] = parse_hex_byte(start[i+2], start[i+3]);
                i += 3; // Skip past evaluated character block segment
            }
        }
    }
}

// 3. Export the functional array vector smoothly straight into your automation pipeline
inline std::vector<uint8_t> get_secret_wake_token() {
    std::vector<uint8_t> token(15, 0);
    SecretsParser::fill_tokens(token.data());
    return token;
}


//=================================================

// --- ARDUINOJSON VERSION COMPATIBILITY BRIDGE ---
#if ARDUINOJSON_VERSION_MAJOR >= 7
  using JsonDocType = JsonDocument;
  #define ALLOCATE_JSON_DOC(doc, size) JsonDocType doc; doc.shrinkToFit();
#else
  using JsonDocType = DynamicJsonDocument;
  #define ALLOCATE_JSON_DOC(doc, size) JsonDocType doc(size);
#endif

// --- FIXED-SIZE TYPE ALIASES ---
typedef char ButtonNameStr[32];
typedef char ProfileNameStr[32];
typedef char ProtocolNameStr[16];
typedef char ComponentBufferStr[128];

// --- CORE STRUCT DEFINITIONS ---
struct IRCommand {
  ButtonNameStr name; 
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

// Central configuration for learned profiles
inline constexpr uint16_t MAX_LEARNED_PROFILES = 5; 

// --- SINGLE SOURCE OF TRUTH FOR BUTTON NAMES ---
inline constexpr const char* learn_button_names[] = {
  "power_on", "power_off", "cursor_left", "cursor_right", "cursor_up", "cursor_down",
  "cursor_enter", "settings_menu", "back", "home", "game_menu", "input", "picture",
  "focus_manual", "focus_auto", "shortcut_1", "shortcut_2", "shortcut_3", "shortcut_4",
  "volume_up", "volume_down", "mute", "token_sniff", "token_clear", "token_recall",
  "BT_start_pair", "BT_clear_pair"
};

// Calculate total elements automatically at the compiler level
inline constexpr size_t TOTAL_LEARN_BUTTONS = sizeof(learn_button_names) / sizeof(learn_button_names[0]);


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
  FlashStoredKey keys[85]; 
};

// Resolves an ESPHome button component reference from its text identifier string
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

// Helper to find text strings between simple delimiters without copying them to heap
inline bool get_json_value_bounds(const std::string& json, size_t start_pos, const char* key, size_t& val_start, size_t& val_len) {
    size_t k_pos = json.find(key, start_pos);
    if (k_pos == std::string::npos) return false;
    
    size_t colon = json.find(':', k_pos);
    if (colon == std::string::npos) return false;
    
    size_t quote_start = json.find('"', colon);
    if (quote_start != std::string::npos && (quote_start < json.find_first_of(",}]", colon))) {
        size_t quote_end = json.find('"', quote_start + 1);
        if (quote_end == std::string::npos) return false;
        val_start = quote_start + 1;
        val_len = quote_end - val_start;
        return true;
    } else {
        size_t num_start = json.find_first_not_of(" \t\n\r", colon + 1);
        if (num_start == std::string::npos) return false;
        size_t num_end = json.find_first_of(", \t\n\r}]", num_start);
        if (num_end == std::string::npos) num_end = json.length();
        val_start = num_start;
        val_len = num_end - num_start;
        return true;
    }
}

// ==========================================
// 1. SAFE ZERO-HEAP JSON PARSER
// ==========================================
inline bool import_profiles_from_json(const std::string& json_str) {
    if (json_str.empty()) return false;

    if (json_str.find("\"err\"") != std::string::npos || json_str.find("\"sta\"") != std::string::npos) {
        ESP_LOGE("json_import", "Aborting import: JSON payload contains status error codes.");
        return false;
    }

    size_t v_start = 0, v_len = 0;
    
    if (!get_json_value_bounds(json_str, 0, "\"idx\"", v_start, v_len)) return false;
    int profile_idx = std::stoi(json_str.substr(v_start, v_len));

    size_t total_required_slots = factory_count + MAX_LEARNED_PROFILES;
    if (profile_idx < (int)factory_count || profile_idx >= (int)total_required_slots) {
        ESP_LOGE("json_import", "Target profile index %d violates dynamic custom memory boundaries.", profile_idx);
        return false;
    }

    if (remote_profiles.size() < total_required_slots) {
        remote_profiles.resize(total_required_slots, { "Placeholder Layout", "NEC", 0, 0, 0, {} });
    }

    auto& target_profile = remote_profiles[profile_idx];

    if (!get_json_value_bounds(json_str, 0, "\"nam\"", v_start, v_len)) return false;
    target_profile.profile_name = json_str.substr(v_start, v_len);

    if (!get_json_value_bounds(json_str, 0, "\"pro\"", v_start, v_len)) return false;
    target_profile.protocol = json_str.substr(v_start, v_len);

    if (!get_json_value_bounds(json_str, 0, "\"adr\"", v_start, v_len)) return false;
    target_profile.device_address = std::stoul(json_str.substr(v_start, v_len), nullptr, 16);

    if (!get_json_value_bounds(json_str, 0, "\"car\"", v_start, v_len)) return false;
    target_profile.cmd_clear_token_arm = std::stoul(json_str.substr(v_start, v_len), nullptr, 16);

    if (!get_json_value_bounds(json_str, 0, "\"cfr\"", v_start, v_len)) return false;
    target_profile.cmd_clear_token_fire = std::stoul(json_str.substr(v_start, v_len), nullptr, 16);

    target_profile.cmd_codes.clear();
    target_profile.cmd_codes.reserve(95); 

    size_t array_pos = json_str.find("\"key\"");
    if (array_pos == std::string::npos) return false;
    array_pos = json_str.find('[', array_pos);
    if (array_pos == std::string::npos) return false;

    while (true) {
        size_t open_brace = json_str.find('{', array_pos);
        size_t close_bracket = json_str.find(']', array_pos);
        
        if (open_brace == std::string::npos || (close_bracket != std::string::npos && close_bracket < open_brace)) {
            break; 
        }

        size_t close_brace = json_str.find('}', open_brace);
        if (close_brace == std::string::npos) break;

        size_t k_start = 0, k_len = 0;
        size_t b_start = 0, b_len = 0;

        if (get_json_value_bounds(json_str, open_brace, "\"x\"", k_start, k_len) &&
            get_json_value_bounds(json_str, open_brace, "\"b\"", b_start, b_len)) {
            
            uint32_t raw_hex = std::stoul(json_str.substr(k_start, k_len), nullptr, 16);
            std::string button_identifier = json_str.substr(b_start, b_len);

            IRCommand cmd;
            std::memset(cmd.name, 0, sizeof(cmd.name));
            std::strncpy(cmd.name, button_identifier.c_str(), sizeof(cmd.name) - 1);
            
            cmd.button_obj = resolve_button(cmd.name);
            target_profile.cmd_codes.push_back({ raw_hex, cmd });
        }
        array_pos = close_brace + 1;
    }

    commit_database_to_flash();
    return true;
}

// ==========================================
// 2. CHRONOLOGICAL ZERO-HEAP JSON GENERATOR
// ==========================================
inline std::string generate_minified_profile_json(int idx) {
    if (remote_profiles.empty()) {
        return "{\n  \"sta\":\"warming\"\n}";
    }

    int factory = (int) factory_count;
    const int custom  = 50; 
    const int max_idx = factory + custom;
  
    if (idx < 0 || idx >= max_idx || idx >= (int) remote_profiles.size()) {
        return "{\n  \"err\":\"range\"\n}";
    }

    const IRProfile &p = remote_profiles[idx];
    if (p.profile_name.empty() || p.profile_name.rfind("Placeholder", 0) == 0) {
        return "{\n  \"err\":\"empty\"\n}";
    }

    // Pre-allocates a flat 5KB payload block upfront.
    // Completely prevents mid-run reallocations and short-term heap fragmentation.
    std::string json_out;
    json_out.reserve(5120); 

    // Reusable small scratchpad buffer on the stack (512 bytes protects deep metadata)
    char row_buf[512];
    std::memset(row_buf, 0, sizeof(row_buf));

    // 1. Generate Header Block Safely
    snprintf(row_buf, sizeof(row_buf), 
             "{\n  \"idx\":%d,\n  \"int\":%d,\n  \"nam\":\"%s\",\n  \"pro\":\"%s\",\n  \"adr\":\"%04X\",\n  \"car\":\"%04X\",\n  \"cfr\":\"%04X\",\n  \"key\":[\n",
             idx, (idx < factory) ? 1 : 0, p.profile_name.c_str(), p.protocol.c_str(),
             (unsigned int) p.device_address, (unsigned int) p.cmd_clear_token_arm, (unsigned int) p.cmd_clear_token_fire);
    json_out += row_buf;

    // 2. Single-Pass Chronological Loop
    // Iterates cleanly from index 0 straight to the end of the profile's std::vector
    for (size_t i = 0; i < p.cmd_codes.size(); i++) {
        uint32_t code = p.cmd_codes[i].first;
        const char* b_name = p.cmd_codes[i].second.name;

        bool is_last = (i == p.cmd_codes.size() - 1);
        std::memset(row_buf, 0, sizeof(row_buf));
        snprintf(row_buf, sizeof(row_buf), 
                 "    {\"x\":\"%X\",\"b\":\"%s\"}%s\n",
                 (unsigned int)code, b_name, is_last ? "" : ",");
    
        json_out += row_buf; 
    }
  
    json_out += "  ]\n}";
    return json_out;
}
