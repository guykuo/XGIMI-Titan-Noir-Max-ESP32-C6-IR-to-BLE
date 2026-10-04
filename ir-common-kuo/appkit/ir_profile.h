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
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_private/esp_clk.h" 
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include <unordered_set>

#define STRINGIFY_MACRO(x) #x
#define TOSTRING_MACRO(x) STRINGIFY_MACRO(x)

// --- HUMAN-READABLE PROTOCOL FOOTPRINTS ---
#define PROTO_UNKNOWN   0
#define PROTO_NEC       1
#define PROTO_JVC       2
#define PROTO_SONY      3
#define PROTO_LG        4
#define PROTO_PANASONIC 5
#define PROTO_RC5       6
#define PROTO_RC6       7

static const char *const TAG_MAPS = "universal_hid_maps";

// Zero-heap translation helper function for logging statements
inline const char* to_string(uint8_t proto_id) {
  switch (proto_id) {
    case PROTO_NEC:       return "NEC";
    case PROTO_JVC:       return "JVC";
    case PROTO_SONY:      return "SONY";
    case PROTO_LG:        return "LG";
    case PROTO_PANASONIC: return "PANASONIC";
    case PROTO_RC5:       return "RC5";
    case PROTO_RC6:       return "RC6";
    default:              return "UNKNOWN";
  }
}

// Global tracking configuration variables
inline size_t factory_count = 13; 
inline constexpr uint16_t MAX_LEARNED_PROFILES = 3;
inline bool flash_hydration_complete = false;

#ifdef ENABLE_EXTRA_BUTTONS
  inline constexpr uint32_t CURRENT_PROFILE_VERSION = 1024; // Power user footprint track
#else
  inline constexpr uint32_t CURRENT_PROFILE_VERSION = 66; // <-- increment this change in NVRAM storage structures
#endif



// Single Source of Truth for Button Mapping Arrays
inline constexpr const char* learn_button_names[] = {
  "power_on", "power_off", "cursor_left", "cursor_right", "cursor_up", "cursor_down",
  "cursor_enter", "settings_menu", "back", "home", "game_menu", "input", "picture",
  "focus_manual", "focus_auto", "shortcut_1", "shortcut_2", "shortcut_3", "shortcut_4",
  "volume_up", "volume_down", "mute", "token_sniff", "token_clear", "token_recall",
  "macro_record", "macro_play"
  #ifdef ENABLE_EXTRA_BUTTONS
  ,"custom_1", "custom_2", "custom_3", "custom_4", "custom_5"
  #endif
};

inline constexpr size_t TOTAL_SYSTEM_BUTTONS = sizeof(learn_button_names) / sizeof(learn_button_names[0]);

// ====================================================================
// Macro storage STRUCTS
// ====================================================================
#define MAX_MACRO_STEPS    32  
#define MAX_BOUND_HOTKEYS  9  
#define CURRENT_MACRO_VERSION  8

static const size_t MAX_ACTION_STRING_LEN = 32;

struct UniversalMacroStep {
  char action_string[MAX_ACTION_STRING_LEN]; 
  uint8_t event_state;    // 0 = DOWN, 2 = UP                  
  uint16_t delay_ms;      // Pacing interval                  };
};

struct UniversalFlashMacro {
  uint32_t struct_version;
  uint16_t total_steps;
  UniversalMacroStep steps[MAX_MACRO_STEPS]; 
};

struct BindingPair {
    char action_string[MAX_ACTION_STRING_LEN]; // e.g., "shortcut_4" or "RKEY:51 down arrow"
    uint8_t  shared_macro_slot;                // Associated Macro Storage Slot ID
};

struct UniversalBindingRegistry {
    uint32_t struct_version;
    uint16_t total_bound_keys;
    BindingPair bindings[MAX_BOUND_HOTKEYS];
};




// ====================================================================
// ⚡ HIGH-SPEED MACRO CSV TRANSLATION STRINGS
// ====================================================================
inline const char* type_to_str(uint8_t type) {
    if (type == 0) return "KEYBOARD";
    if (type == 1) return "CONSUMER";
    return "TOKEN";
}

inline uint8_t str_to_type(const std::string& str) {
    if (str == "KEYBOARD") return 0;
    if (str == "CONSUMER") return 1;
    return 2;
}

inline const char* state_to_str(uint8_t state) {
    return (state == 0) ? "DOWN" : "UP";
}

inline uint8_t str_to_state(const std::string& str) {
    return (str == "DOWN") ? 0 : 2;
}



// ====================================================================
// ⚡ UNIFIED XGIMI BLUETOOTH HID USE-MAPPING TRANSLATION ENGINE
// ====================================================================
inline uint16_t resolve_action_to_true_hid(const char* name, uint8_t& out_type) {
    if (name == nullptr) return 0xFFFF;
    std::string action_name(name);

    // KEYBOARD RESOLUTION PATH (REPORT TYPE 0)
    if (action_name == "power_off")         { out_type = 0; return 0x7F; }
    else if (action_name == "cursor_up")    { out_type = 0; return 0x52; }
    else if (action_name == "cursor_down")  { out_type = 0; return 0x51; }
    else if (action_name == "cursor_left")  { out_type = 0; return 0x50; }
    else if (action_name == "cursor_right") { out_type = 0; return 0x4F; }
    else if (action_name == "cursor_enter") { out_type = 0; return 0x28; }
    else if (action_name == "settings_menu"){ out_type = 0; return 0x41; }
    else if (action_name == "back")          { out_type = 0; return 0x29; }
    else if (action_name == "home")          { out_type = 0; return 0x4A; }
    else if (action_name == "game_menu")     { out_type = 0; return 0x65; }
    else if (action_name == "focus_manual")  { out_type = 0; return 0x3D; }
    else if (action_name == "focus_auto")    { out_type = 0; return 0x44; }
    else if (action_name == "volume_up")    { out_type = 0; return 0x80; }
    else if (action_name == "volume_down")  { out_type = 0; return 0x81; }

    // CONSUMER RESOLUTION PATH (REPORT TYPE 1)
    else if (action_name == "input")        { out_type = 1; return 0x01BC; }
    else if (action_name == "picture")      { out_type = 1; return 0x0223; }
    else if (action_name == "shortcut_1")   { out_type = 1; return 0x021D; }
    else if (action_name == "shortcut_2")   { out_type = 1; return 0x021F; }
    else if (action_name == "shortcut_3")   { out_type = 1; return 0x0221; }
    else if (action_name == "shortcut_4")   { out_type = 1; return 0x0222; }
    else if (action_name == "mute")         { out_type = 1; return 0x01BD; }

    // WEB ESCAPE HATCH FOR CUSTOM INJECTED CODES (With Commentary Support)
    else if (action_name.rfind("RKEY:", 0) == 0) {
        out_type = 0;
        std::string raw_hex = action_name.substr(5);
        size_t space_pos = raw_hex.find(' ');
        if (space_pos != std::string::npos) {
            raw_hex = raw_hex.substr(0, space_pos);
        }
        return (uint16_t)std::strtoul(raw_hex.c_str(), nullptr, 16);
    }
    else if (action_name.rfind("RCON:", 0) == 0) {
        out_type = 1;
        std::string raw_hex = action_name.substr(5);
        size_t space_pos = raw_hex.find(' ');
        if (space_pos != std::string::npos) {
            raw_hex = raw_hex.substr(0, space_pos);
        }
        return (uint16_t)std::strtoul(raw_hex.c_str(), nullptr, 16);
    }
    
    out_type = 2; // Fallback if no string parameters match incoming inputs
    return 0xFFFF;
}

// Expose workspaces globally to fix compilation linkages
inline UniversalFlashMacro active_recording_buffer{};
inline UniversalBindingRegistry global_binding_registry{};

// Initialize actual storage space allocations safely across translational units
#ifdef DEFINE_GLOBAL_WORKSPACE_RESERVES
  UniversalFlashMacro active_recording_buffer{};
  UniversalBindingRegistry global_binding_registry{};
#endif

// ====================================================================
// DECODING AND ABSOLUTE CONVERSION ALGORITHMS
// ====================================================================
inline uint16_t encode_action_to_id(const char* name, uint8_t& out_type) {
    if (name == nullptr) return 0xFFFF;
    
    if (std::strncmp(name, "RKEY:", 5) == 0) {
        out_type = 0;
        std::string raw_hex(name + 5);
        size_t space_pos = raw_hex.find(' ');
        if (space_pos != std::string::npos) {
            raw_hex = raw_hex.substr(0, space_pos);
        }
        return (uint16_t)std::strtoul(raw_hex.c_str(), nullptr, 16);
    }
    if (std::strncmp(name, "RCON:", 5) == 0) {
        out_type = 1;
        std::string raw_hex(name + 5);
        size_t space_pos = raw_hex.find(' ');
        if (space_pos != std::string::npos) {
            raw_hex = raw_hex.substr(0, space_pos);
        }
        return (uint16_t)std::strtoul(raw_hex.c_str(), nullptr, 16);
    }
    
    out_type = 2; 
    for (size_t i = 0; i < TOTAL_SYSTEM_BUTTONS; i++) {
        if (std::strcmp(name, learn_button_names[i]) == 0) {
            return (uint16_t)i;
        }
    }
    return 0xFFFF;
}



inline const char* decode_idx_to_action_string(uint16_t action_idx) {
    if (action_idx < TOTAL_SYSTEM_BUTTONS) {
        return learn_button_names[action_idx];
    }
    return "unassigned";
}

inline bool is_action_allowed_as_macro_hotkey(const char* action_name) {
    if (action_name == nullptr) return false;

    // Hard Guard: Block utility actions from carrying macros to prevent infinite loops
    if (std::strcmp(action_name, "macro_record") == 0 || 
        std::strcmp(action_name, "macro_play") == 0) {
        return false;
    }
    return true;
}

inline uint8_t get_bound_macro_slot(const char* current_action_name) {
    if (current_action_name == nullptr) return 255;

    // 1. Run the guard first using the literal string
    if (!is_action_allowed_as_macro_hotkey(current_action_name)) {
        return 255; 
    }

    // 2. Perform the quick profile-agnostic string scan
    for (uint16_t i = 0; i < global_binding_registry.total_bound_keys; i++) {
        if (std::strcmp(global_binding_registry.bindings[i].action_string, current_action_name) == 0) {
            return global_binding_registry.bindings[i].shared_macro_slot;
        }
    }
    return 255; // Not a hotkey, let it pass through natively
}





typedef char action_stringType[MAX_ACTION_STRING_LEN]; // Field 2: Visible Xgimi Token or RKEY RCON (e.g., "game_menu")
typedef char button_nameType[32];   // Field 3: name of button on physical remote (e.g., "Cinema Master")
typedef char profile_nameType[32];
typedef char ComponentBufferStrType[128];

// ====================================================================
// 1. THE 3-FIELD ACTIVE RUNTIME ROW STRUCTURE
// ====================================================================
struct IRCommand {
  action_stringType action_string;  // Field 2: Visible Xgimi Token or RKEY RCON (e.g., "game_menu")
  button_nameType button_name;      // Field 3: name of button on physical remote (e.g., "Cinema Master")
};

struct IRProfile {
  std::string profile_name;
  uint8_t protocol;
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  // Field 1: The hex code acts as the lookup index paired to your 2 string elements
  std::vector<std::pair<uint32_t, IRCommand>> cmd_codes; 
};

// --- THE SINGLE RUNTIME RAM WORKSPACE ---
// This is the ONLY profile container that lives permanently on your workbench RAM
inline IRProfile active_profile_workspace;


