// TinyTang — the crash and hang recorder.
//
// A crash on this board used to look exactly like a hang: the SDK's exception
// handler prints to a UART nobody can see and then spins with interrupts off,
// so the screen froze, Bluetooth dropped, USB stayed enumerated and the
// console went silent.  A real hang looked the same.  This makes both leave
// evidence and recover:
//
//   * A hardware watchdog resets the chip if it is not fed for 4 s.  It is fed
//     by a high-priority task, so anything that stops the scheduler -- a trap,
//     interrupts held off, a top-priority task spinning -- resets the board.
//     That task also stops feeding it if a lowest-priority heartbeat has not
//     run for 30 s, which catches a task spinning without ever yielding.
//   * The exception handler, the stack-overflow hook, the malloc-failure hook
//     and the heartbeat check record what happened and reset the board.
//   * The tick hook keeps a trail of the last 32 (task, PC) samples, 4 ms
//     apart, so the record shows what was running up to the failure.
//
// A warm reset on this board lands in the vendor loader, which starts the
// FT2232 debugger rather than this firmware (FLS-001); only a power cycle
// brings TinyTang back, and that scrambles RAM.  So the record and the trail
// are written to the last sector of the flash (0x3FF000) before the reset, and
// the next boot prints them, appends them to /sd/crash.log and erases the
// sector.  A hang with interrupts held off runs no code before the watchdog
// fires, so it leaves no record: a board found in FT2232 mode with nothing
// recorded points to exactly that.  `crash` shows the record again and can
// cause each kind on purpose.
//
// SPDX-License-Identifier: MIT

#ifndef TANG_CRASH_H
#define TANG_CRASH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* First thing in main: take the previous run's record aside and start a new
 * one.  Touches no peripherals but the reset-status register. */
void tang_crash_init(void);

/* Before the scheduler starts: create the watchdog and heartbeat tasks.  The
 * watchdog itself starts when its task first runs. */
void tang_crash_start(void);

/* From the shell, once the card is mounted: print what the previous run left,
 * if anything, and append it to /sd/crash.log.  Once only. */
void tang_crash_report(void);

/* Stop the watchdog for good before a deliberate reset that takes longer
 * than its timeout -- tangflash's commit, which runs with interrupts off --
 * and mark the reset as intended. */
void tang_crash_suspend(void);

int tang_crash_register(void);

#ifdef __cplusplus
}
#endif

#endif /* TANG_CRASH_H */
