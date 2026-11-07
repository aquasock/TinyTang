#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The card firmware tool's identity reads and refusals, on the host
# (tools/tests/check_card_firmware.py).  No board and no card are needed.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
python3 "$root/tools/tests/check_card_firmware.py"