// ====================================================================
// 2. THE THREE-FIELD FLASH PERSISTENCE LAYER (NVS REGISTER SETS)
// ====================================================================
struct FlashStoredKey {
  uint32_t irCommand; 
  action_stringType action_string; // Maps straight to Field 2 (Xgimi Action)
  button_nameType button_name;     // Maps straight to Field 3 (Hidden Comment)
};

struct FlashStoredProfile {
  uint32_t struct_version; 
  profile_nameType profile_name;
  uint8_t protocol;              
  uint8_t reserved_padding[3];   // Padding to maintain strict 32-bit alignment structure
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  uint16_t total_keys;
  FlashStoredKey keys[80];       // Centralized configuration ceiling cap
};






// ====================================================================
// 3. INTERNAL LINKER EXTRACTION HELPER (ROBUST SANITIZATION ENGINE)
// ====================================================================
inline esphome::button::Button* resolve_button(const char* name) {
  if (name == nullptr) return nullptr;

  // --- STEP 1: INITIAL LOWERCASE CONVERSION ---
  std::string target_str(name);
  std::transform(target_str.begin(), target_str.end(), target_str.begin(), 
                 [](unsigned char c){ return std::tolower(c); });

  // Since macro_record and macro_play are virtual state-machine triggers 
  // that do not have physical compiled button entities, exit quietly 
  // with a nullptr to completely stop loud error logs from generating.
  if (target_str == "macro_record" || target_str == "macro_play") {
    return nullptr;
  }

  // --- STEP 2: NON-ALPHANUMERIC STRIPPER WORKER ---
  // Normalizes formatting irregularities (like double __ or trailing _) 
  // by stripping text down to raw alphanumeric signatures.
  auto strip_to_raw_alphanumeric = [](const std::string& input) {
      std::string clean_output;
      clean_output.reserve(input.size());
      for (char c : input) {
          if (std::isalnum(static_cast<unsigned char>(c))) {
              clean_output += std::tolower(static_cast<unsigned char>(c));
          }
      }
      return clean_output;
  };

  std::string clean_target = strip_to_raw_alphanumeric(target_str);

  // --- STEP 3: SCAN AND MATCH COMPILED ENTITIES ---
  for (auto* btn : esphome::App.get_buttons()) {
    ComponentBufferStrType buffer = {0}; 
    std::span<char, 128> buf_span(buffer);
    esphome::StringRef id_ref = btn->get_object_id_to(buf_span);
    
    // Normalize the compiled hardware identifier from the core engine loop
    std::string clean_hardware = strip_to_raw_alphanumeric(id_ref.c_str());
    
    // Evaluate stripped variants safely (e.g., "cursorright" == "cursorright")
    if (clean_target == clean_hardware) {
      ESP_LOGD("IR_LINKER", "Resolved: Flash Data '%s' matched Hardware Button '%s'", name, id_ref.c_str());
      return btn;
    }
  }
  
  ESP_LOGE("IR_LINKER", "Failed to resolve button: %s. (Cleaned target was: '%s')", name, clean_target.c_str());
  return nullptr; 
}


// ====================================================================
// 4. LIGHTWEIGHT INITIALIZATION ROW BUILDER HELPER
// ====================================================================
inline void add_cmd(uint32_t irCommand, const char* xgimi_id, const char* comment) {
    IRCommand cmd;
    std::strncpy(cmd.action_string, xgimi_id, sizeof(cmd.action_string) - 1);
    cmd.action_string[sizeof(cmd.action_string) - 1] = '\0';
    
    std::strncpy(cmd.button_name, comment, sizeof(cmd.button_name) - 1);
    cmd.button_name[sizeof(cmd.button_name) - 1] = '\0';
    active_profile_workspace.cmd_codes.push_back({ irCommand, cmd });
}

// ====================================================================
// 5. DATABASE INPUT/OUTPUT FLASH PERSISTENCE MANAGEMENT ROUTINES
// ====================================================================

// Commits the current active custom layout straight to an isolated NVS slot tracking track
inline void commit_database_to_flash(uint16_t target_slot) {
  if (target_slot >= MAX_LEARNED_PROFILES) return;

  uint64_t slot_nvs_key = 1948204712ULL + target_slot;
  auto pref_obj = esphome::global_preferences->make_preference<FlashStoredProfile>(slot_nvs_key);

  static FlashStoredProfile flash_p;
  std::memset(&flash_p, 0, sizeof(flash_p));
  flash_p.struct_version = CURRENT_PROFILE_VERSION; //struct layout signature version
  
  std::strncpy(flash_p.profile_name, active_profile_workspace.profile_name.c_str(), sizeof(flash_p.profile_name) - 1);
  flash_p.protocol = active_profile_workspace.protocol; 
  flash_p.device_address = active_profile_workspace.device_address;
  flash_p.cmd_clear_token_arm = active_profile_workspace.cmd_clear_token_arm;
  flash_p.cmd_clear_token_fire = active_profile_workspace.cmd_clear_token_fire;

  uint16_t k_idx = 0;
  for (const auto& kv_pair : active_profile_workspace.cmd_codes) {
    if (k_idx >= 80) break; // increased to 80
    flash_p.keys[k_idx].irCommand = kv_pair.first;
    std::strncpy(flash_p.keys[k_idx].action_string, kv_pair.second.action_string, sizeof(flash_p.keys[k_idx].action_string) - 1);
    std::strncpy(flash_p.keys[k_idx].button_name, kv_pair.second.button_name, sizeof(flash_p.keys[k_idx].button_name) - 1);
    k_idx++;
  }
  flash_p.total_keys = k_idx;
  pref_obj.save(&flash_p);
  esphome::global_preferences->sync();
}

// ====================================================================

// Flash memory optimized layout item (12 bytes total per row)
struct FlashCommandRow {
  uint32_t irCommand;
  const char* action_string; // 4-byte flash address pointer
  const char* button_name;   // 4-byte flash address pointer
};

// --- PROGMEM FACTORY DATA STORAGE TABLES (SINGLE-ITEM-PER-LINE) ---

alignas(4) const FlashCommandRow AWOL_COMMANDS[] {
  { 0x1AE5, "power_on",       "power on" },
  { 0x19E6, "power_off",      "power off" },
  { 0xDB24, "cursor_left",    "left arrow" },
  { 0xDA25, "cursor_right",   "right arrow" },
  { 0xD926, "cursor_up",      "up arrow" },
  { 0xD827, "cursor_down",    "down arrow" },
  { 0xD728, "cursor_enter",   "ok" },
  { 0xA55A, "settings_menu",  "menu" },
  { 0xA35C, "back",           "back" },
  { 0xA45B, "home",           "home" },
  { 0x23DC, "game_menu",      "Netflix" },
  { 0xED12, "input",          "input select" },
  { 0x1EE1, "picture",        "Google Assist" },
  { 0x25DA, "focus_manual",   "AI Box" },
  { 0x22DD, "focus_auto",     "Prime Video" },
  { 0x18E7, "shortcut_1",     "HDMI 1" },
  { 0x17E8, "shortcut_2",     "HDMI 2" },
  { 0x16E9, "shortcut_3",     "HDMI 3" },
  { 0x1DE2, "shortcut_4",     "Live TV" },
  { 0xF50A, "volume_up",      "vol up" },
  { 0xF40B, "volume_down",    "vol down" },
  { 0xE41B, "mute",           "mute" },
  { 0x21DE, "token_sniff",    "Disney" },
  { 0x97,   "token_clear",    "none" },
  { 0x97,   "token_recall",   "none" },
  { 0x27D8, "macro_record",   "select profile" },
  { 0x24DB, "macro_play",     "Youtube" }
};

alignas(4) const FlashCommandRow BENQ_COMMANDS[] {
  { 0xB04F, "power_on",       "power on" },
  { 0xB14E, "power_off",      "power off" },
  { 0xF40B, "cursor_up",      "up arrow" },
  { 0xF30C, "cursor_down",    "down arrow" },
  { 0xF20D, "cursor_left",    "left arrow" },
  { 0xF10E, "cursor_right",   "right arrow" },
  { 0xEA15, "cursor_enter",   "ok" },
  { 0xF00F, "settings_menu",  "menu" },
  { 0x7A85, "back",           "back" },
  { 0x8778, "home",           "default" },
  { 0x41BE, "game_menu",      "cinema master" },
  { 0xFB04, "input",          "source" },
  { 0xEF10, "picture",        "picure mode" },
  { 0xEC13, "focus_manual",   "aspect" },
  { 0xF708, "focus_auto",     "auto" },
  { 0xE916, "shortcut_1",     "brightness" },
  { 0xEE11, "shortcut_2",     "contrast" },
  { 0x837C, "shortcut_3",     "dynamic iris" },
  { 0xCF30, "shortcut_4",     "light mode" },
  { 0xA15E, "volume_up",      "gamma" },
  { 0x817E, "volume_down",    "sharp" },
  { 0xF807, "mute",           "eco blank" },
  { 0xC33C, "token_sniff",    "HDR" },
  { 0x629D, "token_clear",    "invert" },
  { 0x639C, "token_recall",   "3D" },
  { 0xA05F, "macro_record",   "color temp" },
  { 0xA45B, "macro_play",     "color manage" },
  { 0x6B94, "home",           "test pattern" }
};

alignas(4) const FlashCommandRow EPSON_COMMANDS[]{
  { 0x6F90, "power_on",       "Power On" },
  { 0x6E91, "power_off",      "Power Off" },
  { 0x4FB0, "cursor_up",      "up arrow" },
  { 0x4DB2, "cursor_down",    "down arrow" },
  { 0x4CB3, "cursor_left",    "left arrow" },
  { 0x4EB1, "cursor_right",   "right arrow" },
  { 0x7A85, "cursor_enter",   "enter" },
  { 0x659A, "settings_menu",  "menu" },
  { 0x7B84, "back",           "ESC" },
  { 0xC639, "home",           "default" },
  { 0x708F, "game_menu",      "color mode" },
  { 0xA956, "input",          "HDMI link" },
  { 0x55AA, "picture",        "image enhance" },
  { 0xA25D, "focus_manual",   "skip back" },
  { 0x728D, "focus_manual",   "lens NH" },
  { 0xA45B, "focus_auto",     "pause" },
  { 0xA55A, "shortcut_1",     "reverse" },
  { 0xA15E, "shortcut_2",     "play" },
  { 0xA35C, "shortcut_3",     "FF" },
  { 0xA05F, "shortcut_4",     "skip forward" },
  { 0x8C73, "shortcut_1",     "HDMI 1" },
  { 0x8877, "shortcut_2",     "HDMI 2" },
  { 0x7D82, "shortcut_3",     "P-in-P NH" },
  { 0x629D, "shortcut_4",     "PC NH" },
  { 0x6798, "volume_up",      "volume up" },
  { 0x6699, "volume_down",    "volume down" },
  { 0x52AD, "mute",           "mute" },
  { 0x7C83, "token_sniff",    "frame interp" },
  { 0xC23D, "token_clear",    "RGBCMY" },
  { 0x6996, "token_recall",   "pattern" },
  { 0xC43B, "macro_record",   "3D format" },
  { 0x758A, "macro_play",     "Aspect" },
  { 0x6A95, "home",           "Home" },
  { 0x8B74, "home",           "input LAN" },
  { 0x609F, "home",           "user" },
  { 0x9F60, "home",           "link menu" },
  { 0x6C93, "home",           "blank NH" },
  { 0x748B, "home",           "memory NH" },
  { 0x51AE, "home",           "lens1 NH" },
  { 0x50AF, "home",           "lens2 NH" }
};

