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
#include <string_view>

#define STRINGIFY_MACRO(x) #x
#define TOSTRING_MACRO(x) STRINGIFY_MACRO(x)

// =================== HUMAN-READABILITY DEFINES ==========================
#define PROTO_UNKNOWN   0
#define PROTO_NEC       1
#define PROTO_JVC       2
#define PROTO_SONY      3
#define PROTO_LG        4
#define PROTO_PANASONIC 5
#define PROTO_RC5       6
#define PROTO_RC6       7

static const char *const TAG_MAPS = "universal_hid_maps";

// --- IR RECEIVER WORKSPACE MODES  ---
#define IR_MODE_DEACTIVATED         0   // Safety arrest state / boot protection
#define IR_MODE_ACTION              1   // Default operational long-press layer
#define IR_MODE_SET_PROFILE         2   // Double-click profile capture scanner
#define IR_MODE_LEARN               3   // Triple-click sequential remote sniffer

// --- KEY EVENT STATES ---
#define KEY_EVENT_DOWN    0  // Initial button press edge
#define KEY_EVENT_HOLDING 1  // Continuous hold frame updates
#define KEY_EVENT_UP      2  // Physical button liftoff/release edge

// -- MACRO WORKFLOW STATES ---
#define MACRO_STATE_IDLE            0   // Standard operational state
#define MACRO_STATE_RECORDING       1   // Actively caching IR codes to buffer
#define MACRO_STATE_AWAITING_HOTKEY  2   // Capture stopped; waiting for destination button
#define MACRO_STATE_LIFTOFF_SHIELD  99  // Trapper guard to absorb release packet
// ====================================================================

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
  inline constexpr uint32_t CURRENT_PROFILE_VERSION = 68; // <-- increment this change in NVRAM storage structures
#endif


// ====================================================================
// OPTIMIZED MEMORY ALLOCATION CEILING SPECS
// ====================================================================
static const size_t MAX_ACTION_STRING_LEN = 16; // Perfectly fits up to 15 chars + null
static const size_t MAX_BUTTON_NAME_LEN   = 32; // Provides roomy headroom for RKEY/RCON strings
#define MAX_MACRO_STEPS    32  
#define MAX_BOUND_HOTKEYS  9  
#define CURRENT_MACRO_VERSION  8

typedef char action_stringType[MAX_ACTION_STRING_LEN]; // Standardized short Xgimi Tokens
typedef char button_nameType[MAX_BUTTON_NAME_LEN];     // Dynamic payload cell (RKEY/RCON data)
typedef char profile_nameType[32];
typedef char ComponentBufferStrType[128];


// Single Source of Truth for Button Mapping Arrays
inline constexpr const char* learn_button_names[] = {
  "power_on", "power_off", "cursor_left", "cursor_right", "cursor_up", "cursor_down",
  "cursor_enter", "settings_menu", "back", "home", "game_menu", "input", "picture",
  "focus_manual", "focus_auto", "shortcut_1", "shortcut_2", "shortcut_3", "shortcut_4",
  "volume_up", "volume_down", "mute", "token_sniff", "token_clear", "token_recall",
  "macro_record", "macro_play"
  #ifdef ENABLE_EXTRA_BUTTONS
  ,"custom_1", "custom_2", "custom_3", "custom_4", "custom_5", "custom_6", "custom_7", "custom_8"
  #endif
};

inline constexpr size_t TOTAL_SYSTEM_BUTTONS = sizeof(learn_button_names) / sizeof(learn_button_names[0]);

// ====================================================================
// Macro storage STRUCTS
// ====================================================================
#define MAX_MACRO_STEPS    32  
#define MAX_BOUND_HOTKEYS  9  
#define CURRENT_MACRO_VERSION  8


