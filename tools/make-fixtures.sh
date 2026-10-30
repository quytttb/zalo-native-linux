#!/usr/bin/env bash
# Regenerate the checked-in ASAR fixture. Requires Node.js and npx.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fixtures="$root/tests/fixtures"

rm -rf "$fixtures/sample.asar" "$fixtures/sample.asar.unpacked"
npx --yes @electron/asar pack "$fixtures/sample-src" "$fixtures/sample.asar" \
    --unpack "*.node"

printf 'wrote %s\n' "$fixtures/sample.asar"