alignas(4) const FlashCommandRow HISENSE_COMMANDS[] {
  { 0xF708, "power_on",       "power on" },
  { 0x8E71, "power_on",       "power on" },
  { 0xEF10, "power_off",      "power off" },
  { 0x8D72, "power_off",      "0" },
  { 0xA956, "cursor_up",      "arrow up" },
  { 0xA857, "cursor_down",    "arrow down" },
  { 0xA758, "cursor_left",    "arrow left" },
  { 0xA659, "cursor_right",   "arrow right" },
  { 0xA55A, "cursor_enter",   "select" },
  { 0xFB04, "back",           "back" },
  { 0xBC43, "home",           "home" },
  { 0xF40B, "input",          "input" },
  { 0xB54A, "settings_menu",  "menu" },
  { 0x718E, "settings_menu",  "menu" },
  { 0x35CA, "game_menu",      "apps" },
  { 0xFF00, "picture",        "channel up" },
  { 0xE817, "focus_manual",   "7" },
  { 0xE718, "focus_auto",     "8" },
  { 0xAB54, "shortcut_1",     "yellow" },
  { 0xAA55, "shortcut_2",     "blue" },
  { 0xAD52, "shortcut_3",     "red" },
  { 0xAC53, "shortcut_4",     "green" },
  { 0xFD02, "volume_up",      "volume up" },
  { 0xFC03, "volume_down",    "volume down" },
  { 0xF609, "mute",           "mute" },
  { 0xEB14, "token_sniff",    "4" },
  { 0xEA15, "token_clear",    "5" },
  { 0xE916, "token_recall",   "6" },
  { 0xB847, "macro_record",   "Prime Video" },
  { 0xB649, "macro_play",     "Youtube" }
};

alignas(4) const FlashCommandRow JVC_VCR_COMMANDS[] {
  { 0xC2D0, "power_on",       "power on" },
  { 0xC2B8, "power_on",       "power on" },
  { 0xC258, "power_off",      "power off" },
  { 0xC2E8, "power_off",      "audio monitor" },
  { 0xC2CC, "power_off",      "0" },
  { 0xC2C3, "back",           "review" },
  { 0xC241, "cursor_up",      "cursor up" },
  { 0xC298, "cursor_up",      "cursor up H" },
  { 0xC218, "cursor_down",    "cursor down" },
  { 0xC261, "cursor_down",    "cursor down H" },
  { 0xC2A8, "cursor_left",    "cursor left" },
  { 0xC228, "cursor_right",   "cursor right H" },
  { 0xC23C, "cursor_enter",   "OK" },
  { 0xC2EC, "settings_menu",  "menu" },
  { 0xC207, "settings_menu",  "memu" },
  { 0xC26C, "home",           "cancel" },
  { 0xC230, "game_menu",      "game menu" },
  { 0xC2C8, "input",          "tv/vcr" },
  { 0xC260, "picture",        "fast forward" },
  { 0xC214, "focus_manual",   "8" },
  { 0xC2E4, "focus_auto",     "7" },
  { 0xC283, "shortcut_1",     "prog" },
  { 0xC2BC, "shortcut_2",     "prog check" },
  { 0xC28C, "shortcut_3",     "SP/EP" },
  { 0xC269, "shortcut_4",     "skip search" },
  { 0xC213, "volume_up",      "start down" },
  { 0xC293, "volume_down",    "start up" },
  { 0xC2B0, "mute",           "pause" },
  { 0xC224, "token_sniff",    "4" },
  { 0xC2A4, "token_clear",    "5" },
  { 0xC264, "token_recall",   "6" },
  { 0xC284, "macro_record",   "1" },
  { 0xC244, "macro_play",     "2" }
};

alignas(4) const FlashCommandRow JVC_PROJ_A_COMMANDS[] {
  { 0xA0, "power_on",       "power on" },
  { 0x60, "power_off",      "power off" },
  { 0x80, "cursor_up",      "up arrow" },
  { 0x40, "cursor_down",    "down arrow" },
  { 0x6C, "cursor_left",    "left arrow" },
  { 0x2C, "cursor_right",   "right arrow" },
  { 0xF4, "cursor_enter",   "enter/ok" },
  { 0x74, "settings_menu",  "menu" },
  { 0xC0, "back",           "exit" },
  { 0xB8, "home",           "hide" },
  { 0xD6, "game_menu",      "dynamic" },
  { 0xCE, "game_menu",      "advanced menu" },
  { 0x0E, "input",          "input HDMI 1" },
  { 0x8E, "picture",        "input HDMI 2" },
  { 0x2F, "picture",        "picture mode" },
  { 0xCC, "focus_manual",   "focus -" },
  { 0x8C, "focus_auto",     "focus +" },
  { 0x11, "focus_manual",   "color profile" },
  { 0xAF, "focus_auto",     "gamma settings" },
  { 0x36, "shortcut_1",     "user 1" },
  { 0xB6, "shortcut_2",     "user 2" },
  { 0x76, "shortcut_3",     "user 3" },
  { 0xEE, "shortcut_4",     "aspect" },
  { 0x1B, "shortcut_1",     "mode 1" },
  { 0x9B, "shortcut_2",     "mode 2" },
  { 0x5B, "shortcut_3",     "mode 3" },
  { 0x2E, "shortcut_4",     "info" },
  { 0x5E, "volume_up",      "brightness up" },
  { 0xDE, "volume_down",    "brightness down" },
  { 0x6E, "mute",           "color temp" },
  { 0x04, "volume_up",      "lens AP" },
  { 0x0C, "volume_down",    "lens control_" },
  { 0xA3, "mute",           "anamorphic" },
  { 0x16, "token_sniff",    "cinema" },
  { 0x96, "token_sniff",    "cinema" },
  { 0x56, "token_clear",    "natural" },
  { 0xAE, "token_recall",   "gamma" },
  { 0xB7, "token_recall",   "HDR" },
  { 0xFE, "macro_record",   "sharp down" },
  { 0x9A, "macro_play",     "sharp up" },
  { 0x51, "macro_record",   "CMD" },
  { 0x0F, "macro_play",     "mpc" },
  { 0x3E, "home",           "color up" },
  { 0xBE, "home",           "color down" },
  { 0x1E, "home",           "contast up" },
  { 0x9E, "home",           "contrast down" },
  { 0x36, "home",           "test" },
  { 0xAC, "home",           "zoom T" },
  { 0xEC, "home",           "zoom W" },
  { 0x6B, "home",           "3D format" },
  { 0x4E, "home",           "pic adjust" }
};

alignas(4) const FlashCommandRow JVC_PROJ_B_COMMANDS[] {
  { 0xA0, "power_on",       "power on" },
  { 0x60, "power_off",      "power off" },
  { 0x80, "cursor_up",      "up arrow" },
  { 0x40, "cursor_down",    "down arrow" },
  { 0x6C, "cursor_left",    "left arrow" },
  { 0x2C, "cursor_right",   "right arrow" },
  { 0xF4, "cursor_enter",   "enter" },
  { 0x74, "settings_menu",  "menu" },
  { 0xC0, "back",           "exit" },
  { 0xB8, "home",           "hide" },
  { 0xD6, "game_menu",      "dynamic" },
  { 0xCE, "game_menu",      "advanced menu_" },
  { 0x0E, "input",          "input HDMI 1" },
  { 0x8E, "picture",        "input HDMI 2" },
  { 0x2F, "picture",        "picture mode_" },
  { 0xCC, "focus_manual",   "focus -" },
  { 0x8C, "focus_auto",     "focus +" },
  { 0x11, "focus_manual",   "color profile" },
  { 0xAF, "focus_auto",     "gamma settings" },
  { 0x36, "shortcut_1",     "user 1" },
  { 0xB6, "shortcut_2",     "user 2" },
  { 0x76, "shortcut_3",     "user 3" },
  { 0xEE, "shortcut_4",     "aspect" },
  { 0x1B, "shortcut_1",     "mode 1_" },
  { 0x9B, "shortcut_2",     "mode 2_" },
  { 0x5B, "shortcut_3",     "mode 3_" },
  { 0x2E, "shortcut_4",     "info" },
  { 0x5E, "volume_up",      "brightness up" },
  { 0xDE, "volume_down",    "brightness down" },
  { 0x6E, "mute",           "color temp" },
  { 0x04, "volume_up",      "lens AP_" },
  { 0x0C, "volume_down",    "lens control_" },
  { 0xA3, "mute",           "anamorphic_" },
  { 0x16, "token_sniff",    "cinema" },
  { 0x96, "token_sniff",    "cinema" },
  { 0x56, "token_clear",    "natural" },
  { 0xAE, "token_recall",   "gamma" },
  { 0xB7, "token_recall",   "HDR" },
  { 0xFE, "macro_record",   "sharp down" },
  { 0x9A, "macro_play",     "sharp up" },
  { 0x51, "macro_record",   "CMD" },
  { 0x0F, "macro_play",     "mpc" },
  { 0x3E, "home",           "color up" },
  { 0xBE, "home",           "color down" },
  { 0x1E, "home",           "contast up" },
  { 0x9E, "home",           "contrast down" },
  { 0x36, "home",           "test" },
  { 0xAC, "home",           "zoom T" },
  { 0xEC, "home",           "zoom W" },
  { 0x6B, "home",           "3D format" },
  { 0x4E, "home",           "pic adjust" }
};

alignas(4) const FlashCommandRow LG_COMMANDS[] {
  { 0xF708, "power_on",       "power toggle" },
  { 0x23DC, "power_on",       "Power On" },
  { 0x2CC3, "power_off",      "Power Off" },
  { 0xEF10, "power_off",      "0" },
  { 0xD728, "back",           "Return" },
  { 0xF807, "cursor_left",    "cursor left" },
  { 0xF906, "cursor_right",   "cursor right" },
  { 0xBF40, "cursor_up",      "cursor up" },
  { 0xBE41, "cursor_down",    "cursor down" },
  { 0xBB44, "cursor_enter",   "select" },
  { 0xBC43, "settings_menu",  "menu" },
  { 0x837C, "home",           "home" },
  { 0xF40B, "input",          "input toggle" },
  { 0xFE01, "game_menu",      "channel down" },
  { 0xB24D, "picture",        "picture mode" },
  { 0x8679, "focus_manual",   "aspect ratio" },
  { 0xA35C, "focus_auto",     "prime video" },
  { 0x8d72, "shortcut_1",     "red" },
  { 0x8e71, "shortcut_2",     "green" },
  { 0x9C63, "shortcut_3",     "yellow" },
  { 0x9E61, "shortcut_4",     "blue" },
  { 0xFD02, "volume_up",      "volume up" },
  { 0xFC03, "volume_down",    "volume down" },
  { 0xF609, "mute",           "mute" },
  { 0xEB14, "token_sniff",    "4" },
  { 0xEA15, "token_clear",    "5" },
  { 0xE916, "token_recall",   "6" },
  { 0xA956, "macro_record",   "Netflix" },
  { 0x4FB0, "macro_play",     "play" },
  { 0xEE11, "home",           "1" },
  { 0xED12, "home",           "2" },
  { 0xEC13, "home",           "3" },
  { 0xE817, "home",           "7" },
  { 0xE718, "home",           "8" },
  { 0xE619, "home",           "9" },
  { 0x54AB, "home",           "ch_list" },
  { 0xB34C, "home",           "-" },
  { 0xE51A, "home",           "pre-ch" },
  { 0xF10E, "home",           "sleep" },
  { 0xF30C, "home",           "portal" },
  { 0xC639, "home",           "cc" }
};

