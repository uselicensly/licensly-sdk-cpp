#include <licensly/client.hpp>

#include <curl/curl.h>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <sstream>

#include "json_simple.hpp"

namespace licensly {

namespace {

std::string generate_nonce() {
    unsigned char buf[16];
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) throw std::runtime_error("Cannot open /dev/urandom");
    if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
        fclose(f);
        throw std::runtime_error("Failed to read random bytes");
    }
    fclose(f);
    char hex[33];
    for (int i = 0; i < 16; ++i)
        snprintf(hex + 2 * i, 3, "%02x", buf[i]);
    return std::string(hex, 32);
}

std::string json_quote(const std::string& value) {
    std::string out{"\""};
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char escaped[7];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
                    out += escaped;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
    return out;
}

struct WriteBuffer {
    std::string data;
    static size_t callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        auto* buf = static_cast<WriteBuffer*>(userdata);
        buf->data.append(ptr, size * nmemb);
        return size * nmemb;
    }
};

Envelope envelope_from_map(const std::map<std::string, std::string>& m) {
    Envelope env;
    env.version = std::stoi(m.at("version"));
    env.kid = m.at("kid");
    env.nonce = m.at("nonce");
    env.issued_at = m.at("issued_at");
    env.payload = m.at("payload");
    env.signature = m.at("signature");
    return env;
}

Lease lease_from_signed_body(const std::string& resp_body, const std::string& public_key_hex) {
    auto top = simple_json_parse(resp_body);
    auto it = top.find("envelope");
    Envelope env;
    if (it != top.end()) {
        env = envelope_from_map(simple_json_parse(it->second));
    } else {
        env = envelope_from_map(top);
    }
    return verify_envelope(env, public_key_hex);
}

Activation activation_from_signed_body(const std::string& resp_body, const std::string& public_key_hex) {
    auto top = simple_json_parse(resp_body);
    auto token = top.find("session_token");
    if (token == top.end() || token->second.empty()) {
        throw std::runtime_error("signed response missing session_token");
    }
    return Activation{token->second, lease_from_signed_body(resp_body, public_key_hex)};
}

bool bool_from_json(const std::string& value, const char* field) {
    if (value == "true") return true;
    if (value == "false") return false;
    throw std::runtime_error(std::string("invalid boolean field: ") + field);
}

ValidationResult validation_from_body(const std::string& resp_body) {
    auto fields = simple_json_parse(resp_body);
    ValidationResult result;
    result.valid = bool_from_json(fields.at("valid"), "valid");
    result.product_id = fields.at("product_id");
    result.license_id = fields.at("license_id");
    result.license_status = fields.at("license_status");
    result.device_id_hash = fields.at("device_id_hash");
    result.min_app_version = fields.at("min_app_version");
    result.offline_usable = bool_from_json(fields.at("offline_usable"), "offline_usable");
    return result;
}

}  // namespace

struct Client::Impl {
    std::string base_url;
    std::string product_id;
    std::string public_key_hex;
    long timeout_ms;
    CURL* curl{nullptr};

    Impl(std::string base_url, std::string product_id, std::string public_key_hex, long timeout_ms)
        : base_url(std::move(base_url))
        , product_id(std::move(product_id))
        , public_key_hex(std::move(public_key_hex))
        , timeout_ms(timeout_ms) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize libcurl");
    }

    ~Impl() {
        if (curl) curl_easy_cleanup(curl);
        curl_global_cleanup();
    }

    std::string url(const std::string& path) const {
        return base_url + "/api/v1/" + path;
    }

    std::string post(const std::string& path, const std::string& body) {
        WriteBuffer resp;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string full_url = url(path);
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBuffer::callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        CURLcode rc = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        if (rc != CURLE_OK) {
            throw std::runtime_error(std::string("HTTP error: ") + curl_easy_strerror(rc));
        }
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code < 200 || http_code >= 300) {
            std::string code = "internal";
            std::string message = resp.data;
            auto parsed = simple_json_parse(resp.data);
            auto it_err = parsed.find("error");
            if (it_err != parsed.end()) {
                auto err = simple_json_parse(it_err->second);
                if (auto c = err.find("code"); c != err.end()) code = c->second;
                if (auto m = err.find("message"); m != err.end()) message = m->second;
            }
            if (code == "app_version_too_old") {
                throw AppVersionTooOldError(code, static_cast<int>(http_code), message);
            }
            if (code == "plan_limit") {
                throw PlanLimitError(code, static_cast<int>(http_code), message);
            }
            throw ApiError(code, static_cast<int>(http_code), message);
        }
        return resp.data;
    }
};

Client::Client(std::string base_url, std::string product_id, std::string public_key_hex, long timeout_ms)
    : d_(new Impl(std::move(base_url), std::move(product_id), std::move(public_key_hex), timeout_ms)) {}

Client::~Client() { delete d_; }

Activation Client::activate(const std::string& license_key,
                            const std::string& device_id,
                            const std::string& nonce_in,
                            const std::string& app_version) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = "{\"license_key\":" + json_quote(license_key)
        + ",\"product_id\":" + json_quote(d_->product_id)
        + ",\"device_id\":" + json_quote(device_id);
    if (!app_version.empty()) {
        body += ",\"app_version\":" + json_quote(app_version);
    }
    body += ",\"nonce\":" + json_quote(n) + "}";
    std::string resp_body = d_->post("activate", body);
    return activation_from_signed_body(resp_body, d_->public_key_hex);
}

Lease Client::heartbeat(const std::string& session_token, const std::string& nonce_in) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = "{\"session_token\":" + json_quote(session_token)
        + ",\"product_id\":" + json_quote(d_->product_id)
        + ",\"nonce\":" + json_quote(n) + "}";
    return lease_from_signed_body(d_->post("heartbeat", body), d_->public_key_hex);
}

void Client::deactivate(const std::string& session_token, const std::string& nonce_in) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = "{\"session_token\":" + json_quote(session_token)
        + ",\"product_id\":" + json_quote(d_->product_id)
        + ",\"nonce\":" + json_quote(n) + "}";
    d_->post("deactivate", body);
}

ValidationResponse Client::validate(const std::string& license_key,
                                    const std::string& device_id,
                                    const std::string& nonce_in,
                                    const std::string& app_version,
                                    bool issue_session) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = "{\"license_key\":" + json_quote(license_key)
        + ",\"product_id\":" + json_quote(d_->product_id)
        + ",\"device_id\":" + json_quote(device_id);
    if (!app_version.empty()) {
        body += ",\"app_version\":" + json_quote(app_version);
    }
    body += ",\"issue_session\":";
    body += (issue_session ? "true" : "false");
    body += ",\"nonce\":" + json_quote(n) + "}";

    std::string resp_body = d_->post("validate", body);
    if (issue_session) {
        return activation_from_signed_body(resp_body, d_->public_key_hex);
    }
    return validation_from_body(resp_body);
}

}  // namespace licensly
