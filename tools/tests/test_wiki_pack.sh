#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# The TinyKiwix pack contract, both halves: the reader's record and ordering
# helpers (tools/tests/tb_wiki_pack.c) and the writer's output over a synthetic
# archive (tools/tests/check_wiki_convert.py).
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
   -I "$root/ports/bl616" \
   "$root/tools/tests/tb_wiki_pack.c" -o "$work/tb"
"$work/tb"

python3 "$root/tools/tests/check_wiki_convert.py"