alignas(4) const FlashCommandRow OPTOMA_COMMANDS[] {
  { 0xFD02, "power_on",       "power on" },
  { 0xD12E, "power_off",      "power off" },
  { 0xEF10, "cursor_left",    "left arrow" },
  { 0xEC12, "cursor_right",   "right arrow" },
  { 0xEE11, "cursor_up",      "up arrow" },
  { 0xEB14, "cursor_down",    "down arrow" },
  { 0xF00F, "cursor_enter",   "ok" },
  { 0xF10E, "settings_menu",  "menu" },
  { 0x9C63, "back",           "sleep" },
  { 0xE916, "input",          "input HDMI 1" },
  { 0xCF30, "game_menu",      "input HDMI 2" },
  { 0xFA05, "picture",        "mode" },
  { 0x9B64, "focus_manual",   "aspect" },
  { 0xBB44, "focus_auto",     "DB" },
  { 0xE41B, "shortcut_1",     "input VGA 1" },
  { 0xE11E, "shortcut_2",     "input VGA 2" },
  { 0xE31C, "shortcut_3",     "input video" },
  { 0xE817, "shortcut_4",     "input YPbPr" },
  { 0x7689, "volume_up",      "3D" },
  { 0xF807, "volume_down",    "keystone" },
  { 0xAD52, "mute",           "mute" },
  { 0xC936, "token_sniff",    "user 1" },
  { 0x9A65, "token_clear",    "user 2" },
  { 0x9966, "token_recall",   "user 3" },
  { 0xBE41, "macro_record",   "brightness" },
  { 0xBD42, "macro_play",     "contrast" }
};

alignas(4) const FlashCommandRow SONY_PROJ_COMMANDS[] {
  { 0x03A2A, "power_on",      "Power On" },
  { 0x07A2A, "power_off",     "Power Off" },
  { 0x0542A, "power_on",      "Power Toggle" },
  { 0x0562A, "cursor_up",     "up arrow" },
  { 0x0362A, "cursor_down",   "down arrow" },
  { 0x0162A, "cursor_left",   "left arrow" },
  { 0x0662A, "cursor_right",  "right arrow" },
  { 0x02D2A, "cursor_enter",  "OK / Enter" },
  { 0x04A2A, "settings_menu", "Menu" },
  { 0x06F2A, "home",          "Reset" },
  { 0x18BE4, "back",          "Position" },
  { 0x6AB54, "game_menu",     "Game" },
  { 0xEAB54, "picture",       "Photo" },
  { 0x0752A, "input",         "Input" },
  { 0x26B54, "focus_manual",  "Focus" },
  { 0x46B54, "focus_auto",    "Zoom" },
  { 0x76B54, "shortcut_1",    "aspect ratio" },
  { 0x0502A, "shortcut_2",    "motion flow" },
  { 0xDCB54, "shortcut_3",    "3D" },
  { 0xD2B54, "shortcut_4",    "color Space" },
  { 0x00C2A, "volume_up",     "contrast" },
  { 0x04C2A, "volume_down",   "contrast down" },
  { 0xFAB54, "mute",          "advanced iris" },
  { 0x9AB54, "token_sniff",   "BRT Cinema" },
  { 0x8AB54, "token_clear",   "BRT TV" },
  { 0x2AB54, "token_recall",  "User" },
  { 0x07C2A, "macro_record",  "brightness down" },
  { 0x03C2A, "macro_play",    "brightness up" },
  { 0x3AB54, "home",          "color temp" },
  { 0x0702A, "home",          "contrast enhancer" },
  { 0xCAB54, "home",          "film 1" },
  { 0x1AB54, "home",          "film 2" },
  { 0x7AB54, "home",          "gamma Corr" },
  { 0x06A2A, "home",          "input HDMI 1" },
  { 0x01A2A, "home",          "input HDMI 2" },
  { 0x04BE4, "home",          "position 1.85" },
  { 0x84BE4, "home",          "position 2.35" },
  { 0xC4BE4, "home",          "position Custom 2" },
  { 0x24BE4, "home",          "position Custom 3" },
  { 0x32B54, "home",          "reality creation" },
  { 0xAAB54, "home",          "REF" },
  { 0x0622A, "home",          "sharpness down" },
  { 0x0222A, "home",          "sharpness up" },
  { 0xC6B54, "home",          "shift" },
  { 0x4AB54, "home",          "TV" },
  { 0x42BE4, "home",          "wide mode full" },
  { 0xFCBE4, "home",          "wide mode full1" },
  { 0x02BE4, "home",          "wide mode full2" },
  { 0x82BE4, "home",          "wide mode normal" },
  { 0x7CBE4, "home",          "wide mode WZoom" },
  { 0xC2BE4, "home",          "wide mode zoom" },
  { 0x22BE4, "home",          "wide mode anamorphic zoom" }
};

alignas(4) const FlashCommandRow SONY_XBR_COMMANDS[] {
  { 0x0750, "power_on",       "power on" },
  { 0x0A90, "power_on",       "power toggle" },
  { 0x0F50, "power_off",      "power off" },
  { 0x0910, "power_off",      "0" },
  { 0x02D0, "cursor_left",    "Arrow Left" },
  { 0x0CD0, "cursor_right",   "Arrow Right" },
  { 0x02F0, "cursor_up",      "Arrow Up" },
  { 0x0AF0, "cursor_down",    "Arrow Down" },
  { 0x0A70, "cursor_enter",   "Arrow Select" },
  { 0x6923, "settings_menu",  "Action Menu" },
  { 0x62E9, "back",           "Back" },
  { 0x0070, "home",           "Home" },
  { 0x3123, "game_menu",      "Google Play" },
  { 0x0A50, "input",          "Input" },
  { 0x0250, "picture",        "TV" },
  { 0x0AE9, "focus_auto",     "subtitle" },
  { 0x0E90, "focus_manual",   "audio" },
  { 0x72E9, "shortcut_1",     "yellow" },
  { 0x12E9, "shortcut_2",     "blue" },
  { 0x52E9, "shortcut_3",     "red" },
  { 0x32E9, "shortcut_4",     "green" },
  { 0x0490, "volume_up",      "volume up" },
  { 0x0C90, "volume_down",    "volume down" },
  { 0x0290, "mute",           "mute" },
  { 0x0C10, "token_sniff",    "4" },
  { 0x0210, "token_clear",    "5" },
  { 0x0A10, "token_recall",   "6" },
  { 0x1CE9, "macro_record",   "fast forward" },
  { 0x2CE9, "macro_play",     "play" }
};

alignas(4) const FlashCommandRow TIVO_COMMANDS[] {
  { 0xE010, "power_on",       "TV power)" },
  { 0xE011, "power_off",      "live TV" },
  { 0xC031, "power_off",      "0" },
  { 0xE014, "cursor_up",      "arrow up" },
  { 0xE016, "cursor_down",    "arrow down" },
  { 0xE017, "cursor_left",    "arrow left" },
  { 0xE015, "cursor_right",   "arrow right" },
  { 0xE019, "cursor_enter",   "select" },
  { 0xF00C, "settings_menu",  "tivo" },
  { 0xF00D, "settings_menu",  "tivo (myHarmony)" },
  { 0xB044, "back",           "zoom" },
  { 0xE01E, "home",           "channel up" },
  { 0xC036, "game_menu",      "guide" },
  { 0xC034, "input",          "input" },
  { 0xE013, "picture",        "into" },
  { 0xD02E, "focus_auto",     "7" },
  { 0xD02F, "focus_manual",   "8" },
  { 0x9060, "shortcut_1",     "A yellow" },
  { 0x9061, "shortcut_2",     "B blue" },
  { 0x9062, "shortcut_3",     "C red" },
  { 0x9063, "shortcut_4",     "D green" },
  { 0xE01C, "volume_up",      "volume up" },
  { 0xE01D, "volume_down",    "volume down" },
  { 0xE01B, "mute",           "mute" },
  { 0xD02B, "token_sniff",    "4" },
  { 0xD02C, "token_clear",    "5" },
  { 0xD02D, "token_recall",   "6" },
  { 0xD020, "macro_record",   "record" },
  { 0xD021, "macro_play",     "play" }
};

alignas(4) const FlashCommandRow PANASONIC_COMMANDS[] {
  { 0x1003A3B, "power_on",    "on" },
  { 0x100BCBD, "power_off",   "off" },
  { 0x1007273, "cursor_left", "left arrow" },
  { 0x100F2F3, "cursor_right","right arrow" },
  { 0x1005253, "cursor_up",    "up arrow" },
  { 0x100D2D3, "cursor_down",  "down arrow" },
  { 0x1009293, "cursor_enter", "OK" },
  { 0x1004A4B, "settings_menu","menu" },
  { 0x1002B2A, "back",         "return" },
  { 0x1009C9D, "home",         "info" },
  { 0x10090F1, "game_menu",    "apps" },
  { 0x100A0A1, "input",        "AV input" },
  { 0x1000B0A, "picture",      "picture mode" },
  { 0x100C2C3, "focus_manual", "focus +" },
  { 0x100E2E3, "focus_auto",   "focus -" },
  { 0x1000E0F, "shortcut_1",   "red" },
  { 0x1008E8F, "shortcut_2",   "green" },
  { 0x1004E4F, "shortcut_3",   "yellow" },
  { 0x100CECF, "shortcut_4",   "blue" },
  { 0x1000405, "home",         "volume +" },
  { 0x1008485, "home",         "volume -" },
  { 0x1004C4D, "home",         "mute" },
  { 0x100A8A9, "token_sniff",  "4" },
  { 0x1002829, "token_clear",  "5" },
  { 0x100C8C9, "token_recall", "6" },
  { 0x1008889, "macro_record", "2" },
  { 0x1004849, "macro_play",   "3" },
  { 0x1009899, "home",         "0" },
  { 0x1000809, "home",         "1" },
  { 0x1006869, "home",         "7" },
  { 0x100E8E9, "home",         "8" },
  { 0x1001819, "home",         "9" },
  { 0x1002223, "home",         "HDMI 1" },
  { 0x100A2A3, "home",         "HDMI 2" },
  { 0x1006263, "home",         "computer" }
};


