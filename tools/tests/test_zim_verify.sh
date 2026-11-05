#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The ZIM checksum verifier on the host (tools/tests/check_zim_verify.py).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
python3 "$root/tools/tests/check_zim_verify.py"
