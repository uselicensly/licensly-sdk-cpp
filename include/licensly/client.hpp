#pragma once

#include <licensly/types.hpp>
#include <string>
#include <stdexcept>

namespace licensly {

struct SignatureVerificationError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct ApiError : std::runtime_error {
    std::string code;
    int http_status{0};
    ApiError(std::string code, int http_status, const std::string& message)
        : std::runtime_error(message), code(std::move(code)), http_status(http_status) {}
};

struct Activation {
    std::string session_token;
    Lease lease;
};

Lease verify_envelope(const Envelope& env, const std::string& public_key_hex);

class Client {
public:
    Client(std::string base_url,
           std::string product_id,
           std::string public_key_hex,
           long timeout_ms = 10000);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Activation activate(const std::string& license_key,
                        const std::string& device_id,
                        const std::string& nonce = "");

    Lease heartbeat(const std::string& session_token,
                    const std::string& nonce = "");

    void deactivate(const std::string& session_token,
                    const std::string& nonce = "");

    Lease validate(const std::string& session_token,
                   const std::string& nonce = "");

private:
    struct Impl;
    Impl* d_;
};

}  // namespace licensly
