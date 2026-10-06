# Draft issues for tinydesk-project/tinydesk-shell

Not posted yet. STANDARDS.md §9 lists these topics as "not standardised yet"
and asks ports to open issues describing what they did, so a rule can be
written from a real case. They are written in the first person because they
post under the maintainer's account (@aquasock); post only with the user's
approval. One detail is deliberately left open in issue A: whether TinyTang
also runs `~/.tdshrc.tdsh` has not been checked, so the draft only asks about
the order.

---

## A. Boot script: a system-wide script that runs before anyone logs in

STANDARDS.md §9 lists a system-wide boot script as not standardised yet. Here is what TinyTang does, as a real case.

**What it does.** At power-up, after the banner and before the first prompt, TinyTang runs `/scripts/boot.tdsh` from the SD card once, as root, through the shell's own `tdsh run`. If the file is missing, the board comes up at a plain prompt. Nothing else is special about it: it is an ordinary script with the same commands and paths as anything typed at the console, and if it fails the prompt is still there to say why.

**Why it exists.** It decides what the board *is* at power-up, without reflashing: TinyTang's loads an FPGA core, probes it and turns on the desktop layer on HDMI; pointing it at another core makes the board boot into a game instead. This is different from `~/.tdshrc.tdsh`, which belongs to one user's session; the boot script configures the machine, once, before any session.

**Questions a rule would need to answer:**
- Path: TinyTang uses `/scripts/boot.tdsh`, which isn't in the §6 layout. Would `/etc/boot.tdsh` (or `/etc/rc.tdsh`) be the right home?
- Does it always run as root?
- Does it run before or after the console user's `~/.tdshrc.tdsh`?
- Is there a standard way to skip it at power-up (a held key, a flag file), for when it leaves the board in a bad state?
- Should its output go to the console like any command, or be quieter?

---

## B. Displays outside the terminal: TinyTang's desktop layer on HDMI

STANDARDS.md §9 lists displays drawn outside the terminal, and how they share the screen with the desktop, as not standardised yet. This is TinyTang's case.

**The hardware.** On the Tang Console the BL616 runs TinyDesk, and the picture on HDMI comes from the FPGA core (an NES core, a music player…), which also composites an 80×45 cell text layer over its own video at 1280×720.

**What TinyTang does.** TinyDesk draws into a terminal emulator (`td_vterm`) on the BL616; a port task diffs it against what the core has and sends only changed cells over a UART (a cursor command, then 5 bytes per cell: character, foreground and background colour; plus an enable command), and the core draws them in the layer. While the layer shows the desktop, the desktop's drawing goes only to the layer, and USB carries just the shell's text. The bare console, with no desktop running, is mirrored into the same layer, so the HDMI screen always shows the session. **F12** (or L on a gamepad) switches the screen between TinyDesk and the core's own picture, the way MiSTer reserves F12 for its menu: the desktop keeps running underneath, and coming back is instant. While the core has the screen, keyboard and pad input belong to it, not to TinyDesk.

**Questions a rule would need to answer:**
- Should `td_hal` describe a second output like this (an off-terminal cell grid), or is it purely a port matter?
- How should TinyDesk behave while its screen is hidden (pause redraws, keep running, notify apps)?
- Should the switch key be standard across ports (F12, as on MiSTer), and reserved so apps never see it?
- Is a session mirrored to two places (here HDMI and USB, with the desktop's drawing on one only) something the core should know about?

---

## C. Media playback: TinyTang's `phosphor` command and app

STANDARDS.md §9 lists audio and media playback commands as not standardised yet. TinyTang has one, so here it is as a case.

**What it plays.** Music decoded on the FPGA side (a RISC-V core running Rockbox's codecs inside the Tang-Phosphor FPGA core) and sent to HDMI audio; twelve formats so far, including MP3, FLAC, Ogg Vorbis, Opus, AAC and WAV.

**The command,** one command with verbs as subcommands, per §4:
- `phosphor play <file> [nowait]`: blocks until the track ends unless `nowait`; Ctrl-C stops it
- `phosphor status`: idle / loading / playing / ended / stopped / failed, with file, elapsed time, sample rate and underruns
- `phosphor stop`, `phosphor pause`, `phosphor resume`
- plus core-specific `caps`, `stats`, `peek` and `poke`

Playback runs on a background task, so the shell stays free; loading a different core stops it.

**The app.** A TinyDesk window listing `/music`, with title, status line, progress bar, Prev / Play / Pause / Stop / Next and auto-advance, using the same background task rather than the command line.

**Questions a rule would need to answer:**
- A common command name (`play`, `media`, `audio`) with the backend chosen by the port, or port-named commands like this one?
- Standard verbs (`play`, `stop`, `pause`, `resume`, `status`) and a standard status format, so scripts work across ports?
- A standard folder for music (`~/Music`, `/music`)?
- Should Files open audio files in the player, the way it runs `.tdsh` scripts in the Terminal?
