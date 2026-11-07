Published as [tinydesk-project/tinydesk PR #18](https://github.com/tinydesk-project/tinydesk/pull/18).
The user reviewed the title and body, then authorized the branch push and posted
the PR themselves. The branch is `aquasock:start-menu-max-apps`, commit
`79806d5` on upstream `main` at `fb07d1a`, built and tested in a clone of the
fork. It changes `src/wm.c` and `tests/test_wm.c` and nothing else. The PR is
open for upstream review.

The patch itself is kept as `docs/upstream/menu-max-fix.patch`, the exact diff
that was applied and tested, so the change survives even if the fork branch is
ever deleted.

Everything below the rule is the PR as posted: title, then body.

The posted body is the markdown below. GitHub initially filled the field with
the commit message, which says the same things more compactly but omits the
file-and-line references and the verification section; the user replaced it
with this text before creating the PR.

---

**Title:** Start menu: size the menu from `TD_MAX_APPS`, so raising it does not eat "Exit"

## Body

`#define MENU_MAX 20` in `src/wm.c` is a fixed number, but `TD_MAX_APPS`
(`include/tinydesk/td_config.h:110`) is `#ifndef`-wrapped and meant to be
overridden per port. A port that raises `TD_MAX_APPS` therefore gets a menu
that drops apps, and because `start_menu_chosen()` offsets the fixed entries
by `s_app_count` rather than by the number of apps the menu placed, every
entry below the separator then acts on the wrong item.

The visible result is a desktop whose start menu cannot close it: "Exit" lands
one past the end of the callback's chain and does nothing, and `td_quit()` is
reachable only from there.

Reachable at the default configuration too: `apps/session.c` adds one start
item, leaving zero margin at `TD_MAX_APPS` 16, so a second start item is
enough.

### The change

`src/wm.c`, two edits, no API change:

- `MENU_MAX` is derived from the two limits instead of picked:
  `(TD_MAX_APPS + START_EXTRA_MAX + 3)` — every app, every extra start item,
  the separator, "Redraw screen" and "Exit". `START_EXTRA_MAX` is already
  defined at line 60, above this point.
- A new `s_menu_apps` records how many apps the open menu placed, and
  `start_menu_chosen()` offsets by that rather than by `s_app_count`. This is
  belt-and-braces once the size is right, but it keeps the callback honest if a
  menu is ever clipped for another reason, and it is the half that was actually
  mis-dispatching.

`tests/test_wm.c` gains `test_start_menu_tail()`, which needs no build
configuration: fill the registry at the default 16, add two start items, open
the menu, press END and ENTER, and assert `td_step()` returns false.

I deliberately did not add a `TD_MAX_APPS` override to `CMakeLists.txt` for the
suite. My port carries one, but the bug reproduces without it, so upstream does
not need to change what its tests are built with.

### Verification

- `ctest`: 13/13 pass, including `test_wm` at 60 checks.
- The new test **fails on the current tree** at `CHECK(!td_step())` — Exit
  redraws instead of quitting — and passes with the change. Verified both ways
  by reverting `src/wm.c` alone and rebuilding.
- One assertion was removed from the test during review: it looked for the
  last app's label in the composed frame, but desktop icons draw app names too,
  so it passed on the unfixed tree and proved nothing. The labels asserted now
  ("Redraw screen", "Exit") appear nowhere but the menu.
- No warnings from the changed files.

Not tested: the menu on a small screen, where `td_wm_desktop_rows()` limits the
height. This change only lowers the number of apps that can be clipped to zero;
it does not alter the drawing.
