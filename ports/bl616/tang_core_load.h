// TinyTang — load an FPGA core from the card, the way `tangload` does.
//
// `tangload` is a shell command, and the shell runs a command's work in a
// worker task with a 16 KB stack.  That is fine at the prompt and fails from
// the desktop, where the heap is already spent: the worker or the script's
// line table cannot be allocated, and the load does not happen.  This entry
// point is the same sequence without the shell, so a caller that already owns
// a task can load a core directly.
//
// It is deliberately the only implementation: `cmd_tangload` resolves its
// argument and then calls this, so the shell and the app cannot drift apart.
#ifndef TANG_CORE_LOAD_H
#define TANG_CORE_LOAD_H

#include <stdbool.h>

/* The outcome, kept apart because the shell names a concurrent load and the
 * app does not: both are failures to the caller, but only the command can
 * usefully say which. */
typedef enum {
    TANG_LOAD_OK = 0,
    TANG_LOAD_BUSY,
    TANG_LOAD_FAILED,
} tang_load_result_t;

/* Program the FPGA from a FatFS path (one the port mounts, such as
 * "/sd/root/cores/console138k/tinyphosphor.bin").  Stops the Phosphor player
 * and releases the OLED and PMOD declarations first, and re-applies what
 * /tang.ini declares afterwards, exactly as `tangload` does.  Blocks for the
 * whole erase and programming time. */
tang_load_result_t tang_core_load(const char *real_path);

#endif
