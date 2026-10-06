Draft pull request for tinydesk-project/tinydesk-shell, addressing issue #2.
Prepared in `/run/media/vash/GIT/tinydesk-shell` on
`configurable-script-limits` from upstream `main` at `8456dd1`. Not pushed
or posted. The proposed source diff is the carried patch
`third_party/patches/tdsh/0001-configurable-script-limits.patch`; it changes
only the upstream header and README. Review this diff and text before publication.

# Shell: let ports set script memory limits

Make `TDSH_MAX_VARS`, `TDSH_VAR_NAME_MAX`, `TDSH_VAR_VALUE_MAX` and
`TDSH_SCRIPT_TASK_STACK` overridable, preserving current defaults and each
script's copied session and variables. The header and README explain that
every component including `tdsh.h` must use consistent overrides because
the variable limits change the session layout.

Addresses #2. TinyTang's 48-variable, 128-byte-value configuration saves
10,768 bytes per session on the BL616. The README example uses this table
size so that the comprehensive host script fits and every host test runs.

Validation on Linux x86_64 with the POSIX/pthread host port: Shell 8/8 and
TinyDesk 10/10 tests pass without compiler warnings, with defaults and
with 48 variables and 128-byte value buffers. A separate 32-variable
configuration passes the seven compatible Shell tests and all ten desktop
tests; the comprehensive script retains more than 32 variables and is run
at 48. Boundary and worker probes check both table sizes, variable lengths,
inherited values, script isolation, cleanup and the exact 16 KB stack request.
The changed header passes clang-format 16.0.6.

The native host suites retain their default 32 KB worker stack: Linux pthread
runs with 16 KB crash in the script tests. TinyTang uses a 16 KB worker stack.
On the Sipeed Tang Console 138K (BL616/FreeRTOS), the 48-variable build
passed repeated Castlevania and Phosphor script launches from the desktop,
including switching cores while music played with the Phosphor window open.
The target session is 8,328 bytes. The last script used 10,728 of its 16,384
stack bytes, with no allocation refusals or crash record after the tests.
Music subsequently played at 44.1 kHz with zero underruns. TinyTang also
fixed its own Terminal task restart and sized its music catalog to the
actual folder; those port changes are outside this upstream diff. The firmware
build retains its existing C-standard-option warning for C++.

Reproduce the checks with `tools/tests/test_script_limits.sh` in TinyTang.
The separate script-loading allocation-error message discussed in #2 remains
pending.
