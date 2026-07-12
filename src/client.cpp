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
            throw ApiError(code, static_cast<int>(http_code), message);
        }
        return resp.data;
    }
};

Client::Client(std::string base_url, std::string product_id, std::string public_key_hex, long timeout_ms)
    : d_(new Impl(std::move(base_url), std::move(product_id), std::move(public_key_hex), timeout_ms)) {}

Client::~Client() { delete d_; }

Activation Client::activate(const std::string& license_key, const std::string& device_id, const std::string& nonce_in) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = std::string(R"({"license_key":")") + license_key + R"(","product_id":")" + d_->product_id
        + R"(","device_id":")" + device_id + R"(","nonce":")" + n + R"("})";
    std::string resp_body = d_->post("activate", body);
    auto top = simple_json_parse(resp_body);
    auto tok = top.find("session_token");
    if (tok == top.end() || tok->second.empty()) {
        throw std::runtime_error("activate response missing session_token");
    }
    return Activation{tok->second, lease_from_signed_body(resp_body, d_->public_key_hex)};
}

Lease Client::heartbeat(const std::string& session_token, const std::string& nonce_in) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = std::string(R"({"session_token":")") + session_token + R"(","product_id":")" + d_->product_id
        + R"(","nonce":")" + n + R"("})";
    return lease_from_signed_body(d_->post("heartbeat", body), d_->public_key_hex);
}

void Client::deactivate(const std::string& session_token, const std::string& nonce_in) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = std::string(R"({"session_token":")") + session_token + R"(","product_id":")" + d_->product_id
        + R"(","nonce":")" + n + R"("})";
    d_->post("deactivate", body);
}

Lease Client::validate(const std::string& session_token, const std::string& nonce_in) {
    std::string n = nonce_in.empty() ? generate_nonce() : nonce_in;
    std::string body = std::string(R"({"session_token":")") + session_token + R"(","product_id":")" + d_->product_id
        + R"(","nonce":")" + n + R"("})";
    return lease_from_signed_body(d_->post("validate", body), d_->public_key_hex);
}

}  // namespace licensly
