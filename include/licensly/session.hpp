#pragma once

#include <licensly/client.hpp>
#include <licensly/types.hpp>
#include <atomic>
#include <thread>
#include <chrono>

namespace licensly {

/** RAII session: activate/bind this device on construct, heartbeat in background,
 *  end the client session on destroy (does not clear the durable device binding or revoke the license key). */
class LicenslySession {
public:
    LicenslySession(Client& client,
                    const std::string& license_key,
                    const std::string& device_id);
    ~LicenslySession();

    LicenslySession(const LicenslySession&) = delete;
    LicenslySession& operator=(const LicenslySession&) = delete;

    const Lease& lease() const noexcept { return lease_; }
    const std::string& session_token() const noexcept { return session_token_; }

private:
    void run();

    Client& client_;
    std::string session_token_;
    Lease lease_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

}  // namespace licensly
