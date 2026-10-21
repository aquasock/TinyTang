# Phosphor: music and the scope

Phosphor plays the music on the card and draws it as an XY scope.

## Player tab

     [Library list ]   the folders and tracks
     Prev Play Pause Stop Next
     [x] Play the next track when one ends
     Load              load the Phosphor core

Press Load once. The screen hands over to the scope; F12 brings the
desktop back.

## Scope tab

     [x] Enable the XY scope
     Persistence (Afterglow)  67ms 133ms 267ms 533ms
     [ ] Swap X/Y   [ ] Dots
     Channels  XY | X | Y
     [ ] Bloom

- Persistence: how long a lit point stays lit.
- Swap X/Y: draws right on Y and left on X, to check a swapped lead.
- Dots: points only, no lines between them.
- Channels: both, or one alone, to look for a dead side.
- Bloom: a pale fringe around the trace.

The counters under the controls say how many sweeps have been drawn
and how many samples were dropped.

## Formats

WAV, FLAC, MP3, MP2, Ogg Vorbis, Opus, AAC, ALAC, WavPack, WMA, AC-3
and TTA all play, at any file size. Audio goes to the Pmod I2S2 and
to HDMI at the same time.

## If it will not start

Press Load before choosing a track. A track played while no core is
loaded is refused, and the window says so on its own status line.
