#include <licensly/client.hpp>

#include <sodium.h>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cstring>

#include "base64url.hpp"
#include "json_simple.hpp"

namespace licensly {

namespace {

std::vector<uint8_t> hex_decode(const std::string& hex) {
    if (hex.size() % 2 != 0)
        throw std::runtime_error("Invalid hex string length");
    std::vector<uint8_t> out(hex.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        unsigned int byte;
        sscanf(hex.c_str() + 2 * i, "%2x", &byte);
        out[i] = static_cast<uint8_t>(byte);
    }
    return out;
}

} // anonymous namespace

Lease verify_envelope(const Envelope& env, const std::string& public_key_hex) {
    if (sodium_init() < 0)
        throw std::runtime_error("Failed to initialize libsodium");

    // 1. Decode public key
    auto pub_bytes = hex_decode(public_key_hex);
    if (pub_bytes.size() != crypto_sign_ed25519_PUBLICKEYBYTES)
        throw std::runtime_error("Invalid public key length");

    // 2. Build canonical signing input: v{version}\n{kid}\n{nonce}\n{issued_at}\n{payload}
    std::string signing_input =
        "v" + std::to_string(env.version) + "\n" +
        env.kid       + "\n" +
        env.nonce     + "\n" +
        env.issued_at + "\n" +
        env.payload;

    // 3. Decode base64url signature
    auto sig_bytes = base64url_decode(env.signature);
    if (sig_bytes.size() != crypto_sign_ed25519_BYTES)
        throw SignatureVerificationError("Signature has wrong length");

    // 4. Verify
    int rc = crypto_sign_ed25519_verify_detached(
        sig_bytes.data(),
        reinterpret_cast<const unsigned char*>(signing_input.data()),
        signing_input.size(),
        pub_bytes.data()
    );
    if (rc != 0)
        throw SignatureVerificationError("Ed25519 signature verification failed");

    // 5. Decode payload and parse lease
    auto payload_bytes = base64url_decode(env.payload);
    std::string payload_json(payload_bytes.begin(), payload_bytes.end());

    auto fields = simple_json_parse(payload_json);

    Lease lease;
    lease.product_id               = fields.at("product_id");
    lease.license_id               = fields.at("license_id");
    lease.license_status           = fields.at("license_status");
    lease.session_id               = fields.at("session_id");
    lease.device_id_hash           = fields.at("device_id_hash");
    lease.expires_at               = fields.at("expires_at");
    lease.heartbeat_interval_seconds = std::stoi(fields.at("heartbeat_interval_seconds"));
    lease.offline_grace_seconds    = std::stoi(fields.at("offline_grace_seconds"));
    if (auto it = fields.find("min_app_version"); it != fields.end())
        lease.min_app_version = it->second;
    return lease;
}

} // namespace licensly
