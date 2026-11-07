**Not filed, and superseded.** This issue was drafted and the user chose to
send the fix as a pull request instead, since the change was already written and
tested and a bare PR is less work to review than an issue plus a PR. The
published form is
[tinydesk-project/tinydesk PR #18](https://github.com/tinydesk-project/tinydesk/pull/18);
`docs/upstream/menu-max-pr.md` holds its text. Nothing was ever filed on GitHub
as an issue.

The draft is kept because it explains the failure more fully than the PR body
does -- in particular the index arithmetic that makes "Exit" redraw instead of
quit at the default `TD_MAX_APPS`, which is the part a future reader of this
repository is most likely to need.

Everything below the rule is the issue as it was drafted.

---

## Raising `TD_MAX_APPS` silently truncates the start menu, and then "Exit" stops working

`src/wm.c:644` hardcodes `#define MENU_MAX 20`, while `TD_MAX_APPS`
(`include/tinydesk/td_config.h:110`) is `#ifndef`-wrapped and therefore meant to
be overridden per port. Nothing ties the two together, so a port that raises
`TD_MAX_APPS` gets a start menu that drops entries without saying so.

Two things then break at once.

`td_wm_toggle_start_menu()` (`src/wm.c:857`) caps the app section at
`MENU_MAX - 3 - s_start_extra_count`, so apps beyond that are never placed.
`start_menu_chosen()` (`src/wm.c:782`) then interprets the chosen index from
`s_app_count` — the number of *registered* apps, not the number the menu
actually placed:

```c
int extra = i - s_app_count - 1;       /* after the separator */
if (i < s_app_count)
    launch_app(s_apps[i]);
```

Once anything has been dropped, every entry below the separator is off by the
difference. An app index can reach the separator, "Redraw screen" can launch an
app, and "Exit" lands past the end of the chain and does nothing. `td_quit()` is
reached only from that entry, so the start menu can no longer close the desktop.

### It is reachable at the default configuration

No build change is needed. `apps/session.c:207` adds "Switch user..." as a start
item, which leaves exactly zero margin at the default `TD_MAX_APPS` of 16
(`20 - 3 - 1 = 16`). **A second start item** — in a port, a test, or a build
that adds one more — drops the last app and starts the failure:

- 16 apps and 2 start items: the menu places 15 apps, separator, 2 extras,
  "Redraw screen", "Exit".
- Choosing "Exit" (index 19) computes `extra = 19 - 16 - 1 = 2`, which equals
  `s_start_extra_count`, so it **redraws the screen instead of quitting**.
- Choosing "Redraw screen" (index 18) computes `extra = 1`, which is a valid
  extra index, so it **runs a start item's action** instead.

Observed on TinyTang, which registers 19 apps: the menu placed 17, "Redraw
screen" launched `s_apps[18]` — an app that was never drawn — and "Exit" did
nothing at all.

### Suggested fix

Two changes in `src/wm.c`, no API change:

1. Size the menu from both limits instead of picking 20:
   `#define MENU_MAX (TD_MAX_APPS + START_EXTRA_MAX + 3)` — every app, every
   extra start item, the separator, "Redraw screen" and "Exit". `START_EXTRA_MAX`
   is already defined at `src/wm.c:60`.
2. Record how many apps the open menu actually placed, and offset the tail by
   that rather than by `s_app_count`, so the callback stays correct even if a
   menu is ever clipped for another reason.

A regression test needs no build configuration: fill the registry at the
default 16, add two start items, press END and ENTER, and assert `td_step()`
returns false. It fails on the current tree at exactly that assertion.

I have both changes and the test ready and can send them as a PR if that is
useful — say so and I will open one; otherwise the report stands on its own.
