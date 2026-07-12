#!/usr/bin/env bash
# Fetch and verify the pinned Licensly contract bundle into .contract/
#
# Resolution order:
#   1. LICENSLY_CONTRACT_BUNDLE — path to an already-built .tar.gz
#   2. LICENSLY_CONTRACT_API_ROOT — pack from a local api/ tree that includes
#      scripts/pack-contract-bundle.py
#   3. Adjacent ../api with pack script (local monorepo-style checkout)
#   4. Published GitHub release asset contract-vVERSION on uselicensly/licensly
#      (set LICENSLY_CONTRACT_TOKEN or GH_TOKEN when the release requires auth)
#
# Requires: contract.lock.json in the consumer repo root.
set -euo pipefail

ROOT="$(pwd)"
if [[ ! -f "${ROOT}/contract.lock.json" ]]; then
  ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fi
LOCK="${ROOT}/contract.lock.json"
DEST="${ROOT}/.contract"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -f "$LOCK" ]]; then
  echo "missing $LOCK" >&2
  exit 1
fi

VERSION="$(python3 -c "import json; print(json.load(open('$LOCK'))['contract_version'])")"
EXPECT="$(python3 -c "import json; print(json.load(open('$LOCK'))['bundle_sha256'])")"
ARCHIVE_NAME="licensly-contract-v${VERSION}.tar.gz"
ARCHIVE=""

sha256_of() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  else
    sha256sum "$1" | awk '{print $1}'
  fi
}

pack_from_api() {
  local api_root="$1"
  local pack="$api_root/scripts/pack-contract-bundle.py"
  if [[ ! -f "$pack" ]]; then
    return 1
  fi
  python3 "$pack" >/dev/null
  local built="$api_root/dist/$ARCHIVE_NAME"
  if [[ ! -f "$built" ]]; then
    return 1
  fi
  cp "$built" "$TMP/$ARCHIVE_NAME"
  ARCHIVE="$TMP/$ARCHIVE_NAME"
  return 0
}

download_release_asset() {
  local token="$1"
  local tag="contract-v${VERSION}"
  local api_base="https://api.github.com/repos/uselicensly/licensly"
  local auth_args=()
  if [[ -n "$token" ]]; then
    auth_args=(-H "Authorization: Bearer ${token}")
  fi

  # Private repos often 404 on the browser download URL; resolve via the Releases API.
  local release_json asset_id
  release_json="$(curl -fsSL "${auth_args[@]}" -H "Accept: application/vnd.github+json" \
    "${api_base}/releases/tags/${tag}")"
  asset_id="$(python3 -c "import json,sys; assets=json.load(sys.stdin).get('assets') or [];
print(next((a['id'] for a in assets if a.get('name')==sys.argv[1]), ''))" "$ARCHIVE_NAME" <<<"$release_json")"
  if [[ -z "$asset_id" ]]; then
    echo "release ${tag} has no asset named ${ARCHIVE_NAME}" >&2
    return 1
  fi
  curl -fsSL "${auth_args[@]}" -H "Accept: application/octet-stream" -L \
    -o "$TMP/$ARCHIVE_NAME" \
    "${api_base}/releases/assets/${asset_id}"
  ARCHIVE="$TMP/$ARCHIVE_NAME"
}

if [[ -n "${LICENSLY_CONTRACT_BUNDLE:-}" ]]; then
  cp "${LICENSLY_CONTRACT_BUNDLE}" "$TMP/$ARCHIVE_NAME"
  ARCHIVE="$TMP/$ARCHIVE_NAME"
elif [[ -n "${LICENSLY_CONTRACT_API_ROOT:-}" ]]; then
  pack_from_api "$(cd "${LICENSLY_CONTRACT_API_ROOT}" && pwd)"
elif [[ -d "${ROOT}/../api" && -f "${ROOT}/../scripts/pack-contract-bundle.py" ]]; then
  pack_from_api "$(cd "${ROOT}/.." && pwd)"
elif [[ -d "${ROOT}/api" && -f "${ROOT}/scripts/pack-contract-bundle.py" ]]; then
  pack_from_api "$ROOT"
else
  TOKEN="${LICENSLY_CONTRACT_TOKEN:-${GH_TOKEN:-${GITHUB_TOKEN:-}}}"
  download_release_asset "$TOKEN"
fi

if [[ -z "$ARCHIVE" || ! -f "$ARCHIVE" ]]; then
  echo "could not resolve contract bundle for v${VERSION}" >&2
  exit 1
fi

GOT="$(sha256_of "$ARCHIVE")"
if [[ "$GOT" != "$EXPECT" ]]; then
  echo "contract bundle digest mismatch:" >&2
  echo "  expected $EXPECT" >&2
  echo "  got      $GOT" >&2
  echo "Update contract.lock.json to match the published contract release." >&2
  exit 1
fi

rm -rf "$DEST"
mkdir -p "$DEST"
tar -xzf "$ARCHIVE" -C "$TMP"
# Archive contains contract-vVERSION/…
STAGE="$TMP/contract-v${VERSION}"
if [[ ! -d "$STAGE" ]]; then
  echo "archive missing contract-v${VERSION}/" >&2
  exit 1
fi
cp -R "$STAGE/." "$DEST/"
echo "Contract v${VERSION} verified → ${DEST}"
