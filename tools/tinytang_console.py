# SPDX-License-Identifier: MIT
"""Ask the board what its console is doing before sending it anything.

Every tool that writes to the console uses this, because the console is one
byte stream with one reader at a time: at the shell prompt a byte is a command
character, but while the desktop runs it is a keystroke into whichever window
has focus, and while a command such as tangput runs it is that command's data
(TOOL-009).

The board is asked directly.  The probe, ESC [ ? 7 7 n, is taken out of the
USB input by the firmware before the shell or the desktop sees it, and answered
with ESC [ ? 7 7 ; <state> n -- 1 at the shell prompt, 2 with the desktop
running, 3 with a command running (ports/bl616/tdsh_bl616.h).  A running
command reads nothing, so it does not answer in time.  Nothing is sent unless
the answer is 1.

Earlier guards sent a carriage return and looked for the prompt and for the
desktop's drawing; the desktop no longer draws to USB (USB-007), so its
markers never come, and the carriage return was itself a keystroke.  Asking
is the only way to know without typing into whatever is there.
"""

import re
import time

PROBE = b"\x1b[?77n"
REPLY = re.compile(rb"\x1b\[\?77;(\d+)n")

AT_PROMPT, DESKTOP, BUSY = 1, 2, 3
STATES = {
    AT_PROMPT: "at the shell prompt",
    DESKTOP: "running the desktop",
    BUSY: "busy running a command",
}


class ConsoleNotReady(RuntimeError):
    """The console is not at a shell prompt, or did not say."""


def console_state(port, timeout=1.5, on_output=None):
    """Return the console's state (AT_PROMPT, DESKTOP, BUSY), or None if it
    did not answer within `timeout` seconds.  A busy console answers when its
    command ends, so a long timeout waits for the prompt.  Whatever else the
    console prints meanwhile is passed to `on_output`, or dropped; so is
    anything already waiting, which may be a stale answer."""
    on_output = on_output or (lambda data: None)
    on_output(port.read(port.in_waiting))
    port.write(PROBE)
    port.flush()
    seen = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        seen += port.read(256)
        m = REPLY.search(seen)
        if m:
            on_output(seen[:m.start()] + seen[m.end():])
            return int(m.group(1))
    on_output(seen)
    return None


def require_shell(port, timeout=1.5, on_output=None):
    """Raise ConsoleNotReady unless the console is at a shell prompt."""
    state = console_state(port, timeout, on_output)
    if state == AT_PROMPT:
        return
    if state is None:
        raise ConsoleNotReady(
            "the console did not answer the status probe -- a command is "
            "running, the board is not up, or its firmware predates the probe; "
            "refusing to send")
    raise ConsoleNotReady(
        f"the console is {STATES.get(state, f'in state {state}')}; refusing to "
        "send" + (" -- exit the desktop first" if state == DESKTOP else ""))
