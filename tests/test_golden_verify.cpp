/**
 * Golden fixture verify tests.
 *
 * Exercises verify_envelope against canonical vectors from fixtures/.
 * No network access required.
 */
#include <licensly/client.hpp>
#include <licensly/types.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Include our minimal JSON and base64url helpers
#include "../src/json_simple.hpp"
#include "../src/base64url.hpp"

#ifndef FIXTURES_DIR
#define FIXTURES_DIR "fixtures"
#endif

using namespace licensly;

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static Envelope envelope_from_map(const std::map<std::string, std::string>& env_map) {
    Envelope env;
    env.version   = std::stoi(env_map.at("version"));
    env.kid       = env_map.at("kid");
    env.nonce     = env_map.at("nonce");
    env.issued_at = env_map.at("issued_at");
    env.payload   = env_map.at("payload");
    env.signature = env_map.at("signature");
    return env;
}

static int failures = 0;

#define EXPECT_NO_THROW(expr, label) \
    try { expr; std::cout << "[PASS] " label "\n"; } \
    catch (const std::exception& e) { std::cerr << "[FAIL] " label ": " << e.what() << "\n"; ++failures; }

#define EXPECT_THROW(expr, ExcType, label) \
    try { expr; std::cerr << "[FAIL] " label ": expected exception not thrown\n"; ++failures; } \
    catch (const ExcType&) { std::cout << "[PASS] " label "\n"; } \
    catch (const std::exception& e) { std::cerr << "[FAIL] " label ": wrong exception: " << e.what() << "\n"; ++failures; }

int main() {
    std::string fixtures_dir = std::string(FIXTURES_DIR);

    // ── Test 1: valid envelope passes verification ────────────────────────────
    {
        std::string json = read_file(fixtures_dir + "/ed25519_valid.json");
        auto top = simple_json_parse(json);
        auto env_map = simple_json_parse(top.at("envelope"));
        std::string pub_key = top.at("public_key_hex");
        Envelope env = envelope_from_map(env_map);

        EXPECT_NO_THROW(
            [&](){
                Lease lease = verify_envelope(env, pub_key);
                if (lease.license_status != "active")
                    throw std::runtime_error("Unexpected license_status: " + lease.license_status);
                if (lease.product_id != "prd_golden")
                    throw std::runtime_error("Unexpected product_id: " + lease.product_id);
            }(),
            "valid envelope returns active lease"
        );
    }

    // ── Test 2: mutated payload fails verification ────────────────────────────
    {
        std::string json = read_file(fixtures_dir + "/ed25519_mutated_payload.json");
        auto top = simple_json_parse(json);
        auto env_map = simple_json_parse(top.at("envelope"));
        std::string pub_key = top.at("public_key_hex");
        Envelope env = envelope_from_map(env_map);

        EXPECT_THROW(
            verify_envelope(env, pub_key),
            SignatureVerificationError,
            "mutated payload raises SignatureVerificationError"
        );
    }

    // ── Test 3: bad signature fails verification ──────────────────────────────
    {
        std::string json = read_file(fixtures_dir + "/ed25519_bad_signature.json");
        auto top = simple_json_parse(json);
        auto env_map = simple_json_parse(top.at("envelope"));
        std::string pub_key = top.at("public_key_hex");
        Envelope env = envelope_from_map(env_map);

        EXPECT_THROW(
            verify_envelope(env, pub_key),
            SignatureVerificationError,
            "bad signature raises SignatureVerificationError"
        );
    }

    if (failures > 0) {
        std::cerr << failures << " test(s) FAILED\n";
        return 1;
    }
    std::cout << "All golden verify tests passed.\n";
    return 0;
}
