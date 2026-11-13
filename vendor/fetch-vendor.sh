#!/usr/bin/env bash
# Reproduce the read-only M1 reference source tree.
# Bluebook: katahiromz/mzimeja v1.0.0.3 — Public Domain Software (PDS).
# The extracted tree is intentionally NOT committed (see ../.gitignore).
# See LICENSE-NOTICE.txt for why this exact tag is required.
set -euo pipefail

TAG="v1.0.0.3"
VERSION="${TAG#v}"
TARBALL="mzimeja-${TAG}.tar.gz"
SHA256="3545548b47f92b5b3036de622e8bed715e48e155eda3f281cff39d931a354e92"
URL="https://codeload.github.com/katahiromz/mzimeja/tar.gz/refs/tags/${TAG}"

cd "$(dirname "$0")"

if [ ! -f "$TARBALL" ]; then
    echo "Downloading ${URL}"
    curl -fL --retry 3 --max-time 300 -o "$TARBALL" "$URL"
fi

echo "${SHA256}  ${TARBALL}" | sha256sum -c -

rm -rf mzimeja-ref
tar xzf "$TARBALL" 2>/dev/null || true   # tar utime warnings are harmless in containers
mv "mzimeja-${VERSION}" mzimeja-ref

# Guard: refuse anything that is not the unambiguous public-domain snapshot.
if ! grep -qi "public domain" mzimeja-ref/LICENSE.txt; then
    echo "ERROR: LICENSE.txt is not PDS - wrong tag or upstream relicensed." >&2
    exit 1
fi
if [ "$(wc -c < mzimeja-ref/LICENSE.txt)" -gt 4096 ]; then
    echo "ERROR: LICENSE.txt looks like GPLv3 full text, not the 375B PDS notice." >&2
    exit 1
fi

echo "OK: vendor/mzimeja-ref/ ($(find mzimeja-ref -type f | wc -l) files, PDS, ${TAG})"
