// TinyTang: Tang-Control's utils/ae350_play.h (Apache-2.0), ported; see
// ae350_play.cpp.
#pragma once

#include <stdint.h>

#include "fpga_file_stream.h"

// The AE350 subsystem's register view in the merged core (Tang-Phosphor
// docs/debug-registers.md, 0x4000-0x43ff; software/ae350/include/ae350.h).
constexpr uint32_t AE350_REG_STATE = 0x00004020u;    // loader state, runs in 31:16
constexpr uint32_t AE350_REG_RESULT = 0x0000402cu;   // last program's return value
constexpr uint32_t AE350_REG_USER0 = 0x00004040u;    // USER(n) at + 4 * n
constexpr uint32_t AE350_REG_RESTART = 0x000043f0u;

// Restart the AE350's loader and send it the resident player
// (/ae350/resident.tpi), leaving cpu_mode set so the decoded PCM reaches the
// pcm_sink.  *baseline is the request mailbox's sequence from before the
// player was sent, for serving its requests (ae350_file_server).  Returns
// true on success; on failure returns false and, when error_out is non-null,
// sets it to a short message.  `cancel`, when given, is polled between stream
// frames, as fpga_file_stream does; a cancelled send fails with "cancelled".
bool ae350_start_player(uint32_t *baseline, const char **error_out,
                        fpga_file_stream_cancel cancel = nullptr,
                        void *cancel_context = nullptr);

// Register access over the extended protocol; false when the core does not
// answer or reports an error.
bool ae350_read32(uint32_t address, uint32_t *value);
bool ae350_poke32(uint32_t address, uint32_t value);

// Check the core's register ABI and route the stream to the AE350 (0xa8).
bool ae350_select(const char **error_out);

// Restart the AE350's boot loader and wait until it is ready for an image.
bool ae350_restart_loader(const char **error_out);
