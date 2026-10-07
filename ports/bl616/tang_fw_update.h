// TinyTang — replace the BL616 firmware with an image on the SD card.
//
// One engine for both front ends: the shell's `tangflash` and the desktop's
// Software Update window.  The image is checked, copied into the staging area
// and verified there a sector at a time, so a caller can show progress or
// yield between steps; nothing touches the running application until
// tang_fw_update_commit(), which does not return (FLS-002).

#ifndef TANG_FW_UPDATE_H
#define TANG_FW_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FIL file;
    bool open;
    uint32_t image_size;    // bytes, from the boot header; equals the file size
    uint32_t staged;        // bytes copied into the staging area
    uint32_t verified;      // bytes of the staged copy checked against the file
    char error[64];
} tang_fw_update_t;

// Open the image at a real path ("/sd/...") and check its boot header and
// length.  Returns NULL when it can be installed, else a short reason; the
// file is closed on failure.
const char *tang_fw_update_open(tang_fw_update_t *u, const char *real_path);

// Stage or verify one sector.  Returns NULL on success and sets *done once the
// whole staged copy has been verified, else a short reason; the file is closed
// on failure.  Safe to stop at any point before the commit: only the staging
// area has been written.
const char *tang_fw_update_step(tang_fw_update_t *u, bool *done);

// Progress through staging and verification, 0 to 100.
unsigned tang_fw_update_percent(const tang_fw_update_t *u);

// Abandon an open update.  The running firmware is untouched.
void tang_fw_update_close(tang_fw_update_t *u);

// Copy the verified staged image over the application and reset.  Only after
// tang_fw_update_step() has reported done.  Interrupts are off from here on,
// and the reset lands in the vendor loader, so the board must then be
// power-cycled to run the new firmware (FLS-002).
__attribute__((noreturn)) void tang_fw_update_commit(tang_fw_update_t *u);

#ifdef __cplusplus
}
#endif

#endif