struct UniversalMacroStep {
  char action_string[MAX_ACTION_STRING_LEN]; 
  uint8_t event_state;    // 0 = DOWN, 2 = UP                  
  uint16_t delay_ms;      // Pacing interval
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
// STATIC HARDCODED FALLBACK XGIMI HID TRANSLATION MAP
// ====================================================================
inline uint16_t resolve_action_to_true_hid(const char* name, uint8_t& out_type) {
    if (name == nullptr) return 0xFFFF;

    // KEYBOARD RESOLUTION PATH (REPORT TYPE 0)
    if (std::strcmp(name, "power_off") == 0)         { out_type = 0; return 0x7F; }
    else if (std::strcmp(name, "cursor_up") == 0)    { out_type = 0; return 0x52; }
    else if (std::strcmp(name, "cursor_down") == 0)  { out_type = 0; return 0x51; }
    else if (std::strcmp(name, "cursor_left") == 0)  { out_type = 0; return 0x50; }
    else if (std::strcmp(name, "cursor_right") == 0) { out_type = 0; return 0x4F; }
    else if (std::strcmp(name, "cursor_enter") == 0) { out_type = 0; return 0x28; }
    else if (std::strcmp(name, "settings_menu") == 0){ out_type = 0; return 0x41; }
    else if (std::strcmp(name, "back") == 0)          { out_type = 0; return 0x29; }
    else if (std::strcmp(name, "home") == 0)          { out_type = 0; return 0x4A; }
    else if (std::strcmp(name, "game_menu") == 0)     { out_type = 0; return 0x65; }
    else if (std::strcmp(name, "focus_manual") == 0)  { out_type = 0; return 0x3D; }
    else if (std::strcmp(name, "focus_auto") == 0)    { out_type = 0; return 0x44; }
    else if (std::strcmp(name, "volume_up") == 0)    { out_type = 0; return 0x80; }
    else if (std::strcmp(name, "volume_down") == 0)  { out_type = 0; return 0x81; }

    else if (std::strcmp(name, "custom_1") == 0 || std::strcmp(name, "custom_2") == 0 || 
             std::strcmp(name, "custom_3") == 0 || std::strcmp(name, "custom_4") == 0 || 
             std::strcmp(name, "custom_5") == 0 || std::strcmp(name, "custom_6") == 0 || 
             std::strcmp(name, "custom_7") == 0 || std::strcmp(name, "custom_8") == 0)      
             { out_type = 0; return 0x4A; }

    // CONSUMER RESOLUTION PATH (REPORT TYPE 1)
    else if (std::strcmp(name, "input") == 0)        { out_type = 1; return 0x01BC; }
    else if (std::strcmp(name, "picture") == 0)      { out_type = 1; return 0x0223; }
    else if (std::strcmp(name, "shortcut_1") == 0)   { out_type = 1; return 0x021D; }
    else if (std::strcmp(name, "shortcut_2") == 0)   { out_type = 1; return 0x021F; }
    else if (std::strcmp(name, "shortcut_3") == 0)   { out_type = 1; return 0x0221; }
    else if (std::strcmp(name, "shortcut_4") == 0)   { out_type = 1; return 0x0222; }
    else if (std::strcmp(name, "mute") == 0)         { out_type = 1; return 0x01BD; }
    
    out_type = 2; 
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
  FlashStoredKey keys[55];       // Centralized configuration ceiling cap
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
// METADATA-DECOUPLED BLUETOOTH HID INJECTION ENGINE
// ====================================================================
inline void fire_bluetooth_hid_action(const char* action_name, int event_state) {
    if (action_name == nullptr) return;

    // Handle instant virtual action press redirects for power_on hardware lines
    if (std::strcmp(action_name, "power_on") == 0) {
        if (event_state == KEY_EVENT_DOWN) {
            esphome::id(power_on).press();
        }
        return;
    }

    uint8_t resolved_type = 2; // 0 = Keyboard, 1 = Consumer, 2 = Token/None
    uint16_t true_hid_code = 0xFFFF;

    // ⚡ HIGH-SPEED LOOKUP SCAN:
    // Isolate the single matching entry row from your workspace container vector
    const IRCommand* runtime_cmd = nullptr;
    for (const auto& kv_pair : active_profile_workspace.cmd_codes) {
        if (std::strcmp(kv_pair.second.action_string, action_name) == 0) {
            runtime_cmd = &kv_pair.second;
            break; // Row isolated, exit loop instantly
        }
    }

    bool dynamic_payload_found = false;
    if (runtime_cmd != nullptr) {
        // High-speed, zero-allocation prefix comparison checks on the button_name cell string
        if (std::strncmp(runtime_cmd->button_name, "RKEY:", 5) == 0) {
            resolved_type = 0; // Force Keyboard Report Profile
            true_hid_code = (uint16_t)std::strtoul(runtime_cmd->button_name + 5, nullptr, 16);
            dynamic_payload_found = true;
        }
        else if (std::strncmp(runtime_cmd->button_name, "RCON:", 5) == 0) {
            resolved_type = 1; // Force Consumer Report Profile
            true_hid_code = (uint16_t)std::strtoul(runtime_cmd->button_name + 5, nullptr, 16);
            dynamic_payload_found = true;
        }
    }

    // Fallback: If no metadata protocol payload is registered, look up standard hardcoded Xgimi tables
    if (!dynamic_payload_found) {
        true_hid_code = resolve_action_to_true_hid(action_name, resolved_type);
    }

    if (true_hid_code == 0xFFFF && resolved_type != 2) return;

    // Streamlined low-overhead driver execution pipeline
    if (event_state == KEY_EVENT_DOWN) {
        if (resolved_type == 0)      esphome::id(xgimi_remote_controller).hold_keyboard((uint8_t)true_hid_code);
        else if (resolved_type == 1) esphome::id(xgimi_remote_controller).hold_consumer(true_hid_code);
    }
    else if (event_state == KEY_EVENT_HOLDING) {
        // Consumer frames do not use typematic repeat pulses; maintain baseline downstream state directly
        if (resolved_type == 0)      esphome::id(xgimi_remote_controller).hold_keyboard((uint8_t)true_hid_code);
        else if (resolved_type == 1) esphome::id(xgimi_remote_controller).hold_consumer(true_hid_code);
    }
    else if (event_state == KEY_EVENT_UP) {
        if (resolved_type == 0)      esphome::id(xgimi_remote_controller).release_held_keyboard_();
        else if (resolved_type == 1) {
            esphome::id(xgimi_remote_controller).hold_consumer(0x0000); // Send explicit zeroed release frame
            esphome::id(xgimi_remote_controller).release_held_consumer_();
        }
    }
}


// ====================================================================
// DYNAMIC PROFILE LOOKUP FOR PHYSICAL OVERRIDE ROUTES
// ====================================================================
inline uint32_t find_active_command_by_name(const char* action_name) {
    if (action_name == nullptr) return 0;
    
    // Look up within our primary active RAM workspace cache container
    for (const auto& kv_pair : active_profile_workspace.cmd_codes) {
        if (std::strcmp(kv_pair.second.action_string, action_name) == 0) {
            return kv_pair.first; // Found matching hex command mapping code!
        }
    }
    return 0; // Token not explicitly mapped in current profile configuration
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
  { 0xC4BE4, "home",          "pos custom 2" },
  { 0x24BE4, "home",          "pos custom 3" },
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
  { 0x22BE4, "home",          "wide mode anamorph" }
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
// ZERO-HEAP IN-PLACE TEXT TOKENIZATION CSV PROFILE PARSING ENGINE
// ====================================================================
inline bool import_profile_from_csv(const std::string& csv_data) {
    if (csv_data.empty()) {
        ESP_LOGE("CSV Import", "Aborting import: Empty payload string received.");
        return false;
    }

    bool meta_parsed = false;
    size_t keys_imported = 0;

    active_profile_workspace.cmd_codes.clear();

    size_t line_start = 0;
    while (line_start < csv_data.size()) {
        size_t line_end = csv_data.find('\n', line_start);
        if (line_end == std::string::npos) line_end = csv_data.size();

        std::string_view line_view(&csv_data[line_start], line_end - line_start);
        line_start = line_end + 1;

        if (!line_view.empty() && line_view.back() == '\r') {
            line_view.remove_suffix(1);
        }
        if (line_view.empty()) continue;

        auto get_next_cell = [](std::string_view& src) -> std::string_view {
            if (src.empty()) return std::string_view{};
            size_t comma_pos = src.find(',');
            if (comma_pos == std::string::npos) {
                std::string_view ret = src;
                src = std::string_view{};
                return ret;
            }
            std::string_view ret = src.substr(0, comma_pos);
            src.remove_prefix(comma_pos + 1);
            return ret;
        };

        std::string_view cell_type = get_next_cell(line_view);

        // -----------------------------------------------------------
        // METADATA CONFIGURATION LINE PASS
        // -----------------------------------------------------------
        if (cell_type == "META") {
            get_next_cell(line_view); 
            std::string_view name_view  = get_next_cell(line_view);
            std::string_view proto_view = get_next_cell(line_view);
            std::string_view addr_view  = get_next_cell(line_view);
            std::string_view arm_view   = get_next_cell(line_view);
            std::string_view fire_view  = get_next_cell(line_view);

            active_profile_workspace.profile_name.assign(name_view.data(), name_view.size());

            if (proto_view == "NEC")            active_profile_workspace.protocol = PROTO_NEC;
            else if (proto_view == "JVC")       active_profile_workspace.protocol = PROTO_JVC;
            else if (proto_view == "SONY")      active_profile_workspace.protocol = PROTO_SONY;
            else if (proto_view == "LG")        active_profile_workspace.protocol = PROTO_LG;
            else if (proto_view == "PANASONIC") active_profile_workspace.protocol = PROTO_PANASONIC;
            else if (proto_view == "RC5")       active_profile_workspace.protocol = PROTO_RC5;
            else if (proto_view == "RC6")       active_profile_workspace.protocol = PROTO_RC6;
            else                                active_profile_workspace.protocol = PROTO_UNKNOWN;

            char tmp[32] = {0};
            std::memcpy(tmp, addr_view.data(), std::min(addr_view.size(), sizeof(tmp) - 1));
            active_profile_workspace.device_address = std::strtoul(tmp, nullptr, 16);

            std::memset(tmp, 0, sizeof(tmp));
            std::memcpy(tmp, arm_view.data(), std::min(arm_view.size(), sizeof(tmp) - 1));
            active_profile_workspace.cmd_clear_token_arm = std::strtoul(tmp, nullptr, 16);

            std::memset(tmp, 0, sizeof(tmp));
            std::memcpy(tmp, fire_view.data(), std::min(fire_view.size(), sizeof(tmp) - 1));
            active_profile_workspace.cmd_clear_token_fire = std::strtoul(tmp, nullptr, 16);

            meta_parsed = true;
            ESP_LOGI("CSV Import", "Metadata locked. Profile: %s, Address: 0x%X",
                     active_profile_workspace.profile_name.c_str(), (unsigned int)active_profile_workspace.device_address);
        }
        
        // -----------------------------------------------------------
        // HARDWARE DATA KEY EXTRAPOLATION PASS
        // -----------------------------------------------------------
        else if (cell_type == "KEY") {
            if (!meta_parsed) {
                ESP_LOGE("CSV Import", "Structure malformed! Received KEY block before valid META row.");
                return false;
            }

            std::string_view code_view  = get_next_cell(line_view);
            std::string_view token_view = get_next_cell(line_view); 
            std::string_view label_view = get_next_cell(line_view); 

            char tmp_code[32] = {0};
            std::memcpy(tmp_code, code_view.data(), std::min(code_view.size(), sizeof(tmp_code) - 1));
            uint32_t command_code = std::strtoul(tmp_code, nullptr, 16);
            
            char token_buf[MAX_ACTION_STRING_LEN] = {0}; 
            char label_buf[MAX_BUTTON_NAME_LEN]   = {0}; 
            
            std::memcpy(token_buf, token_view.data(), std::min(token_view.size(), sizeof(token_buf) - 1));
            std::memcpy(label_buf, label_view.data(), std::min(label_view.size(), sizeof(label_buf) - 1));

            add_cmd(command_code, token_buf, label_buf);
            keys_imported++;
        }
    } // <-- Closes the while loop

    if (!meta_parsed || keys_imported == 0) {
        ESP_LOGE("CSV Import", "Parsing failed: Meta missing or zero keys processed.");
        return false;
    }

    active_profile_workspace.cmd_codes.shrink_to_fit();
    ESP_LOGI("CSV Import", "Successfully recovered %d layout items.", (int)keys_imported);
    return true;
} 




// ====================================================================
// ==== Custom Webserver ===
// ====================================================================
#include "esp_http_server.h"



// ====================================================================
// HIGH-EFFICIENCY ZERO-ALLOCATION STREAMING HTTP GET ROOT HANDLER
// ====================================================================
inline esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    
    char scratch[256] = {0}; 
    char scratch_option[256] = {0};
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    const char* chip_model_str = "ESP32 (Classic)";
    switch(chip_info.model) {
        case CHIP_ESP32:   chip_model_str = "ESP32 (Classic)"; break;
        case CHIP_ESP32S2: chip_model_str = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model_str = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model_str = "ESP32-C3"; break;
        case CHIP_ESP32C6: chip_model_str = "ESP32-C6"; break;
        case CHIP_ESP32H2: chip_model_str = "ESP32-H2"; break;
        default: break;
    }
    
    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        flash_size = 0; 
    }
    
    const esp_partition_t *running_part = esp_ota_get_running_partition();
    uint32_t app_total_bytes = 0;
    uint32_t app_used_bytes = 0;
    float app_used_percent = 0.0f;

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
                app_used_percent = ((float)app_used_bytes / (float)app_total_bytes) * 100.0f;
            }
        }
    }

    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
    size_t free_heap = esp_get_free_heap_size(); 
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    float fragmentation_percentage = 0.0f;
    if (free_heap > 0) {
        fragmentation_percentage = (1.0f - ((float)largest_free_block / (float)free_heap)) * 100.0f;
    }

    // CHUNK 1: Document head, CSS formatting styles, and Profile Selection Form
    httpd_resp_send_chunk(req, R"rawliteral(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IR Hub Storage Matrix</title>
<style>
  :root { --fs-lg: 18px; --fs-md: 14px; --fs-sm: 12px; }
  body{font-family:system-ui,-apple-system,sans-serif;margin:12px;background:#0d1117;color:#c9d1d9;font-size:var(--fs-md)}
  .box{background:#161b22;padding:14px 20px;border:1px solid #30363d;border-radius:6px;max-width:640px;margin:auto;margin-bottom:10px}
  h3{margin-top:0;color:#58a6ff;border-bottom:1px solid #21262d;padding-bottom:6px;margin-bottom:10px;font-size:var(--fs-lg)}
  label{display:block;margin:8px 0 4px;font-size:var(--fs-md);font-weight:600}
  select,input[type="file"]{width:100%;padding:6px;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#fff;box-sizing:border-box;font-size:var(--fs-md)}
  .btn{padding:8px;background:#238636;color:#fff;border:none;border-radius:6px;font-weight:bold;text-align:center;text-decoration:none;cursor:pointer;font-size:var(--fs-lg)}
  .btn.sec{background:#21262d;border:1px solid #30363d;color:#c9d1d9}
  .btn:hover{opacity:0.9}
  
  /* Adjusted 4-Column Grid Layout Engine */
  .grid-container { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; margin-top: 10px; }
  .btn-test { padding: 12px 6px; background: #1f6feb; color: white; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; font-size: var(--fs-md); text-align: center; }
  .btn-test:active { background: #388bfd; }
  .btn-test.pwr { grid-column: span 2; background: #da3637; }
  .btn-test.pwr:active { background: #f85149; }
  .btn-test.util { background: #21262d; border: 1px solid #30363d; color: #c9d1d9; }
  .btn-test.util:active { background: #30363d; }
  .btn-test.focus { background: #41562d; border: 1px solid #30363d; color: #c9d1d9; }
  .btn-test.focus:active { background: #50663d; }
  .btn-test.dmenu { background: #76752D; border: 1px solid #30363d; color: #c9d1d9; }
  .btn-test.dmenu:active { background: #76752D; }

  .btn-test.span-2 { grid-column: span 2; }

  /* Mobile Responsive Fallback (Flips to 2 columns on small mobile devices) */
  @media(max-width: 480px) {
    .box { max-width: 100%; }
    .grid-container { grid-template-columns: repeat(2, 1fr); }
    .btn-test.pwr { grid-column: span 1; }
    .btn-test.span-2 { grid-column: span 1; }
  }
  
  .stat-grid-box { background: #0d1117; border: 1px solid #21262d; border-radius: 6px; padding: 10px 14px; margin-top: 3px; }
  .diag-grid { display: grid; grid-template-columns: repeat(2, 1fr); column-gap: 20px; row-gap: 6px; }
  .diag-item { display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #21262d; padding-bottom: 4px; }
  .diag-item.span-2 { grid-column: span 2; }
  .stat-lbl{color:#8b949e;font-size:var(--fs-sm);font-weight:600;text-transform:uppercase;margin-right:8px;white-space:nowrap}
  .stat-val{font-family:monospace;font-weight:bold;color:#ff7b72;font-size:var(--fs-md);text-align:right}
</style>
<script>
  function updateActionUrls(){
    const idx = document.getElementById('profile_sel').value;
    document.getElementById('export_link').href = '/export?slot=' + idx;
    document.getElementById('upload_form').action = '/import?slot=' + idx;
  }
  function sendClick(actionName) {
    fetch('/button_inject?action=' + actionName);
  }
  window.onload = updateActionUrls;
</script>
</head><body>
<div class="box">
  <h3>Profile Management: ESP32 IR Hub Target</h3>
)rawliteral", HTTPD_RESP_USE_STRLEN);
           
    int active_idx = esphome::id(active_remote_layout).value();
    load_profile_to_workspace(active_idx);
    
    httpd_resp_send_chunk(req, "  <p style=\"font-size:var(--fs-sm);color:#8b949e;margin:0 0 15px\">Active Profile: <span style=\"color:#58a6ff;font-weight:bold\">", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, active_profile_workspace.profile_name.c_str(), HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, R"rawliteral(</span></p>
  <form action="/select" method="GET">
    <label style="display:block;margin-bottom:6px;font-size:var(--fs-sm);color:#8b949e;">Select Target Profile Slot:</label>
    <select id="profile_sel" name="slot" onchange="updateActionUrls()">)rawliteral", HTTPD_RESP_USE_STRLEN);

    int total_slots = static_cast<int>(factory_count) + MAX_LEARNED_PROFILES;
    for (int i = 0; i < total_slots; i++) {
        const char* kind = (i < static_cast<int>(factory_count)) ? "Factory" : "Custom";
        const char* label_ptr = (i < static_cast<int>(factory_count)) ? factory_names[i] : "Custom Slot";
        
        if (i < static_cast<int>(factory_count)) {
            snprintf(scratch_option, sizeof(scratch_option), "<option value=\"%d\" %s>%s [%s]</option>", 
                     i, (i == active_idx) ? "selected" : "", label_ptr, kind);
        } else {
            snprintf(scratch_option, sizeof(scratch_option), "<option value=\"%d\" %s>Memory Slot %d [%s]</option>", 
                     i, (i == active_idx) ? "selected" : "", i - static_cast<int>(factory_count), kind);
        }
        httpd_resp_send_chunk(req, scratch_option, strlen(scratch_option));
    }
    
    load_profile_to_workspace(active_idx);

    // --------------------------------------------------------------------
    // CHUNK 2: INTERACTION TESTING CONSOLE (COMPACTED TO 4-COLUMNS GRID)
    // --------------------------------------------------------------------
    httpd_resp_send_chunk(req, R"rawliteral(</select>
    <button type="submit" class="btn" style="width:100%;margin-top:12px;background:#403030">Activate Selected Profile</button>
  </form>
  
  <div style="margin-bottom:12px; margin-top:12px;">
    <a id="export_link" href="#" class="btn sec" style="background:#1f6feb; display:block; text-align:center;">Download Selected Profile</a>
  </div>
  <form id="upload_form" method="POST" enctype="multipart/form-data">
    <input type="file" id="file_picker" name="file" onchange="document.getElementById('ul_btn').disabled=false;">
    <button type="submit" id="ul_btn" class="btn" style="width:100%;background:#238636;margin-top:12px;" disabled>Upload Profile to Slot</button>
  </form>
  
</div>

<div class="box">
  <h3>Remote Interaction Testing Console</h3>
  <div class="grid-container">
    <!-- Row 1: System Power Management Keys (Using Span-2 for alignment in 4 columns) -->
    <button class="btn-test pwr" onclick="sendClick('power_on')">POWER ON</button>
    <button class="btn-test pwr" onclick="sendClick('power_off')">POWER OFF</button>
    
    <!-- Row 2: Directional Controls Up Direction -->
    <button class="btn-test dmenu" onclick="sendClick('home')">HOME</button>
    <button class="btn-test" onclick="sendClick('cursor_up')">UP</button>
    <button class="btn-test util" style="visibility:hidden;"></button>
    <button class="btn-test util" style="visibility:hidden;"></button>
    
    <!-- Row 3: Navigation Mid Track Row -->
    <button class="btn-test" onclick="sendClick('cursor_left')">LEFT</button>
    <button class="btn-test" onclick="sendClick('cursor_enter')">ENTER</button>
    <button class="btn-test" onclick="sendClick('cursor_right')">RIGHT</button>
    <button class="btn-test dmenu" onclick="sendClick('game_menu')">GAME</button>
    
    <!-- Row 4: Directional Controls Down Direction -->
    <button class="btn-test dmenu" onclick="sendClick('back')">BACK</button>
    <button class="btn-test" onclick="sendClick('cursor_down')">DOWN</button>
    <button class="btn-test dmenu" onclick="sendClick('settings_menu')">MENU</button>
    <button class="btn-test util" style="visibility:hidden;"></button>
    
    <!-- Row 5: Hardware Audio Controls -->
    <button class="btn-test util" onclick="sendClick('volume_down')">VOL -</button>
    <button class="btn-test util" onclick="sendClick('mute')">MUTE</button>
    <button class="btn-test util" onclick="sendClick('volume_up')">VOL +</button>
    <button class="btn-test util" onclick="sendClick('input')">INPUT</button>
    
    <!-- Row 6: Hardware Video/Gaming Controls -->
    <button class="btn-test focus" onclick="sendClick('focus_manual')">Focus MANUAL</button>
    <button class="btn-test focus" onclick="sendClick('focus_auto')">Focus AUTO</button>
    <button class="btn-test util" onclick="sendClick('picture')">Picture</button>
    <button class="btn-test util" style="visibility:hidden;"></button>
    
    <!-- Row 7: Programmable App Shortcuts -->
    <button class="btn-test" onclick="sendClick('shortcut_1')">Short 1</button>
    <button class="btn-test" onclick="sendClick('shortcut_2')">Short 2</button>
    <button class="btn-test" onclick="sendClick('shortcut_3')">Short 3</button>
    <button class="btn-test" onclick="sendClick('shortcut_4')">Short 4</button>

    <!-- Row 8: Automation Macro Engines -->
    <button class="btn-test util" onclick="sendClick('macro_record')">Macro RECORD</button>
    <button class="btn-test util" onclick="sendClick('macro_play')">Macro PLAY</button>
    <button class="btn-test util" style="visibility:hidden;"></button>
    <button class="btn-test util" style="visibility:hidden;"></button>
    
    <!-- Row 9: Bluetooth Token Diagnostic Layer -->
    <button class="btn-test util" onclick="sendClick('token_sniff')">Token SNIFF</button>
    <button class="btn-test util" onclick="sendClick('token_recall')">Token RECALL</button>
    <button class="btn-test util" style="visibility:hidden;"></button>
    <button class="btn-test util" onclick="sendClick('token_clear')">Token CLEAR</button>
  </div>
</div>
)rawliteral", HTTPD_RESP_USE_STRLEN); // <-- This safely closes Chunk 2

#ifdef ENABLE_EXTRA_BUTTONS
    // ⚡ CLEAN EXTRA CHUNK: Opens its own distinct R"rawliteral( block safely
    httpd_resp_send_chunk(req, R"rawliteral(
<div class="box">
  <h3>Extended Function Matrix</h3>
  <div class="grid-container">
    <!-- Row 10: Custom 1 to 4 -->
    <button class="btn-test util" onclick="sendClick('custom_1')">Custom 1</button>
    <button class="btn-test util" onclick="sendClick('custom_2')">Custom 2</button>
    <button class="btn-test util" onclick="sendClick('custom_3')">Custom 3</button>
    <button class="btn-test util" onclick="sendClick('custom_4')">Custom 4</button>
    <!-- Row 11: Custom 5 to 8 -->
    <button class="btn-test util" onclick="sendClick('custom_5')">Custom 5</button>
    <button class="btn-test util" onclick="sendClick('custom_6')">Custom 6</button>
    <button class="btn-test util" onclick="sendClick('custom_7')">Custom 7</button>
    <button class="btn-test util" onclick="sendClick('custom_8')">Custom 8</button>
    <button class="btn-test util" style="visibility:hidden;"></button>
  </div>
</div>
)rawliteral", HTTPD_RESP_USE_STRLEN); // <-- Safely closes the extra chunk
#endif


    // --------------------------------------------------------------------
    // CHUNK 3: MACRO STORAGE MANAGEMENT & DIAGNOSTICS CONTAINER STARTS
    // --------------------------------------------------------------------
    httpd_resp_send_chunk(req, R"rawliteral(
<div class="box">
  <h3>Macro Storage Management</h3>
  <div style="margin-bottom:15px;">
    <div style="margin-bottom:12px;">
      <a href="/export_macro" class="btn" style="display:block; background:#1f6feb; text-decoration:none;">Download Macros</a>
    </div>
    <form action="/import_macro" method="POST" enctype="multipart/form-data" style="border-top:1px solid #21262d; padding-top:12px;">
       <input type="file" name="file" accept=".csv" style="margin-bottom:8px;">
       <button type="submit" class="btn" style="width:100%;background:#238636;margin-top:12px;">Upload Macros</button>
    </form>
  </div>
</div>

<div class="box">
  <h3>System Diagnostics</h3>
  <div class="stat-grid-box">
    <div class="diag-grid">)rawliteral", HTTPD_RESP_USE_STRLEN);

    // --------------------------------------------------------------------
    // CHUNK 4: LIVE HARDWARE METRICS POOL TELEMETRY
    // --------------------------------------------------------------------
    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">CPU</div><div class=\"stat-val\" style=\"color:#79c0ff\">%s</div></div>", chip_model_str);
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">Cores</div><div class=\"stat-val\" style=\"color:#79c0ff\">%d</div></div>", chip_info.cores);
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">Clock</div><div class=\"stat-val\" style=\"color:#79c0ff\">%u MHz</div></div>", (unsigned int)(esp_clk_cpu_freq() / 1000000));
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">Total Flash</div><div class=\"stat-val\" style=\"color:#79c0ff\">%u MB</div></div>", (unsigned int)(flash_size / (1024 * 1024)));
    httpd_resp_send_chunk(req, scratch, strlen(scratch));
    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">App Part.</div><div class=\"stat-val\" style=\"color:#79c0ff\">%.2f MB</div></div>", (float)app_total_bytes / (1024.0f * 1024.0f));
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">App Used</div><div class=\"stat-val\">%.2f MB (%.1f%%)</div></div>", (float)app_used_bytes / (1024.0f * 1024.0f), app_used_percent);
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">Total RAM</div><div class=\"stat-val\">%u KB</div></div>", (unsigned int)((heap_info.total_free_bytes + heap_info.total_allocated_bytes) / 1024));
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">Free Heap</div><div class=\"stat-val\">%u Bytes</div></div>", (unsigned int)free_heap);
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">Heap Frag.</div><div class=\"stat-val\">%.1f %%</div></div>", fragmentation_percentage);
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item\"><div class=\"stat-lbl\">Max Block</div><div class=\"stat-val\">%u Bytes</div></div>", (unsigned int)largest_free_block);
    httpd_resp_send_chunk(req, scratch, strlen(scratch));

    snprintf(scratch, sizeof(scratch), "<div class=\"diag-item span-2\"><div class=\"stat-lbl\">Free Stack Space</div><div class=\"stat-val\">%u Bytes</div></div>", (unsigned int)uxTaskGetStackHighWaterMark(NULL));
    httpd_resp_send_chunk(req, scratch, strlen(scratch));
    
    // --------------------------------------------------------------------
    // CHUNK 5: FOOTER CLOSE & PIPELINE SIGNOFF
    // --------------------------------------------------------------------
    httpd_resp_send_chunk(req, R"rawliteral(    </div>
  </div>
  <div style="margin-top:12px; font-size:var(--fs-sm); border-top:1px solid #21262d; padding-top:10px">
    <div style="margin-bottom:4px"><span style="color:#8b949e">Project Version:</span> <span style="font-family:monospace;color:#79c0ff">)rawliteral", HTTPD_RESP_USE_STRLEN);

    httpd_resp_send_chunk(req, ESPHOME_PROJECT_VERSION, strlen(ESPHOME_PROJECT_VERSION));

    httpd_resp_send_chunk(req, R"rawliteral(</span></div>
  </div>
</div>
</body></html>)rawliteral", HTTPD_RESP_USE_STRLEN);
    
    // Finalize response pipeline stream signoff with a trailing NULL block
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}



// ====================================================================
// HIGH-EFFICIENCY ZERO-ALLOCATION HTTP GET SELECT LAYOUT HANDLER
// ====================================================================
inline esp_err_t select_handler(httpd_req_t *req) {
    // 256-byte stack-allocated query buffer is more than enough for URI params
    char query_buf[256];
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;

    if (buf_len > 1 && buf_len <= sizeof(query_buf)) {
        if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) == ESP_OK) {
            char param[32];
            if (httpd_query_key_value(query_buf, "slot", param, sizeof(param)) == ESP_OK) {
                int chosen_slot = atoi(param);

                // Commit slot indices straight to runtime globals
                esphome::id(active_remote_layout).value() = chosen_slot;
                esphome::id(profile_has_been_stored).value() = 1; 
                
                load_profile_to_workspace(chosen_slot);
                esphome::id(setup_ir_receiver_for_current_profile).execute();
                
                // Print execution state changes directly into stack log string 
                char change_buf[96];
                const char* kind = (chosen_slot < (int)factory_count) ? "internal" : "custom";
                snprintf(change_buf, sizeof(change_buf), "idx=%d  %.48s  [%s]",
                         chosen_slot, active_profile_workspace.profile_name.c_str(), kind);
                esphome::id(ir_active_profile_ts).publish_state(change_buf);
            }
        }
    }

    // Zero-overhead 303 Redirect header frame construction
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


// ====================================================================
// HIGH-PERFORMANCE ZERO-HEAP CHUNKED STREAMING PROFILE CSV EXPORT HANDLER
// ====================================================================
inline esp_err_t export_handler(httpd_req_t *req) {
    int target_slot = 0;
    char query_buf[128];
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;

    if (buf_len > 1 && buf_len <= sizeof(query_buf)) {
        if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(query_buf, "slot", param, sizeof(param)) == ESP_OK) {
                target_slot = atoi(param);
            }
        }
    }

    // Capture operational layout state and load requested data targets
    int current_active = esphome::id(active_remote_layout).value();
    load_profile_to_workspace(target_slot);

    const auto& p = active_profile_workspace;
    httpd_resp_set_type(req, "text/csv");
    
    // SANITIZATION WORKER: Clean up spaces from the profile name for web transmission
    char sanitized_name[32] = {0};
    std::strncpy(sanitized_name, p.profile_name.c_str(), sizeof(sanitized_name) - 1);
    for (size_t i = 0; i < std::strlen(sanitized_name); i++) {
        if (sanitized_name[i] == ' ') {
            sanitized_name[i] = '_';
        }
    }

    // Format the clean customized profile filename package (0 heap churn)
    char header_buf[128];
    snprintf(header_buf, sizeof(header_buf), 
             "attachment; filename=profile_slot_%d_%s.csv", 
             target_slot, sanitized_name);
    httpd_resp_set_hdr(req, "Content-Disposition", header_buf);

    // Protocol-specific formatting masks
    const char* addr_fmt = "%04X";
    const char* key_fmt  = "%X";

    if (p.protocol == PROTO_NEC) {
        addr_fmt = "%04X"; key_fmt = "%02X";
    } else if (p.protocol == PROTO_JVC) {
        addr_fmt = "%04X"; key_fmt = "%04X";
    } else if (p.protocol == PROTO_SONY) {
        addr_fmt = "%04X"; key_fmt = "%05X";
    } else if (p.protocol == PROTO_PANASONIC) {
        addr_fmt = "%04X"; key_fmt = "%07X";
    }

    char chunk_buf[256];

    // 1. Stream the META row directly
    char addr_buf[16], arm_buf[16], fire_buf[16];
    snprintf(addr_buf, sizeof(addr_buf), addr_fmt, (unsigned int)p.device_address);
    snprintf(arm_buf, sizeof(arm_buf), key_fmt, (unsigned int)p.cmd_clear_token_arm);
    snprintf(fire_buf, sizeof(fire_buf), key_fmt, (unsigned int)p.cmd_clear_token_fire);
    
    snprintf(chunk_buf, sizeof(chunk_buf), "META,%d,%s,%s,%s,%s,%s\n", 
             target_slot, p.profile_name.c_str(), to_string(p.protocol), addr_buf, arm_buf, fire_buf);
    httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));

    // 2. Stream key row allocations sequentially (0 heap allocation overhead)
    for (size_t i = 0; i < p.cmd_codes.size(); i++) {
        char irCommand_buf[16];
        snprintf(irCommand_buf, sizeof(irCommand_buf), key_fmt, (unsigned int)p.cmd_codes[i].first);
        
        // Symmetrically aligned to export Field 2 (action_string) and Field 3 (button_name)
        snprintf(chunk_buf, sizeof(chunk_buf), "KEY,%s,%s,%s\n",
                 irCommand_buf, p.cmd_codes[i].second.action_string, p.cmd_codes[i].second.button_name);
        httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));
    }

    // 3. Finalize stream pipeline signature
    httpd_resp_send_chunk(req, NULL, 0);

    // Restore workspace configuration state back to normal
    if (target_slot != current_active) {
        load_profile_to_workspace(current_active);
    }

    return ESP_OK;
}


// ====================================================================
// LOW-HEAP PRE-FILTERED STREAM-ACCUMULATING PROFILE CSV IMPORT HANDLER
// ====================================================================
inline esp_err_t import_handler(httpd_req_t *req) {
    int target_slot = 0;
    char query_buf[128];
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > 1 && query_len <= sizeof(query_buf)) {
        if (httpd_req_get_url_query_str(req, query_buf, query_len) == ESP_OK) {
            char param[32]; 
            if (httpd_query_key_value(query_buf, "slot", param, sizeof(param)) == ESP_OK) {
                target_slot = atoi(param); 
            }
        }
    }

    size_t total_bytes = req->content_len;
    if (total_bytes == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File payload is completely empty.");
        return ESP_FAIL;
    }

    std::string clean_csv;
    // Pre-allocate exactly what we expect for a maximum layout spec block
    clean_csv.reserve(total_bytes > 3072 ? 3072 : total_bytes);

    char chunk_buf[512]; 
    int received = 0;
    size_t remaining = total_bytes;
    bool inside_payload = false;

    while (remaining > 0) {
        size_t read_target = (remaining < sizeof(chunk_buf)) ? remaining : sizeof(chunk_buf);
        if ((received = httpd_req_recv(req, chunk_buf, read_target)) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        remaining -= received;

        // Process incoming bytes sequentially to strip multipart components
        for (int i = 0; i < received; i++) {
            char c = chunk_buf[i];
            if (!inside_payload) {
                clean_csv.push_back(c);
                if (clean_csv.size() >= 5) {
                    if (clean_csv.rfind("META,", clean_csv.size() - 5) != std::string::npos) {
                        // Isolate the start index and clear out any leading browser junk
                        clean_csv = "META,";
                        inside_payload = true;
                    }
                }
            } else {
                clean_csv.push_back(c);
            }
        }
    }

    // Strip out the multi-part trailing browser footer boundaries cleanly
    size_t end_pos = clean_csv.rfind("\n-");
    if (end_pos != std::string::npos) {
        clean_csv.resize(end_pos);
    }

    if (import_profile_from_csv(clean_csv)) {
        link_hardware_buttons();
        esphome::id(setup_ir_receiver_for_current_profile).execute();
        
        int final_custom_slot = target_slot - (int)factory_count;
        if (final_custom_slot < 0 || final_custom_slot >= MAX_LEARNED_PROFILES) {
            final_custom_slot = 0; 
            esphome::id(active_remote_layout).value() = (int)factory_count;
        } else {
            esphome::id(active_remote_layout).value() = target_slot;
        }
        
        commit_database_to_flash(final_custom_slot);
        esphome::id(display_show).execute(true, "CSV Web Uploaded!", "Profile Operational", active_profile_workspace.profile_name);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "CSV processing engine failure.");
        return ESP_FAIL;
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

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
            snprintf(chunk, sizeof(chunk), "step,%s,%s,%u\n", 
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
// HIGH-PERFORMANCE ZERO-HEAP CHUNKED STREAMING MACRO CSV EXPORT HANDLER
// ====================================================================
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

        // Dynamic lookup to match the human-readable action string to the current slot id
        const char* human_readable_label = nullptr;
        for (uint16_t b = 0; b < global_binding_registry.total_bound_keys; b++) {
            if (global_binding_registry.bindings[b].shared_macro_slot == slot_id) {
                human_readable_label = global_binding_registry.bindings[b].action_string;
                break;
            }
        }

        if (human_readable_label == nullptr || std::strlen(human_readable_label) == 0) {
            snprintf(chunk_buf, sizeof(chunk_buf), "MACRO,Macro_Slot_%d\n", slot_id);
        } else {
            snprintf(chunk_buf, sizeof(chunk_buf), "MACRO,%s\n", human_readable_label);
        }
        httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));

        for (uint16_t i = 0; i < macro_buf.total_steps; i++) {
            const auto& step = macro_buf.steps[i];
            snprintf(chunk_buf, sizeof(chunk_buf), "step,%s,%s,%u\n", 
                     step.action_string, state_to_str(step.event_state), step.delay_ms);
            httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));
        }
    }

    if (!found_any_data) {
        snprintf(chunk_buf, sizeof(chunk_buf), "MACRO,Empty_Suite\n");
        httpd_resp_send_chunk(req, chunk_buf, strlen(chunk_buf));
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


// ====================================================================
// ZERO-HEAP IN-PLACE TEXT TOKENIZATION CSV MACRO DEPLOYMENT ENGINE
// ====================================================================
inline bool import_macro_from_csv(const std::string& csv_data) {
    if (csv_data.empty()) return false;

    static UniversalFlashMacro macro_build;
    std::memset(&macro_build, 0, sizeof(macro_build));
    macro_build.struct_version = CURRENT_MACRO_VERSION;
    
    int active_slot = -1;

    auto save_active_macro = [&]() {
        if (active_slot >= 0 && active_slot < MAX_BOUND_HOTKEYS && macro_build.total_steps > 0) {
            uint64_t macro_nvs_key = 384720194ULL + active_slot;
            auto pref_obj = esphome::global_preferences->make_preference<UniversalFlashMacro>(macro_nvs_key);
            pref_obj.save(&macro_build);
            ESP_LOGI("MACRO_CSV", "Committed slot %d structure registry (%d steps)", active_slot, macro_build.total_steps);
        }
    };

    size_t line_start = 0;
    while (line_start < csv_data.size()) {
        size_t line_end = csv_data.find('\n', line_start);
        if (line_end == std::string::npos) line_end = csv_data.size();

        std::string_view line_view(&csv_data[line_start], line_end - line_start);
        line_start = line_end + 1;

        if (!line_view.empty() && line_view.back() == '\r') {
            line_view.remove_suffix(1);
        }
        if (line_view.empty()) continue;

        auto get_next_cell = [](std::string_view& src) -> std::string_view {
            if (src.empty()) return std::string_view{};
            size_t comma_pos = src.find(',');
            if (comma_pos == std::string::npos) {
                std::string_view ret = src;
                src = std::string_view{};
                return ret;
            }
            std::string_view ret = src.substr(0, comma_pos);
            src.remove_prefix(comma_pos + 1);
            return ret;
        };

        std::string_view cell_type = get_next_cell(line_view);

        // -----------------------------------------------------------
        // METADATA WORKFLOW DEFINITION PASS (Handles MACRO or macro)
        // -----------------------------------------------------------
        if (cell_type == "MACRO" || cell_type == "macro") {
            save_active_macro(); // Flush any previously collected macro steps to flash before switching slots
            
            std::string_view label_view = get_next_cell(line_view);
            active_slot = -1; // Reset tracking pointer
        
            if (!label_view.empty()) {
                char search_name[MAX_ACTION_STRING_LEN] = {0};
                std::memcpy(search_name, label_view.data(), std::min(label_view.size(), sizeof(search_name) - 1));
        
                // Resolve slot target via active hotkey action string matches
                for (uint16_t b = 0; b < global_binding_registry.total_bound_keys; b++) {
                    if (std::strcmp(global_binding_registry.bindings[b].action_string, search_name) == 0) {
                        active_slot = global_binding_registry.bindings[b].shared_macro_slot;
                        break;
                    }
                }
        
                // Legacy Fallback Check
                if (active_slot == -1 && std::strncmp(search_name, "Macro_Slot_", 11) == 0) {
                    active_slot = std::atoi(search_name + 11);
                }
            }
            
            std::memset(&macro_build, 0, sizeof(macro_build));
            macro_build.struct_version = CURRENT_MACRO_VERSION; 
        }
        // -----------------------------------------------------------
        // HARDWARE DATA KEY EXTRAPOLATION PASS (Handles STEP or step)
        // -----------------------------------------------------------
        else if (cell_type == "step" || cell_type == "STEP") {
            if (active_slot == -1 || macro_build.total_steps >= MAX_MACRO_STEPS) continue;
            
            std::string_view payload_view = get_next_cell(line_view); 
            std::string_view state_view   = get_next_cell(line_view);
            std::string_view delay_view   = get_next_cell(line_view);
        
            auto& step = macro_build.steps[macro_build.total_steps];
            
            char state_tmp[16] = {0};
            std::memcpy(state_tmp, state_view.data(), std::min(state_view.size(), sizeof(state_tmp) - 1));
            step.event_state = str_to_state(state_tmp);
        
            char delay_tmp[16] = {0};
            std::memcpy(delay_tmp, delay_view.data(), std::min(delay_view.size(), sizeof(delay_tmp) - 1));
            step.delay_ms = std::strtoul(delay_tmp, nullptr, 10);
        
            std::memset(step.action_string, 0, MAX_ACTION_STRING_LEN);
            std::memcpy(step.action_string, payload_view.data(), std::min(payload_view.size(), MAX_ACTION_STRING_LEN - 1));
        
            macro_build.total_steps++;
        }
    }
    
    save_active_macro(); 
    esphome::global_preferences->sync();
    return true;
}

// ====================================================================
// LOW-HEAP PRE-FILTERED STREAM-ACCUMULATING MACRO IMPORT HANDLER
// ====================================================================
inline esp_err_t import_macro_text_handler(httpd_req_t *req) {
    size_t total_bytes = req->content_len;
    if (total_bytes == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload is completely empty.");
        return ESP_FAIL;
    }

    std::string clean_macro_csv;
    clean_macro_csv.reserve(total_bytes > 4096 ? 4096 : total_bytes);

    char chunk_buf[512];
    int received = 0;
    size_t remaining = total_bytes;
    bool inside_payload = false;

    while (remaining > 0) {
        size_t target = (remaining < sizeof(chunk_buf)) ? remaining : sizeof(chunk_buf);
        if ((received = httpd_req_recv(req, chunk_buf, target)) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        remaining -= received;

        for (int i = 0; i < received; i++) {
            char c = chunk_buf[i];
            if (!inside_payload) {
                clean_macro_csv.push_back(c);
                if (clean_macro_csv.size() >= 6) {
                    if (clean_macro_csv.rfind("MACRO,", clean_macro_csv.size() - 6) != std::string::npos) {
                        clean_macro_csv = "MACRO,";
                        inside_payload = true;
                    }
                }
            } else {
                clean_macro_csv.push_back(c);
            }
        }
    }

    size_t end_pos = clean_macro_csv.find("\r\n------");
    if (end_pos == std::string::npos) {
        end_pos = clean_macro_csv.find("\n------");
    }
    if (end_pos != std::string::npos) {
        clean_macro_csv.resize(end_pos);
    }

    if (import_macro_from_csv(clean_macro_csv)) {
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Macro engine compilation breakdown.");
    return ESP_FAIL;
}

// ====================================================================
// HIGH-PERFORMANCE WEB CONSOLE ACTION INTERCEPTOR & INJECTION GATE
// ====================================================================
inline esp_err_t button_inject_handler(httpd_req_t *req) {
    char query_buf[128] = {0};    
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > 1 && query_len <= sizeof(query_buf)) {
        if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) == ESP_OK) {
            char action_param[32] = {0};
            if (httpd_query_key_value(query_buf, "action", action_param, sizeof(action_param)) == ESP_OK) {
                ESP_LOGI("WEB_CONSOLE", "Web Console Intercept: Firing Action Token '%s'", action_param);
                
                bool is_prefix_listening = esphome::id(macro_play_prefix_active).value();
                uint32_t workflow_state = esphome::id(macro_workflow_state).value();
                
                bool route_to_state_machine = is_prefix_listening || 
                                              (workflow_state == MACRO_STATE_RECORDING) || 
                                              (workflow_state == MACRO_STATE_AWAITING_HOTKEY);

                if (route_to_state_machine) {
                    // Route inbound web clicks directly as KEY_EVENT_DOWN actions to the state engine 
                    // when an automation session is actively listening for payload keys or binding assignments.
                    uint32_t current_hex_mapping = find_active_command_by_name(action_param);
                    esphome::id(ir_map_processor).execute(current_hex_mapping, 0); // 0 = DOWN / Edge Press
                    
                    // ====================================================================
                    // NEW CORRECTION: ANTI-LOCKUP WEB CLEARING GATE (C++ TYPED EDITION)
                    // If the macro engine just finished assigning a button via the web console,
                    // it will land in state 99 waiting for a physical button release.
                    // We forcibly break out back to IDLE via .value() to ensure immediate availability.
                    // ====================================================================
                    if (esphome::id(macro_workflow_state).value() == MACRO_STATE_LIFTOFF_SHIELD) {
                        ESP_LOGI("WEB_CONSOLE", "Web assignment completed. Safely releasing software execution lock states.");
                        esphome::id(macro_workflow_state).value() = MACRO_STATE_IDLE;
                        
                        esphome::id(global_cached_command).value() = 0;
                        esphome::id(global_initial_press_time).value() = 0;
                        esphome::id(global_last_processed_time).value() = 0;
                        esphome::id(global_is_holding).value() = false;
                    }
                
                    httpd_resp_set_status(req, "204 No Content");
                    httpd_resp_send(req, NULL, 0);
                    return ESP_OK;
                }

                // Update physical screen details to log the incoming command transaction
                std::string resolved_display_str = std::string(action_param); 
                for (const auto& kv_pair : active_profile_workspace.cmd_codes) {
                    if (std::strcmp(kv_pair.second.action_string, action_param) == 0) {
                        resolved_display_str = kv_pair.second.action_string;
                        break;
                    }
                }
                resolved_display_str += " [web]";
                esphome::id(display_show).execute(false, resolved_display_str, active_profile_workspace.profile_name, "BLE Web Command");

                // ====================================================================
                // AUTOMATION STATE ENGINES REDIRECT ROUTINES
                // ====================================================================
                if (std::strcmp(action_param, "macro_play") == 0) {
                    ESP_LOGI("WEB_CONSOLE", "Web Redirect: Engaging Macro Playback Sequence Tracker");
                    uint32_t simulated_hex = find_active_command_by_name("macro_play");
                    esphome::id(ir_map_processor).execute(simulated_hex, 0); // 0 = DOWN (Opens prefix window)
                    
                    httpd_resp_set_status(req, "204 No Content");
                    httpd_resp_send(req, NULL, 0);
                    return ESP_OK;
                }
                else if (std::strcmp(action_param, "macro_record") == 0) {
                    ESP_LOGI("WEB_CONSOLE", "Web Redirect: Toggling Macro Recording State Machine");
                    uint32_t simulated_hex = find_active_command_by_name("macro_record");
                    esphome::id(ir_map_processor).execute(simulated_hex, 0); // 0 = DOWN (Starts or steps to assignment)
                    
                    httpd_resp_set_status(req, "204 No Content");
                    httpd_resp_send(req, NULL, 0);
                    return ESP_OK;
                }


                // ====================================================================
                // WEB BUTTON BLUETOOTH TOKEN DIAGNOSTIC INTERCEPT LAYER
                // ====================================================================
                if (std::strcmp(action_param, "token_sniff") == 0) {
                    ESP_LOGI("WEB_CONSOLE", "Web Intercept: Arming Token Sniffer Stack");
                    esphome::id(showing_special_info).value() = true; // Lock display text protection
                    esphome::id(sniffing_sequence).execute(); 
                    
                    httpd_resp_set_status(req, "204 No Content");
                    httpd_resp_send(req, NULL, 0);
                    return ESP_OK;
                } 
                else if (std::strcmp(action_param, "token_recall") == 0) {
                    ESP_LOGI("WEB_CONSOLE", "Web Intercept: Executing Wake Token Recall");
                    esphome::id(showing_special_info).value() = true; // Lock display text protection
                    esphome::id(execute_token_recall_routine).execute();
                    
                    httpd_resp_set_status(req, "204 No Content");
                    httpd_resp_send(req, NULL, 0);
                    return ESP_OK;
                } 
                else if (std::strcmp(action_param, "token_clear") == 0) {
                    ESP_LOGI("WEB_CONSOLE", "Web Intercept: Initiating Wake Token Wipe");
                    esphome::id(run_token_clear_worker).execute();
                    
                    httpd_resp_set_status(req, "204 No Content");
                    httpd_resp_send(req, NULL, 0);
                    return ESP_OK;
                }

                // ====================================================================
                // MONOSTABLE PROGRAMMATIC ACTIONS INTERCEPT GATE
                // ====================================================================
                bool is_monostable_web_key = (std::strcmp(action_param, "power_off") == 0 || 
                                              std::strcmp(action_param, "focus_manual") == 0 ||
                                              std::strcmp(action_param, "focus_auto") == 0 ||
                                              std::strcmp(action_param, "power_on") == 0);

                if (is_monostable_web_key) {
                    auto* target_btn = resolve_button(action_param);
                    if (target_btn != nullptr) {
                        target_btn->press(); 
                        ESP_LOGI("WEB_CONSOLE", "Web redirected straight to native programmatic button for: %s", action_param);
                    } else {
                        // Fallback handling if native platform component entity reference is missing
                        fire_bluetooth_hid_action(action_param, 0);
                    }
                    
                    httpd_resp_set_status(req, "204 No Content");
                    httpd_resp_send(req, NULL, 0);
                    return ESP_OK;
                }

                // ====================================================================
                // STANDARD PASS-THROUGH PIPELINE FOR STATELESS KEYS ONLY
                // ====================================================================
                if (std::strcmp(action_param, "power_off") != 0 && 
                    std::strcmp(action_param, "focus_manual") != 0 && 
                    std::strcmp(action_param, "focus_auto") != 0 &&
                    std::strcmp(action_param, "power_on") != 0 &&
                    std::strcmp(action_param, "macro_play") != 0 &&
                    std::strcmp(action_param, "macro_record") != 0) {
                    
                    fire_bluetooth_hid_action(action_param, 0); // DOWN / Press edge
                    vTaskDelay(pdMS_TO_TICKS(60));              
                    fire_bluetooth_hid_action(action_param, 2); // UP / Release edge
                }
            }
        }
    }
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
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

    httpd_uri_t button_inject_uri = {
        .uri       = "/button_inject",
        .method    = HTTP_GET,
        .handler   = button_inject_handler,
        .user_ctx  = NULL
    };



    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &select_uri);
        httpd_register_uri_handler(server, &export_uri);
        httpd_register_uri_handler(server, &import_uri); 
        httpd_register_uri_handler(server, &export_macro_txt_uri);
        httpd_register_uri_handler(server, &import_macro_txt_uri);
        httpd_register_uri_handler(server, &button_inject_uri); // Added registration hook
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

