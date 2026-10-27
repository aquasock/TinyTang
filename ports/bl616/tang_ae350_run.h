// TinyTang — run an AE350 program on the loaded core, the way `ae350run` does.
//
// `ae350run` is a shell command, and the shell runs a command's work in a
// worker task with a 16 KB stack; a script around it also needs its own line
// table.  That is fine at the prompt and fails from the desktop, where the
// heap is already spent: the worker or the script cannot be given what it
// asks for, and the game does not start.  This entry point is the same
// sequence without the shell, so a caller that already owns a task can load a
// cartridge and serve it directly.
//
// It is deliberately the only implementation: `cmd_ae350run` resolves its
// arguments and then calls this, so the shell and an app cannot drift apart.
#ifndef TANG_AE350_RUN_H
#define TANG_AE350_RUN_H

/* Send <player_real> to the TinyNES core's AE350 and run it, sending
 * <data_real> when the program asks for a file and following its output until
 * it exits.  Both are FatFS paths the port mounts ("/sd/root/..."), already
 * resolved: this does not read a shell session, so it does not map /root onto
 * /sd.  <data_real> may be NULL for a program that wants no file.  Blocks for
 * the whole session -- through the game, past `TinyNES LINK` and until the
 * program exits -- so call it from a task of its own, not from the desktop's.
 * Returns the program's exit code, or 1 when it could not be started. */
int tang_ae350_run(const char *player_real, const char *data_real);

/* The cartridge the last run reported, from the player's CORE line: 1 for a
 * colour cartridge, 0 for a monochrome one, -1 when no run has said.  The
 * TinyGBC window greys the palette row for a colour cartridge, whose picture
 * the setting cannot touch, and it has to ask before the next launch, not
 * after it. */
int tang_ae350_cartridge_cgb(void);

/* Name the file the program may ask for with a "<System> SEND WANT" line: the
 * calculator's link transfer.  A real FatFS path, or NULL for none.  It
 * applies to the next run only; tang_ae350_run clears it when it ends. */
void tang_ae350_set_send_file(const char *path_real);

#endif
