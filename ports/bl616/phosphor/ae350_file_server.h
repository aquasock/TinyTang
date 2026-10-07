// TinyTang: serves file requests from a program running on Tang-Phosphor's
// AE350, after Tang-Control's Tang-PSX disc service (core/tangpsx.cpp,
// Apache-2.0).
//
// The program writes a byte offset and length into a mailbox in its result
// words and then bumps a sequence number (Tang-Phosphor
// software/ae350/include/ae350_request.h); each new sequence is answered with
// one stream session of that range of the file being served, sent by
// fpga_file_stream, which also returns the CRC of what it sent.
#pragma once

#include <stdint.h>

#include "fpga_file_stream.h"

// The mailbox, USER(13..15) of the AE350 register view.
constexpr uint32_t AE350_REG_REQUEST_SEQUENCE = 0x00004074u;
constexpr uint32_t AE350_REG_REQUEST_OFFSET = 0x00004078u;
constexpr uint32_t AE350_REG_REQUEST_LENGTH = 0x0000407cu;

// The largest range one request may ask for.
constexpr uint32_t AE350_REQUEST_MAX = 16u << 20;

struct ae350_file_request {
    uint32_t sequence;
    uint32_t offset;
    uint32_t length;
    fpga_file_stream_result result;  // status INVALID_ARGUMENT when refused
};

using ae350_file_request_report = void (*)(const ae350_file_request &request,
                                           void *context);

struct ae350_file_serve_result {
    uint32_t requests;   // requests answered with a session
    uint32_t refused;    // no file, or a length of 0 or over AE350_REQUEST_MAX
    uint32_t failed;     // sessions that did not complete
    uint32_t bytes;
    uint32_t state;      // the loader state that ended the service
    bool cancelled;
    bool link_failed;    // the core stopped answering register reads
};

// Read the mailbox's sequence as the baseline for ae350_serve_file.  Take it
// before the program is sent, so a request the program makes at once is new.
bool ae350_request_baseline(uint32_t *sequence);

// Serve the running program's requests from the FatFS file `path` (nullptr
// refuses every request) until the loader leaves RUN or `cancel` returns
// true.  States before RUN (an image still being received) are waited
// through for up to two seconds.  `report`, when given, is called after each
// request.
ae350_file_serve_result ae350_serve_file(const char *path, uint32_t baseline,
                                         fpga_file_stream_cancel cancel,
                                         void *cancel_context,
                                         ae350_file_request_report report,
                                         void *report_context);
