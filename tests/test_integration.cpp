/**
 * Integration test — requires a running Licensly server.
 *
 * Skipped unless LICENSLY_URL, LICENSLY_PRODUCT_ID, LICENSLY_LICENSE_KEY,
 * LICENSLY_PUBLIC_KEY_HEX, and LICENSLY_DEVICE_ID are all set.
 *
 * Set LICENSLY_SKIP_INTEGRATION=1 (default in CI) to skip.
 */
#include <licensly/client.hpp>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace licensly;

static const char* env_or(const char* name, const char* fallback = "") {
    const char* v = std::getenv(name);
    return v ? v : fallback;
}

int main() {
    if (std::string(env_or("LICENSLY_SKIP_INTEGRATION", "0")) == "1") {
        std::cout << "[SKIP] Integration test skipped (LICENSLY_SKIP_INTEGRATION=1)\n";
        return 0;
    }

    const std::string base_url        = env_or("LICENSLY_URL");
    const std::string product_id      = env_or("LICENSLY_PRODUCT_ID");
    const std::string license_key     = env_or("LICENSLY_LICENSE_KEY");
    const std::string public_key_hex  = env_or("LICENSLY_PUBLIC_KEY_HEX");
    const std::string device_id       = env_or("LICENSLY_DEVICE_ID");
    const std::string app_version     = env_or("LICENSLY_APP_VERSION");

    if (base_url.empty() || product_id.empty() || license_key.empty()
        || public_key_hex.empty() || device_id.empty()) {
        std::cerr << "[SKIP] Integration test skipped: missing required env vars\n";
        return 0;
    }

    try {
        Client client(base_url, product_id, public_key_hex);
        ValidationResponse validation = client.validate(
            license_key, device_id, "", app_version);
        if (!std::holds_alternative<ValidationResult>(validation)) {
            throw std::runtime_error("sessionless validate returned a session");
        }
        const auto& result = std::get<ValidationResult>(validation);
        if (!result.valid || result.offline_usable) {
            throw std::runtime_error("unexpected sessionless validation result");
        }
        std::cout << "[PASS] validate: status=" << result.license_status << "\n";

        Activation act = client.activate(license_key, device_id, "", app_version);
        std::cout << "[PASS] activate: status=" << act.lease.license_status << "\n";

        Lease hb = client.heartbeat(act.session_token);
        std::cout << "[PASS] heartbeat: status=" << hb.license_status << "\n";

        client.deactivate(act.session_token);
        std::cout << "[PASS] deactivate\n";

        ValidationResponse with_session = client.validate(
            license_key, device_id, "", app_version, true);
        if (!std::holds_alternative<Activation>(with_session)) {
            throw std::runtime_error("validate with issue_session did not return a session");
        }
        client.deactivate(std::get<Activation>(with_session).session_token);
        std::cout << "[PASS] validate with session\n";
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
