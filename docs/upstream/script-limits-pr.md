Published as [tinydesk-project/tinydesk-shell PR #4](https://github.com/tinydesk-project/tinydesk-shell/pull/4), addressing issue #2.

The user reviewed the exact diff and description, then authorized publication.
The fork branch is `aquasock:configurable-script-limits`, commit `b4f9404`
on upstream `main` at `8456dd1`, in `/run/media/vash/GIT/tinydesk-shell`.
It changes only `include/tdsh.h` and `README.md` and matches TinyTang's
carried patch byte-for-byte. The PR is open for upstream review.

# Shell: let ports set script memory limits

Ports can now set `TDSH_MAX_VARS`, `TDSH_VAR_NAME_MAX`, `TDSH_VAR_VALUE_MAX` and `TDSH_SCRIPT_TASK_STACK` from their build. Defaults remain 64 variables, 32-byte names, 256-byte values and a 32 KB worker stack. Each script still receives its own session and variable copy.

The header and README explain the memory cost and require the same overrides in every component using `tdsh.h`, including C++ consumers. The README includes a smaller-table build example.

Addresses #2. TinyTang uses 48 variables and 128-byte values, saving 10,768 bytes per BL616 session.

Validation:

- Linux x86_64: Shell 8/8 and TinyDesk 10/10 tests pass without compiler warnings with defaults and with 48 variables/128-byte values. Boundary, inheritance, isolation, cleanup and stack-request probes also pass.
- Linux suites retain the default 32 KB stack; 16 KB pthread runs fail in the script tests. On BL616/FreeRTOS, the 16 KB worker stack passed repeated desktop core launches and switches, with 5,656 bytes spare and no allocation refusals after testing.
- clang-format 16.0.6 passes.

TinyTang's Terminal restart and catalog-allocation fixes are separate port changes. The script-loading allocation-error message remains a separate follow-up.

Reproducible checks: https://github.com/aquasock/TinyTang/blob/4633b4d/tools/tests/test_script_limits.sh
