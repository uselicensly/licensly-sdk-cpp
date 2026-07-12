#include <licensly/session.hpp>
#include <licensly/client.hpp>

namespace licensly {

LicenslySession::LicenslySession(Client& client,
                                 const std::string& license_key,
                                 const std::string& device_id)
    : client_(client) {
    Activation act = client_.activate(license_key, device_id);
    session_token_ = act.session_token;
    lease_ = act.lease;
    stop_.store(false);
    thread_ = std::thread([this] { run(); });
}

LicenslySession::~LicenslySession() {
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
    try {
        client_.deactivate(session_token_);
    } catch (...) {
    }
}

void LicenslySession::run() {
    while (!stop_.load()) {
        int interval = lease_.heartbeat_interval_seconds;
        for (int slept = 0; slept < interval * 1000 && !stop_.load(); slept += 500) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        if (stop_.load()) break;
        try {
            lease_ = client_.heartbeat(session_token_);
        } catch (...) {
        }
    }
}

}  // namespace licensly