// Mirror your profile definitions exactly to bypass loading layout structures to dynamic vectors
static const char* const factory_names[] = {
  "AWOL Projector", "BenQ Projector", "Epson Projector", "Hisense", 
  "JVC HR-S9600u VCR", "JVC Projector A", "JVC Projector B", "LG Projector", 
  "Optoma Projector", "Sony Projector", "Sony XBR", "TiVo Roamio", "Panasonic Projector"
};
    
    
// THE UNIFIED ON-DEMAND DYNAMIC HYDRATION ENGINE
inline void load_profile_to_workspace(int idx) {
  active_profile_workspace.cmd_codes.clear();
  int factory = static_cast<int>(factory_count);

  if (idx < factory) {
    const FlashCommandRow* flash_array = nullptr;
    size_t array_size = 0;

    // -----------------------------------------------------------
    // EVALUATE PROFILE METADATA & SELECT TARGET PROGMEM MATRIX
    // -----------------------------------------------------------
    if (idx == 0) {
        active_profile_workspace.profile_name = "AWOL Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0xCE00;
        active_profile_workspace.cmd_clear_token_arm = 0xE51A;
        active_profile_workspace.cmd_clear_token_fire = 0xD728;
        flash_array = AWOL_COMMANDS;
        array_size = sizeof(AWOL_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 1) { 
        active_profile_workspace.profile_name = "BenQ Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0x3000;
        active_profile_workspace.cmd_clear_token_arm = 0x629D;
        active_profile_workspace.cmd_clear_token_fire = 0xEA15;
        flash_array = BENQ_COMMANDS;
        array_size = sizeof(BENQ_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 2) {
        active_profile_workspace.profile_name = "Epson Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0x5583;
        active_profile_workspace.cmd_clear_token_arm = 0xC23D;
        active_profile_workspace.cmd_clear_token_fire = 0x7A85;
        flash_array = EPSON_COMMANDS;
        array_size = sizeof(EPSON_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 3) {
        active_profile_workspace.profile_name = "Hisense";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0xFB04;
        active_profile_workspace.cmd_clear_token_arm = 0xEA15;
        active_profile_workspace.cmd_clear_token_fire = 0xA55A;
        flash_array = HISENSE_COMMANDS;
        array_size = sizeof(HISENSE_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 4) {
        active_profile_workspace.profile_name = "JVC HR-S9600u VCR";
        active_profile_workspace.protocol = PROTO_JVC;
        active_profile_workspace.device_address = 0x03C2;
        active_profile_workspace.cmd_clear_token_arm = 0xC2A4;
        active_profile_workspace.cmd_clear_token_fire = 0xC23C;
        flash_array = JVC_VCR_COMMANDS;
        array_size = sizeof(JVC_VCR_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 5) {
        active_profile_workspace.profile_name = "JVC Projector A";
        active_profile_workspace.protocol = PROTO_JVC;
        active_profile_workspace.device_address = 0xCE;
        active_profile_workspace.cmd_clear_token_arm = 0x56;
        active_profile_workspace.cmd_clear_token_fire = 0xF4;
        flash_array = JVC_PROJ_A_COMMANDS;
        array_size = sizeof(JVC_PROJ_A_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 6) {
        active_profile_workspace.profile_name = "JVC Projector B";
        active_profile_workspace.protocol = PROTO_JVC;
        active_profile_workspace.device_address = 0x36;
        active_profile_workspace.cmd_clear_token_arm = 0x56;
        active_profile_workspace.cmd_clear_token_fire = 0xF4;
        flash_array = JVC_PROJ_B_COMMANDS;
        array_size = sizeof(JVC_PROJ_B_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 7) {
        active_profile_workspace.profile_name = "LG Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0xFB04;
        active_profile_workspace.cmd_clear_token_arm = 0xEA15;
        active_profile_workspace.cmd_clear_token_fire = 0xBB44;
        flash_array = LG_COMMANDS;
        array_size = sizeof(LG_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 8) {
        active_profile_workspace.profile_name = "Optoma Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0xCD32;
        active_profile_workspace.cmd_clear_token_arm = 0x9A65;
        active_profile_workspace.cmd_clear_token_fire = 0xF00F;
        flash_array = OPTOMA_COMMANDS;
        array_size = sizeof(OPTOMA_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 9) {
        active_profile_workspace.profile_name = "Sony Projector";
        active_profile_workspace.protocol = PROTO_SONY;
        active_profile_workspace.device_address = 0x0000;
        active_profile_workspace.cmd_clear_token_arm = 0x8AB54;
        active_profile_workspace.cmd_clear_token_fire = 0x02D2A;
        flash_array = SONY_PROJ_COMMANDS;
        array_size = sizeof(SONY_PROJ_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 10) {
        active_profile_workspace.profile_name = "Sony XBR";
        active_profile_workspace.protocol = PROTO_SONY;
        active_profile_workspace.device_address = 0x0000;
        active_profile_workspace.cmd_clear_token_arm = 0x0210;
        active_profile_workspace.cmd_clear_token_fire = 0x0A70;
        flash_array = SONY_XBR_COMMANDS;
        array_size = sizeof(SONY_XBR_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 11) {
        active_profile_workspace.profile_name = "TiVo Roamio";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0x3085;
        active_profile_workspace.cmd_clear_token_arm = 0xD02C;
        active_profile_workspace.cmd_clear_token_fire = 0xE019;
        flash_array = TIVO_COMMANDS;
        array_size = sizeof(TIVO_COMMANDS) / sizeof(FlashCommandRow);
    }
    else if (idx == 12) {
        active_profile_workspace.profile_name = "Panasonic Projector";
        active_profile_workspace.protocol = PROTO_PANASONIC;
        active_profile_workspace.device_address = 0x4004;
        active_profile_workspace.cmd_clear_token_arm = 0x1002829;
        active_profile_workspace.cmd_clear_token_fire = 0x1009293;
        flash_array = PANASONIC_COMMANDS;
        array_size = sizeof(PANASONIC_COMMANDS) / sizeof(FlashCommandRow);
    }

    // -----------------------------------------------------------
    // ONE CENTRAL LOOP TO REHYDRATE FROM THE FLASH COLD-STORAGE
    // -----------------------------------------------------------
    if (flash_array != nullptr) {
        active_profile_workspace.cmd_codes.reserve(array_size);

        for (size_t i = 0; i < array_size; i++) {
            // Under ESP-IDF on ESP32, flash can be read directly like normal RAM!
            uint32_t code = flash_array[i].irCommand;
            const char* name_flash_ptr = flash_array[i].action_string;
            const char* btn_flash_ptr  = flash_array[i].button_name;
            
            char name_ram_buf[32] = {0};
            char btn_ram_buf[32]  = {0};
            
            // Use standard, safe strncpy instead of strncpy_P
            if (name_flash_ptr != nullptr) {
                std::strncpy(name_ram_buf, name_flash_ptr, sizeof(name_ram_buf) - 1);
            }
            if (btn_flash_ptr != nullptr) {
                std::strncpy(btn_ram_buf, btn_flash_ptr, sizeof(btn_ram_buf) - 1);
            }
            
            // Build directly into your runtime RAM workspace vectors
            add_cmd(code, name_ram_buf, btn_ram_buf);
        }
    }
  }
  else {
    // -----------------------------------------------------------
    // REHYDRATE FROM PERSISTENT USER-LEARNED NVS SLOTS
    // -----------------------------------------------------------
    int slot = idx - factory;
    uint64_t slot_nvs_key = 1948204712ULL + slot;
    auto pref_obj = esphome::global_preferences->make_preference<FlashStoredProfile>(slot_nvs_key);
    
    static FlashStoredProfile flash_p;
    std::memset(&flash_p, 0, sizeof(flash_p));
    
    if (pref_obj.load(&flash_p) && flash_p.struct_version == CURRENT_PROFILE_VERSION) {
        active_profile_workspace.profile_name = flash_p.profile_name;
        active_profile_workspace.protocol = flash_p.protocol;
        active_profile_workspace.device_address = flash_p.device_address;
        active_profile_workspace.cmd_clear_token_arm = flash_p.cmd_clear_token_arm;
        active_profile_workspace.cmd_clear_token_fire = flash_p.cmd_clear_token_fire;
        
        uint16_t load_limit = (flash_p.total_keys > 80) ? 80 : flash_p.total_keys;
        active_profile_workspace.cmd_codes.resize(load_limit);
        
        for (uint16_t k = 0; k < load_limit; k++) {
            auto& kv_pair = active_profile_workspace.cmd_codes[k];
            kv_pair.first = flash_p.keys[k].irCommand;
            
            // Field 2 (Internal target token ID) maps to action_string
            std::strncpy(kv_pair.second.action_string, flash_p.keys[k].action_string, sizeof(kv_pair.second.action_string) - 1);
            kv_pair.second.action_string[sizeof(kv_pair.second.action_string) - 1] = '\0';
            
            // Field 3 (Visual/Comment Label Description) maps to button_name
            std::strncpy(kv_pair.second.button_name, flash_p.keys[k].button_name, sizeof(kv_pair.second.button_name) - 1);
            kv_pair.second.button_name[sizeof(kv_pair.second.button_name) - 1] = '\0';
        }
    }
  }
  active_profile_workspace.cmd_codes.shrink_to_fit();
}



// Legacy bridge function placeholders required by older initialization clocks
inline void load_saved_flash_profiles() {
  flash_hydration_complete = true;
}

inline void link_hardware_buttons() {}



// ====================================================================
// 8. HEAP-SAFE HIGH-EFFICIENCY FLAT CSV EXPORT STREAM GENERATOR
// ====================================================================
inline std::string generate_profile_csv(int idx) {
  int current_active = esphome::id(active_remote_layout).value();
  
  if (idx != current_active) {
      load_profile_to_workspace(idx);
  }

  const auto& p = active_profile_workspace;

  // Pre-calculate exact memory footprint boundaries (approx 45 bytes per row)
  size_t reserved_size = 128 + (p.cmd_codes.size() * 45);

  std::string csv_out;
  csv_out.reserve(reserved_size);

  char chunk_buf[256];

  // Determine the best padding width format string based on active protocol structure tracks
  // %02X -> Enforces 2 characters (e.g., 0xA7)
  // %04X -> Enforces 4 characters (e.g., 0xB04F)
  // %05X -> Enforces 5 characters (e.g., 0x03A2A)
  const char* addr_fmt = "%04X"; // Default fallback
  const char* key_fmt  = "%X";   // Default fallback

  if (p.protocol == PROTO_NEC) {
      // NEC addresses are typically 4 hex digits (e.g. 0x7300), commands are 2 digits (e.g. 0x17)
      addr_fmt = "%04X";
      key_fmt  = "%02X";
  } else if (p.protocol == PROTO_JVC) {
      addr_fmt = "%04X";
      key_fmt  = "%04X";
  } else if (p.protocol == PROTO_SONY) {
      // Sony layouts use extended bit values (e.g. address 0x0000, keys 0x03A2A -> 5 digits)
      addr_fmt = "%04X";
      key_fmt  = "%05X";
  } else if (p.protocol == PROTO_PANASONIC) {
      addr_fmt = "%04X";
      key_fmt  = "%07X"; // Panasonic uses large 7-digit codes (e.g., 1003A3B)
  }

  // Row 1: Profile Configuration Header Metadata Line (Strict Padding Applied)
  std::string meta_line = "META," + std::to_string(idx) + "," + p.profile_name + "," + to_string(p.protocol) + ",";
  
  char addr_buf[32];
  char arm_buf[32];
  char fire_buf[32];
  
  snprintf(addr_buf, sizeof(addr_buf), addr_fmt, (unsigned int)p.device_address);
  snprintf(arm_buf, sizeof(arm_buf), key_fmt, (unsigned int)p.cmd_clear_token_arm);
  snprintf(fire_buf, sizeof(fire_buf), key_fmt, (unsigned int)p.cmd_clear_token_fire);
  
  snprintf(chunk_buf, sizeof(chunk_buf), "%s,%s,%s\n", addr_buf, arm_buf, fire_buf);
  csv_out += meta_line + chunk_buf;

  // Rows 2+: Command Code Data Rows (Strict Padding Applied)
  for (size_t i = 0; i < p.cmd_codes.size(); i++) {
      char irCommand_buf[32];
      snprintf(irCommand_buf, sizeof(irCommand_buf), key_fmt, (unsigned int)p.cmd_codes[i].first);
      
      snprintf(chunk_buf, sizeof(chunk_buf), "KEY,%s,%s,%s\n",
               irCommand_buf, p.cmd_codes[i].second.action_string, p.cmd_codes[i].second.button_name);
      csv_out += chunk_buf;
  }

  if (idx != current_active) {
      load_profile_to_workspace(current_active);
  }

  return csv_out;
}



// ====================================================================
// 10. HIGH-EFFICIENCY ZERO-ALLOCATION COLD-STREAM CSV PARSING IMPORT ENGINE
// ====================================================================
inline bool import_profile_from_csv(const std::string& csv_data) {
    if (csv_data.empty()) {
        ESP_LOGE("CSV Import", "Aborting import: Empty payload string received.");
        return false;
    }

    std::stringstream ss(csv_data);
    std::string line;
    bool meta_parsed = false;
    size_t keys_imported = 0;

    // Clear runtime dynamic vector memory structures before starting hydration pass
    active_profile_workspace.cmd_codes.clear();

    while (std::getline(ss, line)) {
        // Strip trailing carriage returns if data originated from a Windows host file
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        std::stringstream line_ss(line);
        std::string cell_type;
        std::getline(line_ss, cell_type, ',');

        // -----------------------------------------------------------
        // METADATA CONFIGURATION LINE PASS
        // -----------------------------------------------------------
        if (cell_type == "META") {
            std::string idx_str, name_str, proto_str, addr_str, arm_str, fire_str;
            
            std::getline(line_ss, idx_str, ','); // Read past the index string cell (ignored)
            std::getline(line_ss, name_str, ',');
            std::getline(line_ss, proto_str, ',');
            std::getline(line_ss, addr_str, ',');
            std::getline(line_ss, arm_str, ',');
            std::getline(line_ss, fire_str, ',');

            active_profile_workspace.profile_name = name_str;

            // Resolve human-readable protocol string back into integer definitions
            if (proto_str == "NEC")        active_profile_workspace.protocol = PROTO_NEC;
            else if (proto_str == "JVC")   active_profile_workspace.protocol = PROTO_JVC;
            else if (proto_str == "SONY")  active_profile_workspace.protocol = PROTO_SONY;
            else if (proto_str == "LG")    active_profile_workspace.protocol = PROTO_LG;
            else if (proto_str == "PANASONIC") active_profile_workspace.protocol = PROTO_PANASONIC;
            else if (proto_str == "RC5")   active_profile_workspace.protocol = PROTO_RC5;
            else if (proto_str == "RC6")   active_profile_workspace.protocol = PROTO_RC6;
            else                           active_profile_workspace.protocol = PROTO_UNKNOWN;

            // Extract core configuration hex properties from raw text streams
            active_profile_workspace.device_address = std::strtoul(addr_str.c_str(), nullptr, 16);
            active_profile_workspace.cmd_clear_token_arm = std::strtoul(arm_str.c_str(), nullptr, 16);
            active_profile_workspace.cmd_clear_token_fire = std::strtoul(fire_str.c_str(), nullptr, 16);

            meta_parsed = true;
            ESP_LOGI("CSV Import", "Metadata locked. Profile: %s, Protocol: %s, Address: 0x%X",
                     name_str.c_str(), proto_str.c_str(), (unsigned int)active_profile_workspace.device_address);
        }
        
        // -----------------------------------------------------------
        // HARDWARE DATA KEY EXTRAPOLATION PASS
        // -----------------------------------------------------------
        else if (cell_type == "KEY") {
            if (!meta_parsed) {
                ESP_LOGE("CSV Import", "Structure malformed! Received KEY block before valid META row.");
                return false;
            }

            std::string code_str, token_str, label_str;
            std::getline(line_ss, code_str, ',');
            std::getline(line_ss, token_str, ',');
            std::getline(line_ss, label_str, ',');

            uint32_t command_code = std::strtoul(code_str.c_str(), nullptr, 16);
            
            // Build and load directly using your lightweight row constructor function
            add_cmd(command_code, token_str.c_str(), label_str.c_str());
            keys_imported++;
        }
    }

    if (!meta_parsed || keys_imported == 0) {
        ESP_LOGE("CSV Import", "Parsing failed: Meta missing or zero keys processed.");
        return false;
    }

    active_profile_workspace.cmd_codes.shrink_to_fit();
    ESP_LOGI("CSV Import", "Successfully recovered %d layout items via cold-stream parsing.", (int)keys_imported);
    return true;
}





// ====================================================================
// ==== Custom Webserver ===
// ====================================================================
#include "esp_http_server.h"

// 1. FLASH-BOUND USER INTERFACE HTML DEFINITION (UPDATED TO EXACTLY THREE FONT SIZES)
static const char dashboard_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IR Hub Storage Matrix</title>
<style>
  :root {
    --fs-lg: 18px;
    --fs-md: 14px;
    --fs-sm: 12px;
  }
  body{font-family:system-ui,-apple-system,sans-serif;margin:20px;background:#0d1117;color:#c9d1d9;font-size:var(--fs-md)}
  .box{background:#161b22;padding:24px;border:1px solid #30363d;border-radius:6px;max-width:480px;margin:auto;margin-bottom:15px}
  h3{margin-top:0;color:#58a6ff;border-bottom:1px solid #21262d;padding-bottom:10px;font-size:var(--fs-lg)}
  label{display:block;margin:14px 0 6px;font-size:var(--fs-md);font-weight:600}
  select,input[type="file"]{width:100%;padding:8px;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#fff;box-sizing:border-box;font-size:var(--fs-md)}
  .row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:16px}
  .btn{padding:10px;background:#238636;color:#fff;border:2px;border-radius:6px;font-weight:bold;text-align:center;text-decoration:none;cursor:pointer;font-size:var(--fs-md)}
  .btn.sec{background:#21262d;border:2px solid #30363d;color:#c9d1d9}
  .btn:hover{opacity:0.9}
  .stat-list{display:flex;flex-direction:column;gap:10px;margin-top:3px}
  .stat-row{display:flex;justify-content:space-between;align-items:center;background:#0d1117;padding:12px 16px;border-radius:6px;border:1px solid #21262d}
  .stat-lbl{color:#8b949e;font-size:var(--fs-sm);font-weight:600;text-transform:uppercase}
  .stat-val{font-family:monospace;font-weight:bold;color:#ff7b72;font-size:var(--fs-md)}
</style>
<script>
  function updateActionUrls(){
    const idx = document.getElementById('profile_sel').value;
    document.getElementById('export_link').href = '/export?slot=' + idx;
    document.getElementById('upload_form').action = '/import?slot=' + idx;
  }
  window.onload = updateActionUrls;
</script>
</head><body>
<div class="box">
  <h3>Profile Management:  %BLE_REMOTE_NAME%</h3>

  <p style="font-size:var(--fs-sm);color:#8b949e;margin:0 0 15px">Active Profile: <span style="color:#58a6ff;font-weight:bold">%ACTIVE_NAME%</span></p>
  <form action="/select" method="GET">
    <label style="display:block;margin-bottom:6px;font-size:var(--fs-sm);color:#8b949e;">Select Target Profile Slot:</label>    
    <select id="profile_sel" name="slot" onchange="updateActionUrls()">%OPTIONS_MARKER%</select>
    
    <button type="submit" class="btn" style="width:100%;margin-top:12px;background:#403030">Activate Selected Profile</button>
  </form>
  
  <div style="margin-bottom:12px;">
    <a id="export_link" href="#" class="btn sec" style="background:#1f6feb; display:block;margin-bottom:12px;">Download Selected Profile</a>
  </div>

  <form id="upload_form" method="POST" enctype="multipart/form-data" style="margin-top:12px">
    <label style="display:block;margin-bottom:6px;font-size:var(--fs-sm);color:#8b949e;">Choose Profile CSV:</label>
    <input type="file" id="file_picker" name="file" onchange="document.getElementById('ul_btn').disabled=false;">
    <button type="submit" id="ul_btn" class="btn" style="width:100%;background:#238636;margin-top:12px;" disabled>Upload Profile to Slot</button>
  </form>
</div>
<div class="box">
  <h3>Macro Storage Management</h3>
  <div class="stat-list" style="margin-bottom:15px">
    <div style="margin-bottom:12px;">
      <a href="/export_macro" class="btn" style="display:block; background:#1f6feb; text-decoration:none;">Download Macros</a>
    </div>
    
    <form action="/import_macro" method="POST" enctype="multipart/form-data" style="border-top:1px solid #21262d; padding-top:12px;">
       <label style="display:block; margin-bottom:6px; font-size:var(--fs-sm); color:#8b949e;">Choose Macros CSV File:</label>
       <input type="file" name="file" accept=".csv" style="margin-bottom:8px;">
       <button type="submit" class="btn" style="width:100%;background:#238636;margin-top:12px;">Upload Macros</button>
    </form>
  </div>
</div>
<div class="box">
  <h3>System Diagnostics</h3>
  <div class="stat-list">
    <div class="stat-row">
      <div class="stat-lbl">CPU</div>
      <div class="stat-val" style="color:#79c0ff">%CPU_TYPE%</div>
      <div class="stat-lbl">Cores & Clock</div>
      <div class="stat-val" style="color:#79c0ff">%CPU_CORES% Cores @ %CPU_SPEED%</div>
    </div>
    <div class="stat-row">
      <div class="stat-lbl">Total Flash</div>
      <div class="stat-val" style="color:#79c0ff">%TOTAL_FLASH%</div>
    </div>
    <div class="stat-row">
      <div class="stat-lbl">App Partition</div>
      <div class="stat-val" style="color:#79c0ff">%APP_TOTAL%</div>
      <div class="stat-lbl">App Used</div>
      <div class="stat-val">%APP_USED%</div>
    </div>
    <div class="stat-row">
      <div class="stat-lbl">Total / Free Heap RAM</div>
      <div class="stat-val">%TOTAL_RAM% / %FREE_RAM%</div>
      <div class="stat-lbl">Heap Frag.</div>
      <div class="stat-val">%FRAGMENTATION%</div>
    </div>    
    <div class="stat-row">
      <div class="stat-lbl">Max Free Block</div>
      <div class="stat-val">%MAX_BLOCK%</div>
      <div class="stat-lbl">Free Stack Space</div>
      <div class="stat-val">%STACK_SIZE%</div>
    </div>
  </div>
  
  <div style="margin-top:15px; font-size:var(--fs-sm); border-top:1px solid #21262d; padding-top:12px">
    <div style="margin-bottom:6px"><span style="color:#8b949e">Reset Reason:</span> <span style="font-family:monospace;color:#79c0ff">%RESET_REASON%</span></div>
  </div>
</div>
</body></html>
)rawliteral";

// 2. ENTRY POINT ROUTE HANDLERS
inline esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    std::string html(dashboard_html);
    std::string active_name = active_profile_workspace.profile_name;
    std::string options = "";
    int total_slots = static_cast<int>(factory_count) + MAX_LEARNED_PROFILES;
    
    int active_idx = esphome::id(active_remote_layout).value();

    // ====================================================================
    // ⚡ ZERO-ALLOCATION STATIC NAME EXTPARATION LOOP FOR RAM STABILITY
    // ====================================================================
    options.reserve(1536); 

    for (int i = 0; i < total_slots; i++) {
        char opt_buf[128] = {0}; // Clear stack allocation
        const char* kind = (i < static_cast<int>(factory_count)) ? "Factory" : "Custom";
        std::string label_str;
        
        if (i < static_cast<int>(factory_count)) {
            // Reads from the global flash pointer matrix cleanly
            label_str = factory_names[i];
        } else {
            label_str = "Memory Slot " + std::to_string(i - static_cast<int>(factory_count));
        }
        
        snprintf(opt_buf, sizeof(opt_buf), "<option value=\"%d\" %s>%s [%s]</option>", 
                 i, (i == active_idx) ? "selected" : "", label_str.c_str(), kind);
        options += opt_buf;
    }

    
    // Explicitly restore the true operational profile once after generation wraps up
    load_profile_to_workspace(active_idx);
    // ====================================================================
    
    // FETCH DIAGNOSTIC DATA DIRECTLY FROM THE CORE KERNEL APPS
    char scratch[128];
    
    // Core SoC Property Detections
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    // Resolve Chip Model Strings
    const char* chip_model_str = "ESP32 (Unknown Variant)";
    switch(chip_info.model) {
        case CHIP_ESP32:   chip_model_str = "ESP32 (Classic)"; break;
        case CHIP_ESP32S2: chip_model_str = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model_str = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model_str = "ESP32-C3"; break;
        case CHIP_ESP32C6: chip_model_str = "ESP32-C6"; break;
        case CHIP_ESP32H2: chip_model_str = "ESP32-H2"; break;
        default: break;
    }
    std::string cpu_type_str(chip_model_str);
    std::string cpu_cores_str = std::to_string(chip_info.cores);
    
    // Fetch Active CPU Clock Speed directly from the system clock configuration register
    uint32_t cpu_speed_hz = esp_clk_cpu_freq();
    snprintf(scratch, sizeof(scratch), "%u MHz", (unsigned int)(cpu_speed_hz / 1000000));
    std::string cpu_speed_str(scratch);
    
    // --- 1. DYNAMIC FLASH CAPACITY DECODING ---
    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        flash_size = 0; 
    }
    
    snprintf(scratch, sizeof(scratch), "%u MB", (unsigned int)(flash_size / (1024 * 1024)));
    std::string total_flash_str(scratch);
    
    // --- 2. APP PARTITION OVER-THE-AIR DIAGNOSTICS ---
    const esp_partition_t *running_part = esp_ota_get_running_partition();
    uint32_t app_total_bytes = 0;
    uint32_t app_used_bytes = 0;
    float app_used_percent = 0.0;

    if (running_part != NULL) {
        app_total_bytes = running_part->size;
        
        esp_image_metadata_t img_meta;
        const esp_partition_pos_t part_pos = {
            .offset = running_part->address,
            .size = running_part->size,
        };
        
        if (esp_image_get_metadata(&part_pos, &img_meta) == ESP_OK) {
            app_used_bytes = img_meta.image_len;
            if (app_total_bytes > 0) {
                app_used_percent = ((float)app_used_bytes / (float)app_total_bytes) * 100.0;
            }
        }
    }

    // Format strings to map cleanly into placeholders
    snprintf(scratch, sizeof(scratch), "%.2f MB", (float)app_total_bytes / (1024.0 * 1024.0));
    std::string app_total_str(scratch);

    snprintf(scratch, sizeof(scratch), "%.2f MB (%.1f%%)", (float)app_used_bytes / (1024.0 * 1024.0), app_used_percent);
    std::string app_used_str(scratch);
    
    // Dynamic RAM Calculations
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
    snprintf(scratch, sizeof(scratch), "%u KB", (unsigned int)((heap_info.total_free_bytes + heap_info.total_allocated_bytes) / 1024));
    std::string total_ram_str(scratch);

    // Free Heap RAM
    size_t free_heap = esp_get_free_heap_size(); 
    snprintf(scratch, sizeof(scratch), "%u Bytes", (unsigned int)free_heap);
    std::string free_ram(scratch);
    
    // Heap Fragmentation
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    float fragmentation_percentage = 0.0f;
    if (free_heap > 0) {
        fragmentation_percentage = (1.0f - ((float)largest_free_block / (float)free_heap)) * 100.0f;
    }
    snprintf(scratch, sizeof(scratch), "%0.1f %%", fragmentation_percentage);
    std::string frag(scratch);
    
    // Max Free Block Size 
    snprintf(scratch, sizeof(scratch), "%u Bytes", (unsigned int)largest_free_block);
    std::string max_block(scratch);
    
    // Free Stack Space Room 
    unsigned int free_stack = (unsigned int)uxTaskGetStackHighWaterMark(NULL);
    snprintf(scratch, sizeof(scratch), "%u Bytes", (unsigned int)free_stack);
    std::string stack_size_str(scratch); 
        
    // Reset Reason Code
    int reason_code = (int)esp_reset_reason();
    std::string reset_reason = "Code " + std::to_string(reason_code);
    
    // Hardware Info 
    std::string hw_info = esphome::App.get_name().c_str(); 

    // EXTRANEOUS STRING REPLACEMENTS PASS
    size_t pos;
    while ((pos = html.find("%BLE_REMOTE_NAME%")) != std::string::npos) html.replace(pos, 17, TOSTRING_MACRO(BLE_REMOTE_NAME_STR));
    while ((pos = html.find("%ACTIVE_NAME%")) != std::string::npos) html.replace(pos, 13, active_name);
    while ((pos = html.find("%OPTIONS_MARKER%")) != std::string::npos) html.replace(pos, 16, options);
    while ((pos = html.find("%CPU_TYPE%")) != std::string::npos) html.replace(pos, 10, cpu_type_str);
    while ((pos = html.find("%CPU_CORES%")) != std::string::npos) html.replace(pos, 11, cpu_cores_str);
    while ((pos = html.find("%CPU_SPEED%")) != std::string::npos) html.replace(pos, 11, cpu_speed_str);
    while ((pos = html.find("%TOTAL_FLASH%")) != std::string::npos) html.replace(pos, 13, total_flash_str);
    while ((pos = html.find("%TOTAL_RAM%")) != std::string::npos) html.replace(pos, 11, total_ram_str);
    while ((pos = html.find("%FREE_RAM%")) != std::string::npos) html.replace(pos, 10, free_ram);
    while ((pos = html.find("%FRAGMENTATION%")) != std::string::npos) html.replace(pos, 15, frag);
    while ((pos = html.find("%MAX_BLOCK%")) != std::string::npos) html.replace(pos, 11, max_block);
    while ((pos = html.find("%STACK_SIZE%")) != std::string::npos) html.replace(pos, 12, stack_size_str);
    while ((pos = html.find("%RESET_REASON%")) != std::string::npos) html.replace(pos, 14, reset_reason);
    while ((pos = html.find("%HW_INFO%")) != std::string::npos) html.replace(pos, 9, hw_info);
    while ((pos = html.find("%APP_TOTAL%")) != std::string::npos) html.replace(pos, 11, app_total_str);
    while ((pos = html.find("%APP_USED%")) != std::string::npos) html.replace(pos, 10, app_used_str);
    
    return httpd_resp_send(req, html.c_str(), HTTPD_RESP_USE_STRLEN);
}

//==========================
inline esp_err_t select_handler(httpd_req_t *req) {
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char* buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[32];
            if (httpd_query_key_value(buf, "slot", param, sizeof(param)) == ESP_OK) {
                int chosen_slot = atoi(param);

                esphome::id(active_remote_layout).value() = chosen_slot;
                esphome::id(profile_has_been_stored).value() = 1; // remember that profile has been set
                
                load_profile_to_workspace(chosen_slot);
                esphome::id(setup_ir_receiver_for_current_profile).execute();
                
                static char change_buf[128];
                const char* kind = (chosen_slot < (int)factory_count) ? "internal" : "custom";
                snprintf(change_buf, sizeof(change_buf), "idx=%d  %s  [%s]",
                         chosen_slot, active_profile_workspace.profile_name.c_str(), kind);
                esphome::id(ir_active_profile_ts).publish_state(change_buf);
            }
        }
        free(buf);
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

//----------
// HEAP Efficient, Chunked Export Handler
inline esp_err_t export_handler(httpd_req_t *req) {
    int target_slot = 0;
    
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char* buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(buf, "slot", param, sizeof(param)) == ESP_OK) {
                target_slot = atoi(param);
            }
        }
        free(buf);
    }

    // Capture the currently running active remote layout to restore later
    int current_active = esphome::id(active_remote_layout).value();
    if (target_slot != current_active) {
        load_profile_to_workspace(target_slot);
    }

    const auto& p = active_profile_workspace;
    httpd_resp_set_type(req, "text/csv");
    
    char header_buf[128];
    snprintf(header_buf, sizeof(header_buf), "attachment; filename=profile_slot_%d.csv", target_slot);
    httpd_resp_set_hdr(req, "Content-Disposition", header_buf);

    // Format strings based on protocol structures
    const char* addr_fmt = "%04X";
    const char* key_fmt  = "%X";

    if (p.protocol == PROTO_NEC) {
        addr_fmt = "%04X";
        key_fmt  = "%02X";
    } else if (p.protocol == PROTO_JVC) {
        addr_fmt = "%04X";
        key_fmt  = "%04X";
    } else if (p.protocol == PROTO_SONY) {
        addr_fmt = "%04X";
        key_fmt  = "%05X";
    } else if (p.protocol == PROTO_PANASONIC) {
        addr_fmt = "%04X";
        key_fmt  = "%07X";
    }

    char chunk_buf[256];

    // 1. STREAM META ROW
    char addr_buf[32], arm_buf[32], fire_buf[32];
    snprintf(addr_buf, sizeof(addr_buf), addr_fmt, (unsigned int)p.device_address);
    snprintf(arm_buf, sizeof(arm_buf), key_fmt, (unsigned int)p.cmd_clear_token_arm);
    snprintf(fire_buf, sizeof(fire_buf), key_fmt, (unsigned int)p.cmd_clear_token_fire);
    
    snprintf(chunk_buf, sizeof(chunk_buf), "META,%d,%s,%s,%s,%s,%s\n", 
             target_slot, p.profile_name.c_str(), to_string(p.protocol), addr_buf, arm_buf, fire_buf);
    httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));

    // 2. STREAM KEY ROWS INDIVIDUALLY (Zero heap string allocation overhead)
    for (size_t i = 0; i < p.cmd_codes.size(); i++) {
        char irCommand_buf[32];
        snprintf(irCommand_buf, sizeof(irCommand_buf), key_fmt, (unsigned int)p.cmd_codes[i].first);
        
        snprintf(chunk_buf, sizeof(chunk_buf), "KEY,%s,%s,%s\n",
                 irCommand_buf, p.cmd_codes[i].second.action_string, p.cmd_codes[i].second.button_name);
        httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));
    }

    // 3. FINALIZE RESPONSE STREAM
    httpd_resp_send_chunk(req, NULL, 0);

    // Restore the system workspace layout cleanly back to the running profile
    if (target_slot != current_active) {
        load_profile_to_workspace(current_active);
    }

    return ESP_OK;
}


// --------------------------------------------------------------------
// HIGH-PERFORMANCE ZERO-HEAP STREAMING HTTP POST CSV IMPORT ENGINE
// --------------------------------------------------------------------
inline esp_err_t import_handler(httpd_req_t *req) {
    int target_slot = 0;
    
    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        char* buf = (char*)malloc(query_len);
        if (httpd_req_get_url_query_str(req, buf, query_len) == ESP_OK) {
            char param[32]; 
            if (httpd_query_key_value(buf, "slot", param, sizeof(param)) == ESP_OK) {
                target_slot = atoi(param); 
            }
        }
        free(buf);
    }

    size_t total_bytes = req->content_len;
    size_t remaining = total_bytes;
    
    if (total_bytes == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File is empty.");
        return ESP_FAIL;
    }

    std::string csv_accumulator;
    csv_accumulator.reserve(total_bytes);

    char chunk_buf[512]; 
    int received = 0;

    while (remaining > 0) {
        size_t read_target = (remaining < sizeof(chunk_buf)) ? remaining : sizeof(chunk_buf);
        if ((received = httpd_req_recv(req, chunk_buf, read_target)) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        csv_accumulator.append(chunk_buf, received);
        remaining -= received;
    }

    size_t start_pos = csv_accumulator.find("META,");
    if (start_pos == std::string::npos) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid backup configuration layout.");
        return ESP_FAIL;
    }

    size_t end_pos = csv_accumulator.rfind("\n-");
    std::string clean_csv = (end_pos != std::string::npos) ? 
                             csv_accumulator.substr(start_pos, end_pos - start_pos) : 
                             csv_accumulator.substr(start_pos);

    if (import_profile_from_csv(clean_csv)) {
        link_hardware_buttons();
        esphome::id(setup_ir_receiver_for_current_profile).execute();
        
        int final_custom_slot = target_slot - (int)factory_count;
        if (final_custom_slot < 0 || final_custom_slot >= MAX_LEARNED_PROFILES) {
            ESP_LOGW("Web Import", "Factory slot selected for import destination. Redirecting safely to Custom Memory Slot 0.");
            final_custom_slot = 0; 
            int hardware_slot_override = (int)factory_count + final_custom_slot;
            esphome::id(active_remote_layout).value() = hardware_slot_override;
        } else {
            esphome::id(active_remote_layout).value() = target_slot;
        }
        
        commit_database_to_flash(final_custom_slot);
        esphome::id(display_show).execute(true, "CSV Web Uploaded!", "Profile Operational", active_profile_workspace.profile_name);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "CSV processing failure.");
        return ESP_FAIL;
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

#include <esp_rom_crc.h>


// ====================================================================
// COMPACT CSV EXPORT GENERATOR FOR ALL MACRO SLOTS (ADAPTABLE)
// ====================================================================
inline std::string generate_macro_csv() {
    std::string csv_out;
    csv_out.reserve(4096); 
    
    char chunk[128];
    bool found_any_data = false;

    // Adaptable: Loops exactly up to your maximum bound buttons
    for (int slot_id = 0; slot_id < MAX_BOUND_HOTKEYS; slot_id++) {
        uint64_t macro_nvs_key = 384720194ULL + slot_id;
        auto pref_obj = esphome::global_preferences->make_preference<UniversalFlashMacro>(macro_nvs_key);
        
        static UniversalFlashMacro macro_buf;
        if (!pref_obj.load(&macro_buf) || macro_buf.total_steps == 0) {
            continue; 
        }

        found_any_data = true;
        std::memset(chunk, 0, sizeof(chunk));
        snprintf(chunk, sizeof(chunk), "MACRO,%d,Macro_Slot_%d\n", slot_id, slot_id);
        csv_out += chunk;

        for (uint16_t i = 0; i < macro_buf.total_steps; i++) {
            const auto& step = macro_buf.steps[i];
            std::memset(chunk, 0, sizeof(chunk));
            
            // STREAMLINED: Drops the old type column cell
            snprintf(chunk, sizeof(chunk), "STEP,%s,%s,%u\n", 
                     step.action_string, state_to_str(step.event_state), step.delay_ms);
            csv_out += chunk;
        }
    }

    if (!found_any_data) {
        snprintf(chunk, sizeof(chunk), "MACRO,0,Empty_Suite\n");
        csv_out += chunk;
    }

    return csv_out;
}

// ====================================================================
// HTTP GET CSV EXPORT HANDLER
// ====================================================================
// Heap Efficient, Chunked export_macro_text_handler
inline esp_err_t export_macro_text_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=esp32_xgimi_macros.csv");

    char chunk_buf[256];
    bool found_any_data = false;

    for (int slot_id = 0; slot_id < MAX_BOUND_HOTKEYS; slot_id++) {
        uint64_t macro_nvs_key = 384720194ULL + slot_id;
        auto pref_obj = esphome::global_preferences->make_preference<UniversalFlashMacro>(macro_nvs_key);
        
        static UniversalFlashMacro macro_buf;
        if (!pref_obj.load(&macro_buf) || macro_buf.total_steps == 0) {
            continue; 
        }

        found_any_data = true;
        snprintf(chunk_buf, sizeof(chunk_buf), "MACRO,%d,Macro_Slot_%d\n", slot_id, slot_id);
        httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));

        for (uint16_t i = 0; i < macro_buf.total_steps; i++) {
            const auto& step = macro_buf.steps[i];
            snprintf(chunk_buf, sizeof(chunk_buf), "STEP,%s,%s,%u\n", 
                     step.action_string, state_to_str(step.event_state), step.delay_ms);
            httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));
        }
    }

    if (!found_any_data) {
        snprintf(chunk_buf, sizeof(chunk_buf), "MACRO,0,Empty_Suite\n");
        httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));
    }

    httpd_resp_send_chunk(req, NULL, 0); // Finalize response
    return ESP_OK;
}


// ====================================================================
// COLD-STREAM ZERO-HEAP CSV MACRO DEPLOYMENT ENGINE (ADAPTABLE)
// ====================================================================
inline bool import_macro_from_csv(const std::string& csv_data) {
    std::stringstream ss(csv_data);
    std::string line;
    
    static UniversalFlashMacro macro_build;
    std::memset(&macro_build, 0, sizeof(macro_build));
    // Set validation tracker to 5 to protect structure bounds
    macro_build.struct_version = CURRENT_MACRO_VERSION;
    
    int active_slot = -1;

    auto save_active_macro = [&]() {
        // Adaptable boundary checking protects slot arrays against overflow
        if (active_slot >= 0 && active_slot < MAX_BOUND_HOTKEYS && macro_build.total_steps > 0) {
            uint64_t macro_nvs_key = 384720194ULL + active_slot;
            auto pref_obj = esphome::global_preferences->make_preference<UniversalFlashMacro>(macro_nvs_key);
            pref_obj.save(&macro_build);
            ESP_LOGI("MACRO_CSV", "Committed slot %d structure registry (%d steps)", active_slot, macro_build.total_steps);
        }
    };

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::stringstream line_ss(line);
        std::string cell_type;
        std::getline(line_ss, cell_type, ',');

        if (cell_type == "MACRO") {
            save_active_macro(); 
            
            std::string slot_str;
            std::getline(line_ss, slot_str, ',');
            active_slot = !slot_str.empty() ? std::atoi(slot_str.c_str()) : -1;
            
            std::memset(&macro_build, 0, sizeof(macro_build));
            macro_build.struct_version = CURRENT_MACRO_VERSION; 
        } 
        else if (cell_type == "STEP") {
            if (active_slot == -1 || macro_build.total_steps >= MAX_MACRO_STEPS) continue;
            
            std::string payload_str, state_str, delay_str;
            std::getline(line_ss, payload_str, ','); // Directly grabs the action string name
            std::getline(line_ss, state_str, ',');
            std::getline(line_ss, delay_str, ',');

            auto& step = macro_build.steps[macro_build.total_steps];
            step.event_state = str_to_state(state_str);
            step.delay_ms = std::strtoul(delay_str.c_str(), nullptr, 10);

            // Populate the action string array
            std::memset(step.action_string, 0, MAX_ACTION_STRING_LEN);
            std::strncpy(step.action_string, payload_str.c_str(), MAX_ACTION_STRING_LEN - 1);
            step.action_string[MAX_ACTION_STRING_LEN - 1] = '\0';

            macro_build.total_steps++;
        }
    }
    
    save_active_macro(); 
    esphome::global_preferences->sync();
    return true;
}

// ====================================================================
// HTTP POST CSV IMPORT HANDLER (SAFE LONG STRING PARSING)
// ====================================================================
inline esp_err_t import_macro_text_handler(httpd_req_t *req) {
    size_t total_bytes = req->content_len;
    size_t remaining = total_bytes;
    if (total_bytes == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload Empty.");
        return ESP_FAIL;
    }

    std::string accumulator;
    accumulator.reserve(total_bytes);
    char chunk_buf[512];
    int received = 0;

    while (remaining > 0) {
        size_t target = (remaining < sizeof(chunk_buf)) ? remaining : sizeof(chunk_buf);
        if ((received = httpd_req_recv(req, chunk_buf, target)) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        accumulator.append(chunk_buf, received);
        remaining -= received;
    }

    // Locate the start of the valid macro suite rows
    size_t start_pos = accumulator.find("MACRO,");
    if (start_pos == std::string::npos) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Malformed Macro Structure.");
        return ESP_FAIL;
    }

    // Search for the exact browser multipart trailing boundary marker 
    // instead of matching a loose single hyphen that could break custom key names
    size_t end_pos = accumulator.find("\r\n------", start_pos);
    if (end_pos == std::string::npos) {
        end_pos = accumulator.find("\n------", start_pos);
    }

    std::string clean_csv = (end_pos != std::string::npos) ? 
                             accumulator.substr(start_pos, end_pos - start_pos) : 
                             accumulator.substr(start_pos);

    if (import_macro_from_csv(clean_csv)) {
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Macro compilation breakdown.");
    return ESP_FAIL;
}


//===================================


inline void start_custom_web_server() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.ctrl_port = 32768; 
    config.stack_size = 8192; 

    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t select_uri = {
        .uri       = "/select",
        .method    = HTTP_GET,
        .handler   = select_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t export_uri = {
        .uri       = "/export",
        .method    = HTTP_GET,
        .handler   = export_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t import_uri = {
        .uri       = "/import",       
        .method    = HTTP_POST,      
        .handler   = import_handler, 
        .user_ctx  = NULL
    };

    httpd_uri_t export_macro_txt_uri = {
        .uri       = "/export_macro",
        .method    = HTTP_GET,
        .handler   = export_macro_text_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t import_macro_txt_uri = {
        .uri       = "/import_macro",
        .method    = HTTP_POST,
        .handler   = import_macro_text_handler,
        .user_ctx  = NULL
    };



    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &select_uri);
        httpd_register_uri_handler(server, &export_uri);
        httpd_register_uri_handler(server, &import_uri); 
        httpd_register_uri_handler(server, &export_macro_txt_uri);
        httpd_register_uri_handler(server, &import_macro_txt_uri);
    }
}


// ====================================================================
// SECRETS.YAML BINARY INJECTION PARSER (DO NOT CHANGE)
// ====================================================================
__asm__(
    ".section .rodata\n"
    ".global _yaml_data_start\n"
    ".global _yaml_data_end\n"
    "_yaml_data_start:\n"
    ".incbin \"../../../../secrets.yaml\"\n"
    "_yaml_data_end:\n"
    ".byte 0\n"
    ".section .text\n"
);

extern "C" {
    extern const char _yaml_data_start[];
    extern const char _yaml_data_end[];
}

namespace SecretsParser {
    inline uint8_t parse_hex_byte(char high, char low) {
        auto convert = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        return (convert(high) << 4) | convert(low);
    }

    inline void fill_tokens(uint8_t* destination) {
        const char* start = _yaml_data_start;
        const char* end = _yaml_data_end;
        size_t length = end - start;
        size_t idx = 0;
        if (length < 4) return;

        for (size_t i = 0; i < length - 3 && idx < 15; ++i) {
            if (start[i] == '0' && (start[i+1] == 'x' || start[i+1] == 'X')) {
                destination[idx++] = parse_hex_byte(start[i+2], start[i+3]);
                i += 3;
            }
        }
    }
}

inline std::vector<uint8_t> get_secret_wake_token() {
    std::vector<uint8_t> token(15, 0);
    SecretsParser::fill_tokens(token.data());
    return token;
}

