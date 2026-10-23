# Carried patches

Changes this project makes to third-party sources it does not own live here as
git-format patch files, not as commits in the third-party checkout itself. The
checkout stays pristine and in sync with upstream, and every local change is
one reviewable file.

- `tinydesk/` — TinyDesk, applied by `scripts/apply-tinydesk-patches.sh`
- `tdsh/` — TinyDesk Shell, applied by `scripts/apply-tdsh-patches.sh`

Each script is idempotent and runs before a build, since a `git submodule
update` or a fresh clone reverts the checkout. Every patch is recorded in
`THIRD_PARTY.md` beside the other third-party obligations.

The nestang series (`0001`–`0007`) and the menu-core patch that built
nand2mario's NES core with the desktop layer and the keyboard link, and an
emulator-free menu core from it, were retired in core-log entry 25: TinyNES
replaced the NES core and the desktop core replaced the menu core. Git history
keeps them.

## Conventions

- One patch per coherent change, `NNNN-short-slug.patch`, numbered in the order
  they apply.
- Mark patched regions in the source with a `TinyTang:` comment, so a reader
  landing in the file sees that it is not upstream's text.
