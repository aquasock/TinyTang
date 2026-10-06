# Phosphor memory budget

TinyTang keeps 48 shell variables, 32-byte name buffers, 128-byte value
buffers and a 16,384-byte script worker stack. These are port choices, not
device limits. A BL616 session is 8,328 bytes; each script's copied-session
job is 8,584 bytes before its stack, interpreter and task overhead.

The Phosphor catalog now allocates 96 bytes per eligible file, up to 256
files. With the tested 19-file folder it uses 1,824 bytes instead of 24,576,
recovering 22,752 bytes. Files are still filtered and sorted as before.
An empty folder needs no catalog allocation. Read or allocation errors leave
an empty list with a visible error; the window's close handler frees the list.

## Future ID3 and artwork work

Plan for at most 16,384 additional bytes across metadata and artwork:

| Purpose | Budget in bytes |
| --- | ---: |
| One 92×92 RGB332 cover | 8,464 |
| JPEG decoder working pool | 4,096 |
| Retained metadata text | 1,024 |
| Decoder state, bounded I/O and allocation overhead | 2,800 |
| Total | 16,384 |

The cover dimensions and inactive-bank upload contract come from
`.ai/core-reference.md` record `PHOS-005`. The Bouffalo SDK's
`components/graphics/lvgl/extra/libs/sjpg/lv_sjpg.c` defines
`TJPGD_WORKBUFF_SIZE` as 4,096; this is a decoder implementation choice.

This is a future implementation budget, not an allocated reserve or a
completed ID3/artwork feature. Parse tags and compressed cover data from
the SD card with bounded buffers. Decode blocks into the small cover buffer
and send that buffer to the FPGA; do not retain a whole compressed cover or
a full-resolution decoded image. Keep metadata bounded and handle unsupported
images or oversized tags without allocating their declared size. Any new
task stack must fit within the budget or trigger a fresh memory review.

The 256-file limit is a ceiling, not a promise that every combination of
apps, scripts, a full catalog and future artwork fits. A full catalog still
uses 24,576 bytes. Recheck the budget when adding apps, increasing limits,
or implementing the decoder.

## Reproduce the measurement

Firmware `f3097e1-dirty.a912290`, with the 19-file folder and Bluetooth
keyboard and mouse, passed repeated desktop core swaps. The generated
launcher reported 24,936 bytes free and a 19,372-byte largest free block at
all three stages, with the Phosphor window open and its script worker active.
No allocation refusals were reported. A 16,384-byte additional budget would
leave 8,552 free bytes before any unbudgeted overhead; this supports the
bounded design above, rather than guaranteeing an unspecified future feature.
At the console after the earlier swaps, 75,044 bytes were free with a
62,072-byte largest block, music played at 44.1 kHz with zero underruns, and
the last ordinary script used 10,728 of its 16,384 stack bytes.

Run `tools/tests/test_phosphor_catalog.sh` for catalog sizing, filtering,
sorting, boundaries, failure handling and cleanup. Generate and upload the
script probe while the USB console is at its prompt:

```sh
python3 tools/make_script_heap_probe.py /tmp/castlevania-heap.tdsh
python3 tools/tinytang_put.py /tmp/castlevania-heap.tdsh /scripts/castlevania-heap.tdsh
```

In TinyDesk, load Phosphor, play a song and keep the Phosphor window open.
Run `tdsh run /scripts/castlevania-heap.tdsh` in its Terminal. The readouts
include the active script's session copy, stack and interpreter before the
core load, before the ROM load and after it. Check free bytes, largest free
block and refused allocations, then read `crash` after returning to the
console. Those sampled readings do not measure an all-time heap minimum.
