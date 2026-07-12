#pragma once
// Minimal flat JSON string-value parser sufficient for Licensly envelope fields.
// No arrays, no nesting beyond one level. Not a general-purpose JSON library.

#include <string>
#include <map>
#include <stdexcept>

namespace licensly {

inline std::string json_unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Returns a flat map of top-level string and number fields.
inline std::map<std::string, std::string> simple_json_parse(const std::string& json) {
    std::map<std::string, std::string> result;
    size_t i = 0;
    auto skip_ws = [&]() { while (i < json.size() && (json[i]==' '||json[i]=='\t'||json[i]=='\n'||json[i]=='\r')) ++i; };

    skip_ws();
    if (i >= json.size() || json[i] != '{')
        return result;
    ++i;

    while (i < json.size()) {
        skip_ws();
        if (json[i] == '}') break;
        if (json[i] == ',') { ++i; continue; }

        // Parse key
        if (json[i] != '"') { ++i; continue; }
        ++i;
        std::string key;
        while (i < json.size() && json[i] != '"') {
            if (json[i] == '\\' && i+1 < json.size()) { key += json[++i]; ++i; }
            else { key += json[i++]; }
        }
        ++i; // closing "

        skip_ws();
        if (i >= json.size() || json[i] != ':') continue;
        ++i;
        skip_ws();

        // Parse value
        std::string value;
        if (json[i] == '"') {
            ++i;
            std::string raw;
            while (i < json.size() && json[i] != '"') {
                if (json[i] == '\\' && i+1 < json.size()) { raw += '\\'; raw += json[++i]; ++i; }
                else { raw += json[i++]; }
            }
            ++i; // closing "
            value = json_unescape(raw);
        } else if (json[i] == '{') {
            // Nested object: capture raw for error sub-parsing
            int depth = 0;
            size_t start = i;
            while (i < json.size()) {
                if (json[i] == '{') ++depth;
                else if (json[i] == '}') { --depth; if (depth == 0) { ++i; break; } }
                ++i;
            }
            value = json.substr(start, i - start);
        } else {
            // number, bool, null
            size_t start = i;
            while (i < json.size() && json[i] != ',' && json[i] != '}' && json[i] != ' ' && json[i] != '\n') ++i;
            value = json.substr(start, i - start);
        }

        result[key] = value;
    }
    return result;
}

} // namespace licensly
