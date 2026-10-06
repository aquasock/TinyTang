Draft pull request for tinydesk-project/tinydesk-shell, addressing issue #2.
Prepared in `/run/media/vash/GIT/tinydesk-shell` on
`configurable-script-limits` from upstream `main` at `8456dd1`. Not pushed
or posted. The proposed source diff is the carried patch
`third_party/patches/tdsh/0001-configurable-script-limits.patch`; it changes
only the upstream header and README. Review this diff and text before publication.

# Shell: let ports set script memory limits

Ports with small heaps can now set `TDSH_MAX_VARS`, `TDSH_VAR_NAME_MAX`,
`TDSH_VAR_VALUE_MAX` and `TDSH_SCRIPT_TASK_STACK` from their build. The existing
values remain the defaults, and scripts still copy the session and its
variables. The header and README explain that every component including
`tdsh.h` must use the same overrides because the variable limits change the
session layout.

This addresses #2. TinyTang's measured desktop failure was a refused allocation
while loading a script after its session copy and worker stack had consumed
most of the available heap. Its 32-variable, 128-byte-value configuration saves
13,344 bytes per session on the BL616.

Validation on Linux x86_64, using the POSIX/pthread host port:

- At the pinned v0.1.5 commits and with the patch's defaults: TinyDesk Shell
  8/8 and TinyDesk 10/10 host tests pass without compiler warnings.
- With 48 variables and 128-byte value buffers: all 8 Shell and 10 desktop
  tests pass without compiler warnings.
- With TinyTang's 32 variables and 128-byte value buffers: 7 compatible Shell
  tests and all 10 desktop tests pass. The comprehensive uScript fixture
  retains more than 32 variables, so it is run in the 48-variable configuration.
- The native host suites retain the default 32 KB worker stack; Linux pthread
  runs with a 16 KB stack crash in the script tests. Separate worker probes
  check the exact 16 KB request, variable boundaries, inherited variables,
  script isolation and allocation cleanup, with both defaults and smaller limits.
- clang-format 16.0.6 passes for the changed header.

The reproducible validation is `tools/tests/test_script_limits.sh` in TinyTang.
On the Sipeed Tang Console 138K (BL616/FreeRTOS), the firmware built and
was installed over USB CDC. After a cold power cycle, desktop launches of
Castlevania and Phosphor scripts from Files and the Terminal passed repeated
manual tests. The target session is 5,752 bytes. The last script used 10,728
of its 16,384-byte stack, with no allocation refusals since boot. A subsequent
MP3 play was running at 44.1 kHz with zero underruns. The firmware build has
existing JTAG-programmer and C-standard-option warnings; the upstream host
builds above have no warnings.

The script-loading error message discussed in #2 is a separate change.
