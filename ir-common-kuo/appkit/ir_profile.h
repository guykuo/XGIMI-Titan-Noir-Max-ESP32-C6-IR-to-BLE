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

// --- HUMAN-READABLE PROTOCOL FOOTPRINTS ---
#define PROTO_UNKNOWN   0
#define PROTO_NEC       1
#define PROTO_JVC       2
#define PROTO_SONY      3
#define PROTO_LG        4
#define PROTO_PANASONIC 5
#define PROTO_RC5       6
#define PROTO_RC6       7

// --- FIXED-SIZE CHARACTER BUFFER SIZE ALIASES ---
typedef char XgimiBtnStr[32];   // Field 2: Visible Xgimi Token (e.g., "game_menu")
typedef char ButtonNameStr[32]; // Field 3: Hidden Physical Remote Comment (e.g., "Cinema Master")
typedef char ProfileNameStr[32];
typedef char ComponentBufferStr[128];

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

// FIXED: Converted definition into a minified inline function body to instantly resolve the linker mismatch
inline bool import_profiles_from_json(const std::string& json_data) {
    ESP_LOGE("JSON Import", "Legacy JSON importer is deactivated to protect memory pools.");
    return false;
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

// ====================================================================
// 1. THE 3-FIELD ACTIVE RUNTIME ROW STRUCTURE
// ====================================================================
struct IRCommand {
  XgimiBtnStr name;          // Field 2: Internal Xgimi action name / user-visible text (e.g., "power_on")
  ButtonNameStr button_name; // Field 3: HIDDEN physical remote button comment (e.g., "Play")
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

// Global tracking configuration variables
inline size_t factory_count = 13; 
inline constexpr uint16_t MAX_LEARNED_PROFILES = 5;
inline bool flash_hydration_complete = false;

inline constexpr uint32_t CURRENT_STRUCT_VERSION = 57; // <===== current version factory struct


// ====================================================================
// COMPILER BRIDGING STRUCTURE FOR LEGACY YAMLS
// ====================================================================
struct RemoteProfilesBridge {
    // Allows remote_profiles.size() to compile and return total layouts safely
    size_t size() const { return factory_count + MAX_LEARNED_PROFILES; }
    
    // Safety Fallback Helpers: Catches both standard .empty() and `.empt` typos inside YAML files
    bool empty() const { return false; }
    bool empt() const { return false; }
    
    // Allows remote_profiles[idx] to compile and return the active workspace context
    const IRProfile& operator[](size_t idx) const { return active_profile_workspace; }
    IRProfile& operator[](size_t idx) { return active_profile_workspace; }
};

inline RemoteProfilesBridge remote_profiles;
// ====================================================================


// ====================================================================
// 2. THE THREE-FIELD FLASH PERSISTENCE LAYER (NVS REGISTER SETS)
// ====================================================================
struct FlashStoredKey {
  uint32_t hex_code; 
  XgimiBtnStr target_button_id; // Maps straight to Field 2 (Xgimi Action)
  ButtonNameStr button_name;    // Maps straight to Field 3 (Hidden Comment)
};

struct FlashStoredProfile {
  uint32_t struct_version; 
  ProfileNameStr profile_name;
  uint8_t protocol;              
  uint8_t reserved_padding[3];   // Padding to maintain strict 32-bit alignment structure
  uint32_t device_address;
  uint32_t cmd_clear_token_arm;  
  uint32_t cmd_clear_token_fire; 
  uint16_t total_keys;
  FlashStoredKey keys[55];       // Centralized configuration ceiling cap
};

// --- SINGLE SOURCE OF TRUTH FOR BUTTON NAMES ---
inline constexpr const char* learn_button_names[] = {
  "power_on", "power_off", "cursor_left", "cursor_right", "cursor_up", "cursor_down",
  "cursor_enter", "settings_menu", "back", "home", "game_menu", "input", "picture",
  "focus_manual", "focus_auto", "shortcut_1", "shortcut_2", "shortcut_3", "shortcut_4",
  "volume_up", "volume_down", "mute", "token_sniff", "token_clear", "token_recall",
  "BT_start_pair", "BT_clear_pair"
};

inline constexpr size_t TOTAL_LEARN_BUTTONS = sizeof(learn_button_names) / sizeof(learn_button_names[0]);

// ====================================================================
// 3. INTERNAL LINKER EXTRACTION HELPER
// ====================================================================
inline esphome::button::Button* resolve_button(const char* name) {
  for (auto* btn : esphome::App.get_buttons()) {
    ComponentBufferStr buffer = {0}; 
    std::span<char, 128> buf_span(buffer);
    esphome::StringRef id_ref = btn->get_object_id_to(buf_span);
    
    if (std::strcmp(id_ref.c_str(), name) == 0) {
      return btn;
    }
  }
  return nullptr; 
}

// ====================================================================
// 4. LIGHTWEIGHT INITIALIZATION ROW BUILDER HELPER
// ====================================================================
inline void add_cmd(uint32_t hex_code, const char* xgimi_id, const char* comment) {
    IRCommand cmd;
    std::strncpy(cmd.name, xgimi_id, sizeof(cmd.name) - 1);
    cmd.name[sizeof(cmd.name) - 1] = '\0'; // FIXED: Cleared literal space character loop tracker warning
    
    std::strncpy(cmd.button_name, comment, sizeof(cmd.button_name) - 1);
    cmd.button_name[sizeof(cmd.button_name) - 1] = '\0'; // FIXED: Cleared literal space character loop tracker warning

    active_profile_workspace.cmd_codes.push_back({ hex_code, cmd });
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
  flash_p.struct_version = CURRENT_STRUCT_VERSION; //struct layout signature version
  
  std::strncpy(flash_p.profile_name, active_profile_workspace.profile_name.c_str(), sizeof(flash_p.profile_name) - 1);
  flash_p.protocol = active_profile_workspace.protocol; 
  flash_p.device_address = active_profile_workspace.device_address;
  flash_p.cmd_clear_token_arm = active_profile_workspace.cmd_clear_token_arm;
  flash_p.cmd_clear_token_fire = active_profile_workspace.cmd_clear_token_fire;

  uint16_t k_idx = 0;
  for (const auto& kv_pair : active_profile_workspace.cmd_codes) {
    if (k_idx >= 55) break; 
    flash_p.keys[k_idx].hex_code = kv_pair.first;
    std::strncpy(flash_p.keys[k_idx].target_button_id, kv_pair.second.name, sizeof(flash_p.keys[k_idx].target_button_id) - 1);
    std::strncpy(flash_p.keys[k_idx].button_name, kv_pair.second.button_name, sizeof(flash_p.keys[k_idx].button_name) - 1);
    k_idx++;
  }
  flash_p.total_keys = k_idx;
  pref_obj.save(&flash_p);
  esphome::global_preferences->sync();
}

// THE UNIFIED ON-DEMAND DYNAMIC HYDRATION ENGINE
inline void load_profile_to_workspace(int idx) {
  active_profile_workspace.cmd_codes.clear();
  int factory = static_cast<int>(factory_count);

  if (idx < factory) {
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 0: MAPPING FOR THE AWOL PROJECTOR
    // -----------------------------------------------------------
    if (idx == 0) {
        active_profile_workspace.profile_name = "AWOL Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0x7300;
        active_profile_workspace.cmd_clear_token_arm = 0x17;
        active_profile_workspace.cmd_clear_token_fire = 0x14;

        // Syntax: add_cmd( Hex Code, Visible Xgimi Token, Hidden Remote Comment );
        add_cmd( 0xA7, "power_on",      "power on" );
        add_cmd( 0x67, "power_off",     "power off" );
        add_cmd( 0x24, "cursor_left",   "left arrow" );
        add_cmd( 0xA4, "cursor_right",  "right arrow" );
        add_cmd( 0x64, "cursor_up",     "up arrow" );
        add_cmd( 0xE4, "cursor_down",   "down arrow" );
        add_cmd( 0x14, "cursor_enter",  "ok" );
        add_cmd( 0x5A, "settings_menu", "menu" );
        add_cmd( 0x3A, "back",          "back" );
        add_cmd( 0xDA, "home",          "home" );
        add_cmd( 0x1B, "game_menu",     "profile" );
        add_cmd( 0x48, "input",         "input" );
        add_cmd( 0xCA, "picture",       "picture mode" );
        add_cmd( 0x9B, "focus_manual",  "focus" );
        add_cmd( 0x47, "focus_auto",    "live guide" );
        add_cmd( 0xBB, "shortcut_1",    "Prime Video" );
        add_cmd( 0x3B, "shortcut_2",    "Netflix" );
        add_cmd( 0x7B, "shortcut_3",    "Disney" );
        add_cmd( 0xDB, "shortcut_4",    "YouTube" );
        add_cmd( 0x50, "volume_up",     "volume up" );
        add_cmd( 0xD0, "volume_down",   "volume down" );
        add_cmd( 0xD8, "mute",          "mute" );
        add_cmd( 0xE7, "token_sniff",   "HDMI 1" );
        add_cmd( 0x17, "token_clear",   "HDMI 2" );
        add_cmd( 0x97, "token_recall",  "HDMI 3" );
        add_cmd( 0x07, "BT_start_pair", "Back + Down" );
        add_cmd( 0xC7, "BT_clear_pair", "Back + Home" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 1: MAPPING FOR THE BENQ W5800
    // -----------------------------------------------------------
    else if (idx == 1) { 
        active_profile_workspace.profile_name = "BenQ Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0x3000;
        active_profile_workspace.cmd_clear_token_arm = 0x629D;
        active_profile_workspace.cmd_clear_token_fire = 0xEA15;
        
        add_cmd( 0xB04F, "power_on",      "power on" );
        add_cmd( 0xB14E, "power_off",     "power off" );
        add_cmd( 0xF40B, "cursor_up",     "up arrow" );
        add_cmd( 0xF30C, "cursor_down",   "down arrow" );
        add_cmd( 0xF20D, "cursor_left",   "left arrow" );
        add_cmd( 0xF10E, "cursor_right",  "right arrow" );
        add_cmd( 0xEA15, "cursor_enter",  "ok" );
        add_cmd( 0xF00F, "settings_menu", "menu" );
        add_cmd( 0x7A85, "back",          "back" );
        add_cmd( 0x8778, "home",          "default" );
        add_cmd( 0x41BE, "game_menu",     "cinema master" ); 
        add_cmd( 0xFB04, "input",         "source" );
        add_cmd( 0xEF10, "picture",       "picure mode" );
        add_cmd( 0xEC13, "focus_manual",  "aspect" );
        add_cmd( 0xF708, "focus_auto",    "auto" );
        add_cmd( 0xE916, "shortcut_1",    "brightness" );
        add_cmd( 0xEE11, "shortcut_2",    "contrast" );
        add_cmd( 0x837C, "shortcut_3",    "dynamic iris" );
        add_cmd( 0xCF30, "shortcut_4",    "light mode" );
        add_cmd( 0xA15E, "volume_up",     "gamma" );
        add_cmd( 0x817E, "volume_down",   "sharp" );
        add_cmd( 0xF807, "mute",          "eco blank" );
        add_cmd( 0xC33C, "token_sniff",   "HDR" );
        add_cmd( 0x629D, "token_clear",   "invert" );
        add_cmd( 0x639C, "token_recall",  "3D" );
        add_cmd( 0xA05F, "BT_start_pair", "color temp" );
        add_cmd( 0xA45B, "BT_clear_pair", "color manage" );
        add_cmd( 0x6B94, "home",          "test pattern" ); 
    }
        // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 2: EPSON PRO CINEMA LS12000
    // -----------------------------------------------------------
    else if (idx == 2) {
        active_profile_workspace.profile_name = "Epson Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0x5583;
        active_profile_workspace.cmd_clear_token_arm = 0xC23D;
        active_profile_workspace.cmd_clear_token_fire = 0x7A85;

        add_cmd( 0x6F90, "power_on",         "Power On" );
        add_cmd( 0x6E91, "power_off",        "Power Off" );
        add_cmd( 0x4FB0, "cursor_up",        "up arrow" );
        add_cmd( 0x4DB2, "cursor_down",      "down arrow" );
        add_cmd( 0x4CB3, "cursor_left",      "left arrow" );
        add_cmd( 0x4EB1, "cursor_right",     "right arrow" );
        add_cmd( 0x7A85, "cursor_enter",     "enter" );
        add_cmd( 0x659A, "settings_menu",    "menu" );
        add_cmd( 0x7B84, "back",             "ESC" );
        add_cmd( 0xC639, "home",             "default" );
        add_cmd( 0x708F, "game_menu",        "color mode" );
        add_cmd( 0xA956, "input",            "HDMI link" );
        add_cmd( 0x55AA, "picture",          "image enhance" );
        add_cmd( 0xA25D, "focus_manual",     "skip back" );
        add_cmd( 0x728D, "focus_manual",     "lens NH" );
        add_cmd( 0xA45B, "focus_auto",       "pause" );
        add_cmd( 0xA55A, "shortcut_1",       "reverse" );
        add_cmd( 0xA15E, "shortcut_2",       "play" );
        add_cmd( 0xA35C, "shortcut_3",       "FF" );
        add_cmd( 0xA05F, "shortcut_4",       "skip forward" );
        add_cmd( 0x8C73, "shortcut_1",       "HDMI 1" );
        add_cmd( 0x8877, "shortcut_2",       "HDMI 2" );
        add_cmd( 0x7D82, "shortcut_3",       "P-in-P NH" );
        add_cmd( 0x629D, "shortcut_4",       "PC NH" );
        add_cmd( 0x6798, "volume_up",        "volume up" );
        add_cmd( 0x6699, "volume_down",      "volume down" );
        add_cmd( 0x52AD, "mute",             "mute" );
        add_cmd( 0x7C83, "token_sniff",      "frame interp" );
        add_cmd( 0xC23D, "token_clear",      "RGBCMY" );
        add_cmd( 0x6996, "token_recall",     "pattern" );
        add_cmd( 0xC43B, "BT_start_pair",    "3D format" );
        add_cmd( 0x758A, "BT_clear_pair",    "Aspect" );
        add_cmd( 0x6A95, "home",             "Home" );
        add_cmd( 0x8B74, "home",             "input LAN" );
        add_cmd( 0x609F, "home",             "user" );
        add_cmd( 0x9F60, "home",             "link menu" );
        add_cmd( 0x6C93, "home",             "blank NH" );
        add_cmd( 0x748B, "home",             "memory NH" );
        add_cmd( 0x51AE, "home",             "lens1 NH" );
        add_cmd( 0x50AF, "home",             "lens2 NH" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 3: MAPPING FOR THE HISENSE 50U6G TV
    // -----------------------------------------------------------
    else if (idx == 3) {
        active_profile_workspace.profile_name = "Hisense";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0xFB04;
        active_profile_workspace.cmd_clear_token_arm = 0xEA15;
        active_profile_workspace.cmd_clear_token_fire = 0xA55A;

        // Syntax: add_cmd( Field 1: Hex, Field 2: Visible Xgimi Token, Field 3: Hidden Comment );
        add_cmd( 0xF708, "power_on",      "power on" );
        add_cmd( 0x8E71, "power_on",      "power on" ); // Shared target action mapping
        add_cmd( 0xEF10, "power_off",     "power off" );
        add_cmd( 0x8D72, "power_off",     "0" );
        add_cmd( 0xA956, "cursor_up",     "arrow up" );
        add_cmd( 0xA857, "cursor_down",   "arrow down" );
        add_cmd( 0xA758, "cursor_left",   "arrow left" );
        add_cmd( 0xA659, "cursor_right",  "arrow right" );
        add_cmd( 0xA55A, "cursor_enter",  "select" );
        add_cmd( 0xFB04, "back",          "back" );
        add_cmd( 0xBC43, "home",          "home" );
        add_cmd( 0xF40B, "input",         "input" );
        add_cmd( 0xB54A, "settings_menu", "menu" );
        add_cmd( 0x718E, "settings_menu", "menu" );
        add_cmd( 0x35CA, "game_menu",     "apps" );
        add_cmd( 0xFF00, "picture",       "channel up" );
        add_cmd( 0xE817, "focus_manual",  "7" );
        add_cmd( 0xE718, "focus_auto",    "8" );
        add_cmd( 0xAB54, "shortcut_1",    "yellow" );
        add_cmd( 0xAA55, "shortcut_2",    "blue" );
        add_cmd( 0xAD52, "shortcut_3",    "red" );
        add_cmd( 0xAC53, "shortcut_4",    "green" );
        add_cmd( 0xFD02, "volume_up",     "volume up" );
        add_cmd( 0xFC03, "volume_down",   "volume down" );
        add_cmd( 0xF609, "mute",          "mute" );
        add_cmd( 0xEB14, "token_sniff",   "4" );
        add_cmd( 0xEA15, "token_clear",   "5" );
        add_cmd( 0xE916, "token_recall",  "6" );
        add_cmd( 0xB847, "BT_start_pair", "Prime Video" );
        add_cmd( 0xB649, "BT_clear_pair", "Youtube" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 4: JVC HR-S9600U VCR
    // -----------------------------------------------------------
    else if (idx == 4) {
        active_profile_workspace.profile_name = "JVC HR-S9600u VCR";
        active_profile_workspace.protocol = PROTO_JVC;
        active_profile_workspace.device_address = 0x03C2;
        active_profile_workspace.cmd_clear_token_arm = 0xC2A4;
        active_profile_workspace.cmd_clear_token_fire = 0xC23C;

        // Syntax: add_cmd( Field 1: Hex, Field 2: Visible Xgimi Token, Field 3: Hidden Comment );
        add_cmd( 0xC2D0, "power_on",      "power on" );
        add_cmd( 0xC2B8, "power_on",      "power on" );
        add_cmd( 0xC258, "power_off",     "power off" );
        add_cmd( 0xC2E8, "power_off",     "audio monitor" );
        add_cmd( 0xC2CC, "power_off",     "0" );
        add_cmd( 0xC2C3, "back",          "review" );
        add_cmd( 0xC241, "cursor_up",     "cursor up" );
        add_cmd( 0xC298, "cursor_up",     "cursor up H" );
        add_cmd( 0xC218, "cursor_down",   "cursor down" );
        add_cmd( 0xC261, "cursor_down",   "cursor down H" );
        add_cmd( 0xC2A8, "cursor_left",   "cursor left" );
        add_cmd( 0xC228, "cursor_right",  "cursor right H" );
        add_cmd( 0xC23C, "cursor_enter",  "OK" );
        add_cmd( 0xC2EC, "settings_menu", "menu" );
        add_cmd( 0xC207, "settings_menu", "memu" );
        add_cmd( 0xC26C, "home",          "cancel" );
        add_cmd( 0xC230, "game_menu",     "game menu" );
        add_cmd( 0xC2C8, "input",         "tv/vcr" );
        add_cmd( 0xC260, "picture",       "fast forward" );
        add_cmd( 0xC214, "focus_manual",  "8" );
        add_cmd( 0xC2E4, "focus_auto",    "7" );
        add_cmd( 0xC283, "shortcut_1",    "prog" );
        add_cmd( 0xC2BC, "shortcut_2",    "prog check" );
        add_cmd( 0xC28C, "shortcut_3",    "SP/EP" );
        add_cmd( 0xC269, "shortcut_4",    "skip search" );
        add_cmd( 0xC213, "volume_up",     "start down" );
        add_cmd( 0xC293, "volume_down",   "start up" );
        add_cmd( 0xC2B0, "mute",          "pause" );
        add_cmd( 0xC224, "token_sniff",   "4" );
        add_cmd( 0xC2A4, "token_clear",   "5" );
        add_cmd( 0xC264, "token_recall",  "6" );
        add_cmd( 0xC284, "BT_start_pair", "1" );
        add_cmd( 0xC244, "BT_clear_pair", "2" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 5: JVC PROJECTORS CODE SET A
    // -----------------------------------------------------------
    else if (idx == 5) {
        active_profile_workspace.profile_name = "JVC Projector A";
        active_profile_workspace.protocol = PROTO_JVC;
        active_profile_workspace.device_address = 0xCE;
        active_profile_workspace.cmd_clear_token_arm = 0x56;
        active_profile_workspace.cmd_clear_token_fire = 0xF4;

        // Syntax: add_cmd( Field 1: Hex, Field 2: Visible Xgimi Token, Field 3: Hidden Comment );
        add_cmd( 0xA0, "power_on",      "power on" );
        add_cmd( 0x60, "power_off",     "power off" );
        add_cmd( 0x80, "cursor_up",     "up arrow" );
        add_cmd( 0x40, "cursor_down",   "down arrow" );
        add_cmd( 0x6C, "cursor_left",   "left arrow" );
        add_cmd( 0x2C, "cursor_right",  "right arrow" );
        add_cmd( 0xF4, "cursor_enter",  "enter/ok" );
        add_cmd( 0x74, "settings_menu", "menu" );
        add_cmd( 0xC0, "back",          "exit" );
        add_cmd( 0xB8, "home",          "hide" );
        add_cmd( 0xD6, "game_menu",     "dynamic" );
        add_cmd( 0xCE, "game_menu",     "advanced menu" );
        add_cmd( 0x0E, "input",         "input HDMI 1" );
        add_cmd( 0x8E, "picture",       "input HDMI 2" );
        add_cmd( 0x2F, "picture",       "picture mode" );
        add_cmd( 0xCC, "focus_manual",  "focus -" );
        add_cmd( 0x8C, "focus_auto",    "focus +" );
        add_cmd( 0x11, "focus_manual",  "color profile" );
        add_cmd( 0xAF, "focus_auto",    "gamma settings" );
        add_cmd( 0x36, "shortcut_1",    "user 1" );
        add_cmd( 0xB6, "shortcut_2",    "user 2" );
        add_cmd( 0x76, "shortcut_3",    "user 3" );
        add_cmd( 0xEE, "shortcut_4",    "aspect" );
        add_cmd( 0x1B, "shortcut_1",    "mode 1" );
        add_cmd( 0x9B, "shortcut_2",    "mode 2" );
        add_cmd( 0x5B, "shortcut_3",    "mode 3" );
        add_cmd( 0x2E, "shortcut_4",    "info" );
        add_cmd( 0x5E, "volume_up",     "brightness up" );
        add_cmd( 0xDE, "volume_down",   "brightness down" );
        add_cmd( 0x6E, "mute",          "color temp" );
        add_cmd( 0x04, "volume_up",     "lens AP" );
        add_cmd( 0x0C, "volume_down",   "lens control_" );
        add_cmd( 0xA3, "mute",          "anamorphic" );
        add_cmd( 0x16, "token_sniff",   "cinema" );
        add_cmd( 0x96, "token_sniff",   "cinema" );
        add_cmd( 0x56, "token_clear",   "natural" );
        add_cmd( 0xAE, "token_recall",  "gamma" );
        add_cmd( 0xB7, "token_recall",  "HDR" );
        add_cmd( 0xFE, "BT_start_pair", "sharp down" );
        add_cmd( 0x9A, "BT_clear_pair", "sharp up" );
        add_cmd( 0x51, "BT_start_pair", "CMD" );
        add_cmd( 0x0F, "BT_clear_pair", "mpc" );
        add_cmd( 0x3E, "home",          "color up" );
        add_cmd( 0xBE, "home",          "color down" );
        add_cmd( 0x1E, "home",          "contast up" );
        add_cmd( 0x9E, "home",          "contrast down" );
        add_cmd( 0x36, "home",          "test" );
        add_cmd( 0xAC, "home",          "zoom T" );
        add_cmd( 0xEC, "home",          "zoom W" );
        add_cmd( 0x6B, "home",          "3D format" );
        add_cmd( 0x4E, "home",          "pic adjust" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 6: JVC PROJECTORS CODE SET B
    // -----------------------------------------------------------
    else if (idx == 6) {
        active_profile_workspace.profile_name = "JVC Projector B";
        active_profile_workspace.protocol = PROTO_JVC;
        active_profile_workspace.device_address = 0x36;
        active_profile_workspace.cmd_clear_token_arm = 0x56;
        active_profile_workspace.cmd_clear_token_fire = 0xF4;

        add_cmd( 0xA0, "power_on",      "power on" );
        add_cmd( 0x60, "power_off",     "power off" );
        add_cmd( 0x80, "cursor_up",     "up arrow" );
        add_cmd( 0x40, "cursor_down",   "down arrow" );
        add_cmd( 0x6C, "cursor_left",   "left arrow" );
        add_cmd( 0x2C, "cursor_right",  "right arrow" );
        add_cmd( 0xF4, "cursor_enter",  "enter" );
        add_cmd( 0x74, "settings_menu", "menu" );
        add_cmd( 0xC0, "back",          "exit" );
        add_cmd( 0xB8, "home",          "hide" );
        add_cmd( 0xD6, "game_menu",     "dynamic" );
        add_cmd( 0xCE, "game_menu",     "advanced menu_" );
        add_cmd( 0x0E, "input",         "input HDMI 1" );
        add_cmd( 0x8E, "picture",       "input HDMI 2" );
        add_cmd( 0x2F, "picture",       "picture mode_" );
        add_cmd( 0xCC, "focus_manual",  "focus -" );
        add_cmd( 0x8C, "focus_auto",    "focus +" );
        add_cmd( 0x11, "focus_manual",  "color profile" );
        add_cmd( 0xAF, "focus_auto",    "gamma settings" );
        add_cmd( 0x36, "shortcut_1",    "user 1" );
        add_cmd( 0xB6, "shortcut_2",    "user 2" );
        add_cmd( 0x76, "shortcut_3",    "user 3" );
        add_cmd( 0xEE, "shortcut_4",    "aspect" );
        add_cmd( 0x1B, "shortcut_1",    "mode 1_" );
        add_cmd( 0x9B, "shortcut_2",    "mode 2_" );
        add_cmd( 0x5B, "shortcut_3",    "mode 3_" );
        add_cmd( 0x2E, "shortcut_4",    "info" );
        add_cmd( 0x5E, "volume_up",     "brightness up" );
        add_cmd( 0xDE, "volume_down",   "brightness down" );
        add_cmd( 0x6E, "mute",          "color temp" );
        add_cmd( 0x04, "volume_up",     "lens AP_" );
        add_cmd( 0x0C, "volume_down",   "lens control_" );
        add_cmd( 0xA3, "mute",          "anamorphic_" );
        add_cmd( 0x16, "token_sniff",   "cinema" );
        add_cmd( 0x96, "token_sniff",   "cinema" );
        add_cmd( 0x56, "token_clear",   "natural" );
        add_cmd( 0xAE, "token_recall",  "gamma" );
        add_cmd( 0xB7, "token_recall",  "HDR" );
        add_cmd( 0xFE, "BT_start_pair", "sharp down" );
        add_cmd( 0x9A, "BT_clear_pair", "sharp up" );
        add_cmd( 0x51, "BT_start_pair", "CMD" );
        add_cmd( 0x0F, "BT_clear_pair", "mpc" );
        add_cmd( 0x3E, "home",          "color up" );
        add_cmd( 0xBE, "home",          "color down" );
        add_cmd( 0x1E, "home",          "contast up" );
        add_cmd( 0x9E, "home",          "contrast down" );
        add_cmd( 0x36, "home",          "test" );
        add_cmd( 0xAC, "home",          "zoom T" );
        add_cmd( 0xEC, "home",          "zoom W" );
        add_cmd( 0x6B, "home",          "3D format" );
        add_cmd( 0x4E, "home",          "pic adjust" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 7: LG CINEBEAM HU810P
    // -----------------------------------------------------------
    else if (idx == 7) {
        active_profile_workspace.profile_name = "LG Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0xFB04;
        active_profile_workspace.cmd_clear_token_arm = 0xEA15;
        active_profile_workspace.cmd_clear_token_fire = 0xBB44;

        // Syntax: add_cmd( Field 1: Hex, Field 2: Visible Xgimi Token, Field 3: Hidden Comment );
        add_cmd( 0xF708, "power_on",      "power toggle" );
        add_cmd( 0x23DC, "power_on",      "Discrete Power On" );
        add_cmd( 0x2CC3, "power_off",     "Discrete Power Off" );
        add_cmd( 0xEF10, "power_off",     "0" );
        add_cmd( 0xD728, "back",          "Return" );
        add_cmd( 0xF807, "cursor_left",   "cursor left" );
        add_cmd( 0xF906, "cursor_right",  "cursor right" );
        add_cmd( 0xBF40, "cursor_up",     "cursor up" );
        add_cmd( 0xBE41, "cursor_down",   "cursor down" );
        add_cmd( 0xBB44, "cursor_enter",  "select" );
        add_cmd( 0xBC43, "settings_menu", "menu" );
        add_cmd( 0x837C, "home",          "home" );
        add_cmd( 0xF40B, "input",         "input toggle" );
        add_cmd( 0xFE01, "game_menu",     "channel down" );
        add_cmd( 0xB24D, "picture",       "picture mode" );
        add_cmd( 0x8679, "focus_manual",  "aspect ratio" );
        add_cmd( 0x4FB0, "focus_auto",    "play" );
        add_cmd( 0x8d72, "shortcut_1",    "red" );
        add_cmd( 0x8e71, "shortcut_2",    "green" );
        add_cmd( 0x9C63, "shortcut_3",    "yellow" );
        add_cmd( 0x9E61, "shortcut_4",    "blue" );
        add_cmd( 0xFD02, "volume_up",     "volume up" );
        add_cmd( 0xFC03, "volume_down",   "volume down" );
        add_cmd( 0xF609, "mute",          "mute" );
        add_cmd( 0xEB14, "token_sniff",   "4" );
        add_cmd( 0xEA15, "token_clear",   "5" );
        add_cmd( 0xE916, "token_recall",  "6" );
        add_cmd( 0xA956, "BT_start_pair", "Netflix" );
        add_cmd( 0xA35C, "BT_clear_pair", "Prime video" );
        add_cmd( 0xEE11, "home",          "1" );
        add_cmd( 0xED12, "home",          "2" );
        add_cmd( 0xEC13, "home",          "3" );
        add_cmd( 0xE817, "home",          "7" );
        add_cmd( 0xE718, "home",          "8" );
        add_cmd( 0xE619, "home",          "9" );
        add_cmd( 0x54AB, "home",          "ch_list" );
        add_cmd( 0xB34C, "home",          "-" );
        add_cmd( 0xE51A, "home",          "pre-ch" );
        add_cmd( 0xF10E, "home",          "sleep" );
        add_cmd( 0xF30C, "home",          "portal" );
        add_cmd( 0xC639, "home",          "cc" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 8: OPTOMA UHD50X
    // -----------------------------------------------------------
    else if (idx == 8) {
        active_profile_workspace.profile_name = "Optoma Projector";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0xCD32;
        active_profile_workspace.cmd_clear_token_arm = 0x9A65;
        active_profile_workspace.cmd_clear_token_fire = 0xF00F;

        add_cmd( 0xFD02, "power_on",      "power on" );
        add_cmd( 0xD12E, "power_off",     "power off" );
        add_cmd( 0xEF10, "cursor_left",   "left arrow" );
        add_cmd( 0xEC12, "cursor_right",  "right arrow" );
        add_cmd( 0xEE11, "cursor_up",     "up arrow" );
        add_cmd( 0xEB14, "cursor_down",   "down arrow" );
        add_cmd( 0xF00F, "cursor_enter",  "ok" );
        add_cmd( 0xF10E, "settings_menu", "menu" );
        add_cmd( 0x9C63, "back",          "sleep" );
        add_cmd( 0xE916, "input",         "input HDMI 1" );
        add_cmd( 0xCF30, "game_menu",     "input HDMI 2" );
        add_cmd( 0xFA05, "picture",       "mode" );
        add_cmd( 0x9B64, "focus_manual",  "aspect" );
        add_cmd( 0xBB44, "focus_auto",    "DB" );
        add_cmd( 0xE41B, "shortcut_1",    "input VGA 1" );
        add_cmd( 0xE11E, "shortcut_2",    "input VGA 2" );
        add_cmd( 0xE31C, "shortcut_3",    "input video" );
        add_cmd( 0xE817, "shortcut_4",    "input YPbPr" );
        add_cmd( 0x7689, "volume_up",     "3D" );
        add_cmd( 0xF807, "volume_down",   "keystone" );
        add_cmd( 0xAD52, "mute",          "mute" );
        add_cmd( 0xC936, "token_sniff",   "user 1" );
        add_cmd( 0x9A65, "token_clear",   "user 2" );
        add_cmd( 0x9966, "token_recall",  "user 3" );
        add_cmd( 0xBE41, "BT_start_pair", "brightness" );
        add_cmd( 0xBD42, "BT_clear_pair", "contrast" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 9: SONY VPL-XW600ES
    // -----------------------------------------------------------
    else if (idx == 9) {
        active_profile_workspace.profile_name = "Sony Projector";
        active_profile_workspace.protocol = PROTO_SONY;
        active_profile_workspace.device_address = 0x0000;
        active_profile_workspace.cmd_clear_token_arm = 0x8AB54;
        active_profile_workspace.cmd_clear_token_fire = 0x02D2A;

        add_cmd( 0x03A2A, "power_on",      "Power On" );
        add_cmd( 0x07A2A, "power_off",     "Power Off" );
        add_cmd( 0x0542A, "power_on",      "Power Toggle" );
        add_cmd( 0x0562A, "cursor_up",     "up arrow" );
        add_cmd( 0x0362A, "cursor_down",   "down arrow" );
        add_cmd( 0x0162A, "cursor_left",   "left arrow" );
        add_cmd( 0x0662A, "cursor_right",  "right arrow" );
        add_cmd( 0x02D2A, "cursor_enter",  "OK / Enter" );
        add_cmd( 0x04A2A, "settings_menu", "Menu" );
        add_cmd( 0x06F2A, "home",          "Reset" );
        add_cmd( 0x18BE4, "back",          "Position" );
        add_cmd( 0x6AB54, "game_menu",     "Game" );
        add_cmd( 0xEAB54, "picture",       "Photo" );
        add_cmd( 0x0752A, "input",         "Input" );
        add_cmd( 0x26B54, "focus_manual",  "Focus" );
        add_cmd( 0x46B54, "focus_auto",    "Zoom" );
        add_cmd( 0x76B54, "shortcut_1",    "aspect ratio" );
        add_cmd( 0x0502A, "shortcut_2",    "motion flow" );
        add_cmd( 0xDCB54, "shortcut_3",    "3D" );
        add_cmd( 0xD2B54, "shortcut_4",    "color Space" );
        add_cmd( 0x00C2A, "volume_up",     "contrast" );
        add_cmd( 0x04C2A, "volume_down",   "contrast down" );
        add_cmd( 0xFAB54, "mute",          "advanced iris" );
        add_cmd( 0x9AB54, "token_sniff",   "BRT Cinema" );
        add_cmd( 0x8AB54, "token_clear",   "BRT TV" );
        add_cmd( 0x2AB54, "token_recall",  "User" );
        add_cmd( 0x07C2A, "BT_start_pair", "Brightness down" );
        add_cmd( 0x03C2A, "BT_clear_pair", "brightness up" );
        add_cmd( 0x3AB54, "home",          "color temp" );
        add_cmd( 0x0702A, "home",          "contrast enhancer" );
        add_cmd( 0xCAB54, "home",          "film 1" );
        add_cmd( 0x1AB54, "home",          "film 2" );
        add_cmd( 0x7AB54, "home",          "gamma Corr" );
        add_cmd( 0x06A2A, "home",          "input HDMI 1" );
        add_cmd( 0x01A2A, "home",          "input HDMI 2" );
        add_cmd( 0x04BE4, "home",          "position 1.85" );
        add_cmd( 0x84BE4, "home",          "position 2.35" );
        add_cmd( 0xC4BE4, "home",          "position Custom 2" );
        add_cmd( 0x24BE4, "home",          "position Custom 3" );
        add_cmd( 0x32B54, "home",          "reality creation" );
        add_cmd( 0xAAB54, "home",          "REF" );
        add_cmd( 0x0622A, "home",          "sharpness down" );
        add_cmd( 0x0222A, "home",          "sharpness up" );
        add_cmd( 0xC6B54, "home",          "shift" );
        add_cmd( 0x4AB54, "home",          "TV" );
        add_cmd( 0x42BE4, "home",          "wide mode full" );
        add_cmd( 0xFCBE4, "home",          "wide mode full1" );
        add_cmd( 0x02BE4, "home",          "wide mode full2" );
        add_cmd( 0x82BE4, "home",          "wide mode normal" );
        add_cmd( 0x7CBE4, "home",          "wide mode WZoom" );
        add_cmd( 0xC2BE4, "home",          "wide mode zoom" );
        add_cmd( 0x22BE4, "home",          "wide mode anamorphic zoom" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 10: SONY XBR-77A9G TV
    // -----------------------------------------------------------
    else if (idx == 10) {
        active_profile_workspace.profile_name = "Sony XBR";
        active_profile_workspace.protocol = PROTO_SONY;
        active_profile_workspace.device_address = 0x0000;
        active_profile_workspace.cmd_clear_token_arm = 0x0210;
        active_profile_workspace.cmd_clear_token_fire = 0x0A70;

        // Syntax: add_cmd( Field 1: Hex, Field 2: Visible Xgimi Token, Field 3: Hidden Comment );
        add_cmd( 0x0750, "power_on",      "power on" );
        add_cmd( 0x0A90, "power_on",      "power toggle" );
        add_cmd( 0x0F50, "power_off",     "power off" );
        add_cmd( 0x0910, "power_off",     "0" );
        add_cmd( 0x02D0, "cursor_left",   "Arrow Left" );
        add_cmd( 0x0CD0, "cursor_right",  "Arrow Right" );
        add_cmd( 0x02F0, "cursor_up",     "Arrow Up" );
        add_cmd( 0x0AF0, "cursor_down",   "Arrow Down" );
        add_cmd( 0x0A70, "cursor_enter",  "Arrow Select" );
        add_cmd( 0x6923, "settings_menu", "Action Menu" );
        add_cmd( 0x62E9, "back",          "Back" );
        add_cmd( 0x0070, "home",          "Home" );
        add_cmd( 0x3123, "game_menu",     "Google Play" );
        add_cmd( 0x0A50, "input",         "Input" );
        add_cmd( 0x0250, "picture",       "TV" );
        add_cmd( 0x0AE9, "focus_auto",    "subtitle" );
        add_cmd( 0x0E90, "focus_manual",  "audio" );
        add_cmd( 0x72E9, "shortcut_1",    "yellow" );
        add_cmd( 0x12E9, "shortcut_2",    "blue" );
        add_cmd( 0x52E9, "shortcut_3",    "red" );
        add_cmd( 0x32E9, "shortcut_4",    "green" );
        add_cmd( 0x0490, "volume_up",     "volume up" );
        add_cmd( 0x0C90, "volume_down",   "volume down" );
        add_cmd( 0x0290, "mute",          "mute" );
        add_cmd( 0x0C10, "token_sniff",   "4" );
        add_cmd( 0x0210, "token_clear",   "5" );
        add_cmd( 0x0A10, "token_recall",  "6" );
        add_cmd( 0x2CE9, "BT_start_pair", "play" );
        add_cmd( 0x1CE9, "BT_clear_pair", "fast forward" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 11: TIVO ROAMIO TCD846500
    // -----------------------------------------------------------
    else if (idx == 11) {
        active_profile_workspace.profile_name = "TiVo Roamio";
        active_profile_workspace.protocol = PROTO_NEC;
        active_profile_workspace.device_address = 0x3085;
        active_profile_workspace.cmd_clear_token_arm = 0xD02C;
        active_profile_workspace.cmd_clear_token_fire = 0xE019;

        add_cmd( 0xE010, "power_on",      "TV power)" );
        add_cmd( 0xE011, "power_off",     "live TV" );
        add_cmd( 0xC031, "power_off",     "0" );
        add_cmd( 0xE014, "cursor_up",     "arrow up" );
        add_cmd( 0xE016, "cursor_down",   "arrow down" );
        add_cmd( 0xE017, "cursor_left",   "arrow left" );
        add_cmd( 0xE015, "cursor_right",  "arrow right" );
        add_cmd( 0xE019, "cursor_enter",  "select" );
        add_cmd( 0xF00C, "settings_menu", "tivo" );
        add_cmd( 0xF00D, "settings_menu", "tivo (myHarmony version)" );
        add_cmd( 0xB044, "back",          "zoom" );
        add_cmd( 0xE01E, "home",          "channel up" );
        add_cmd( 0xC036, "game_menu",     "guide" );
        add_cmd( 0xC034, "input",         "input" );
        add_cmd( 0xE013, "picture",       "into" );
        add_cmd( 0xD02E, "focus_auto",    "7" );
        add_cmd( 0xD02F, "focus_manual",  "8" );
        add_cmd( 0x9060, "shortcut_1",    "A yellow" );
        add_cmd( 0x9061, "shortcut_2",    "B blue" );
        add_cmd( 0x9062, "shortcut_3",    "C red" );
        add_cmd( 0x9063, "shortcut_4",    "D green" );
        add_cmd( 0xE01C, "volume_up",     "volume up" );
        add_cmd( 0xE01D, "volume_down",   "volume down" );
        add_cmd( 0xE01B, "mute",          "mute" );
        add_cmd( 0xD02B, "token_sniff",   "4" );
        add_cmd( 0xD02C, "token_clear",   "5" );
        add_cmd( 0xD02D, "token_recall",  "6" );
        add_cmd( 0xC033, "BT_start_pair", "enter" );
        add_cmd( 0xC032, "BT_clear_pair", "clear" );
    }
    // -----------------------------------------------------------
    // FACTORY LAYOUT INDEX 12: PANASONIC PROJECTOR
    // -----------------------------------------------------------
    else if (idx == 12) {
        active_profile_workspace.profile_name = "Panasonic Projector";
        active_profile_workspace.protocol = PROTO_PANASONIC;
        active_profile_workspace.device_address = 0x4004;
        active_profile_workspace.cmd_clear_token_arm = 0x1002829;
        active_profile_workspace.cmd_clear_token_fire = 0x1009293;

        add_cmd( 0x1003A3B, "power_on",          "on" );
        add_cmd( 0x100BCBD, "power_off",         "off" );
        add_cmd( 0x1007273, "cursor_left",       "left arrow" );
        add_cmd( 0x100F2F3, "cursor_right",      "right arrow" );
        add_cmd( 0x1005253, "cursor_up",         "up arrow" );
        add_cmd( 0x100D2D3, "cursor_down",       "down arrow" );
        add_cmd( 0x1009293, "cursor_enter",      "OK" );
        add_cmd( 0x1004A4B, "settings_menu",     "menu" );
        add_cmd( 0x1002B2A, "back",              "return" );
        add_cmd( 0x1009C9D, "home",              "info" );
        add_cmd( 0x10090F1, "game_menu",         "apps" );
        add_cmd( 0x100A0A1, "input",             "AV input" );
        add_cmd( 0x1000B0A, "picture",           "picture mode" );
        add_cmd( 0x100C2C3, "focus_manual",      "focus +" );
        add_cmd( 0x100E2E3, "focus_auto",        "focus -" );
        add_cmd( 0x1000E0F, "shortcut_1",        "red" );
        add_cmd( 0x1008E8F, "shortcut_2",        "green" );
        add_cmd( 0x1004E4F, "shortcut_3",        "yellow" );
        add_cmd( 0x100CECF, "shortcut_4",        "blue" );
        add_cmd( 0x1000405, "home",              "volume +" );
        add_cmd( 0x1008485, "home",              "volume -" );
        add_cmd( 0x1004C4D, "home",              "mute" );
        add_cmd( 0x100A8A9, "token_sniff",       "4" );
        add_cmd( 0x1002829, "token_clear",       "5" );
        add_cmd( 0x100C8C9, "token_recall",      "6" );
        add_cmd( 0x1008889, "BT_start_pair",     "2" );
        add_cmd( 0x1004849, "BT_clear_pair",     "3" );
        add_cmd( 0x1009899, "home",              "0" );
        add_cmd( 0x1000809, "home",              "1" );
        add_cmd( 0x1006869, "home",              "7" );
        add_cmd( 0x100E8E9, "home",              "8" );
        add_cmd( 0x1001819, "home",              "9" );
        add_cmd( 0x1002223, "home",              "HDMI 1" );
        add_cmd( 0x100A2A3, "home",              "HDMI 2" );
        add_cmd( 0x1006263, "home",              "computer" );
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
    
    if (pref_obj.load(&flash_p) && flash_p.struct_version == CURRENT_STRUCT_VERSION) {
        active_profile_workspace.profile_name = flash_p.profile_name;
        active_profile_workspace.protocol = flash_p.protocol;
        active_profile_workspace.device_address = flash_p.device_address;
        active_profile_workspace.cmd_clear_token_arm = flash_p.cmd_clear_token_arm;
        active_profile_workspace.cmd_clear_token_fire = flash_p.cmd_clear_token_fire;
        
        uint16_t load_limit = (flash_p.total_keys > 55) ? 55 : flash_p.total_keys;
        active_profile_workspace.cmd_codes.resize(load_limit);
        
        for (uint16_t k = 0; k < load_limit; k++) {
            auto& kv_pair = active_profile_workspace.cmd_codes[k];
            kv_pair.first = flash_p.keys[k].hex_code;
            std::strncpy(kv_pair.second.name, flash_p.keys[k].target_button_id, sizeof(kv_pair.second.name) - 1);
            std::strncpy(kv_pair.second.button_name, flash_p.keys[k].button_name, sizeof(kv_pair.second.button_name) - 1);
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
// 6. MINIFIED ZERO-ALLOCATION EXCEL TRANSIT EXPORT STREAM GENERATOR
// ====================================================================
inline std::string generate_minified_profile_json(int idx) {
  std::string json_out;
  json_out.reserve(5120); 
  char row_buf[512];

  // If the request points directly to the active layout, extract it straight out of workspace RAM
  if (idx == id(active_remote_layout)) {
      const auto& p = active_profile_workspace;
      snprintf(row_buf, sizeof(row_buf), "{\n  \"idx\":%d,\n  \"nam\":\"%s\",\n  \"pro\":\"%s\",\n  \"adr\":\"%X\",\n  \"car\":\"%X\",\n  \"cfr\":\"%X\",\n  \"key\":[\n",
               idx, p.profile_name.c_str(), to_string(p.protocol), (unsigned int)p.device_address, (unsigned int)p.cmd_clear_token_arm, (unsigned int)p.cmd_clear_token_fire);
      json_out += row_buf;
      
      for (size_t i = 0; i < p.cmd_codes.size(); i++) {
          bool is_last = (i == p.cmd_codes.size() - 1);
          snprintf(row_buf, sizeof(row_buf), "    {\"x\":\"%X\",\"b\":\"%s\",\"l\":\"%s\"}%s\n",
                   (unsigned int)p.cmd_codes[i].first, p.cmd_codes[i].second.name, p.cmd_codes[i].second.button_name, is_last ? "" : ",");
          json_out += row_buf;
      }
      json_out += "  ]\n}";
      return json_out;
  }

  // Otherwise, quickly step into the flash track, dump the details, and jump straight back
  load_profile_to_workspace(idx);
  const auto& p = active_profile_workspace;
  snprintf(row_buf, sizeof(row_buf), 
         "{\n  \"idx\":%d,\n  \"nam\":\"%s\",\n  \"pro\":\"%s\",\n  \"adr\":\"%X\",\n  \"car\":\"%X\",\n  \"cfr\":\"%X\",\n  \"key\":[\n",
         idx, p.profile_name.c_str(), to_string(p.protocol), (unsigned int)p.device_address, (unsigned int)p.cmd_clear_token_arm, (unsigned int)p.cmd_clear_token_fire);

  json_out += row_buf;
  
  for (size_t i = 0; i < p.cmd_codes.size(); i++) {
      bool is_last = (i == p.cmd_codes.size() - 1);
      snprintf(row_buf, sizeof(row_buf), "    {\"x\":\"%X\",\"b\":\"%s\",\"l\":\"%s\"}%s\n",
               (unsigned int)p.cmd_codes[i].first, p.cmd_codes[i].second.name, p.cmd_codes[i].second.button_name, is_last ? "" : ",");
      json_out += row_buf;
  }
  json_out += "  ]\n}";

  // Instantly rehydrate back to the active operating layout before function teardown finishes
  load_profile_to_workspace(id(active_remote_layout));
  return json_out;
}
