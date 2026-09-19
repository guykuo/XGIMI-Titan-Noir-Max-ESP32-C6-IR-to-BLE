#pragma once
#include <vector>
#include <cstdint>

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
