#pragma once

#include <string>
#include <cstdint>

namespace licensly {

/// Decoded lease payload from a verified Ed25519 envelope.
struct Lease {
    std::string product_id;
    std::string license_id;
    std::string license_status;
    std::string session_id;
    std::string device_id_hash;
    std::string expires_at;
    int32_t     heartbeat_interval_seconds{120};
    int32_t     offline_grace_seconds{3600};
    std::string min_app_version;
};

/// Unsigned, online-only result from validate with issue_session=false.
struct ValidationResult {
    bool        valid{false};
    std::string product_id;
    std::string license_id;
    std::string license_status;
    std::string device_id_hash;
    std::string min_app_version;
    bool        offline_usable{false};
};

/// Ed25519 signed envelope as returned by the server.
struct Envelope {
    int32_t     version{1};
    std::string kid;
    std::string nonce;
    std::string issued_at;
    std::string payload;    ///< base64url-encoded lease JSON (unverified until verify_envelope)
    std::string signature;  ///< base64url-encoded Ed25519 signature
};

} // namespace licensly
