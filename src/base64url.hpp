#pragma once
// Base64url (no padding) encode/decode helpers.

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace licensly {

inline std::vector<uint8_t> base64url_decode(const std::string& in) {
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Build reverse lookup that handles both standard (+/) and url-safe (-_)
    uint8_t rev[256];
    memset(rev, 0xFF, sizeof(rev));
    for (int i = 0; i < 64; ++i) rev[(uint8_t)table[i]] = (uint8_t)i;
    rev[(uint8_t)'-'] = 62;
    rev[(uint8_t)'_'] = 63;

    // Add padding
    std::string s = in;
    while (s.size() % 4 != 0) s += '=';

    std::vector<uint8_t> out;
    out.reserve(s.size() * 3 / 4);

    for (size_t i = 0; i < s.size(); i += 4) {
        uint8_t a = rev[(uint8_t)s[i]];
        uint8_t b = rev[(uint8_t)s[i+1]];
        uint8_t c = (s[i+2] == '=') ? 0 : rev[(uint8_t)s[i+2]];
        uint8_t d = (s[i+3] == '=') ? 0 : rev[(uint8_t)s[i+3]];

        if (a == 0xFF || b == 0xFF)
            throw std::runtime_error("Invalid base64url character");

        out.push_back((a << 2) | (b >> 4));
        if (s[i+2] != '=') out.push_back(((b & 0xF) << 4) | (c >> 2));
        if (s[i+3] != '=') out.push_back(((c & 0x3) << 6) | d);
    }
    return out;
}

} // namespace licensly
