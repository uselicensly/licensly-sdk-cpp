# Licensly C++ SDK

Official C++17 client for the Licensly Client API. Activate licenses, renew signed leases with heartbeats, and verify Ed25519 envelopes before trusting lease data.

Depends on **libcurl** and **libsodium**.

## Build

```bash
./scripts/fetch-contract.sh
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

// On construct: binds the license to this device (if needed) and starts a client session + heartbeats.
// On destroy: ends this client session only. The device binding stays until an admin resets/releases it.
// The license key itself stays valid unless you revoke it in the dashboard/API.
licensly::LicenslySession session(client, "XXXX-XXXX-XXXX-XXXX", "stable-device-fingerprint");
auto lease = session.lease();
```

You supply the device identifier yourself — the SDK never collects hardware IDs.

Docs: https://licensly.dev/docs

## Contract pin

This repo commits only `contract.lock.json`. Golden fixtures are fetched into `.contract/fixtures/` by `./scripts/fetch-contract.sh`.

When the published API contract changes, bump `contract.lock.json` to the new `contract_version` and bundle SHA-256, then re-run fetch.
