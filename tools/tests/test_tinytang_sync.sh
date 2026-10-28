#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The sync tool's parsers, on the host (tools/tests/check_tinytang_sync.py).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
python3 "$root/tools/tests/check_tinytang_sync.py"
