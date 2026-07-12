# Licensly C++ SDK

Official C++17 client for the Licensly Client API. Activate licenses, renew signed leases with heartbeats, and verify Ed25519 envelopes before trusting lease data.

Depends on **libcurl** and **libsodium**.

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Quick start

```cpp
#include <licensly/licensly.hpp>

licensly::Client client(
    "https://your-licensly-host.example",
    "prd_…",
    "<product Ed25519 public key hex>");

auto validation = client.validate(
    "LIC-8F2A-19C4-7B61-D0E3-55AA-91B2-C8D4-0F76",
    "installation-8f27c1a4",
    "",
    "2.3.1");
const auto& result = std::get<licensly::ValidationResult>(validation);
// Sessionless validation is unsigned and online-only.

// On construct: binds the license to this device (if needed) and starts a client session + heartbeats.
// On destroy: ends this client session only. The device binding stays until an admin resets/releases it.
// The license key itself stays valid unless you revoke it in the dashboard/API.
licensly::LicenslySession session(
    client,
    "LIC-8F2A-19C4-7B61-D0E3-55AA-91B2-C8D4-0F76",
    "installation-8f27c1a4",
    "2.3.1");
auto lease = session.lease();
```

You supply an opaque, stable device identifier yourself — the SDK never collects
hardware IDs. Pass `true` as `validate`'s `issue_session` argument to receive an
`Activation`; its signed envelope is verified before the lease is returned.

Docs: https://licensly.dev/docs

## Contract pin

`contract.lock.json` records which Licensly API contract this SDK targets.
Signing golden vectors used by unit tests are vendored under `tests/fixtures/` and must
match the pinned contract version when the lock is bumped.
