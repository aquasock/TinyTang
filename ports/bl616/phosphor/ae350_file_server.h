// TinyTang: serves file requests from a program running on Tang-Phosphor's
// AE350, after Tang-Control's Tang-PSX disc service (core/tangpsx.cpp,
// Apache-2.0).
//
// The program writes a byte offset and length into a mailbox in its result
// words and then bumps a sequence number (Tang-Phosphor
// software/ae350/include/ae350_request.h); each new sequence is answered with
// one stream session of that range of the file being served, sent by
// fpga_file_stream, which also returns the CRC of what it sent.  A request of
// length 0 asks for the file's size instead, answered as a four-byte session
// holding it least significant byte first.
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
    uint32_t length;                 // 0 for a size query
    fpga_file_stream_result result;  // status INVALID_ARGUMENT when refused
};

using ae350_file_request_report = void (*)(const ae350_file_request &request,
                                           void *context);

// One file being served, and what serving it has done so far.
struct ae350_file_server {
    const char *path;    // FatFS path; nullptr refuses every request
    uint32_t last;       // the last sequence answered, or the baseline
    uint32_t requests;   // requests answered with a session
    uint32_t refused;    // no file, or a length over AE350_REQUEST_MAX
    uint32_t failed;     // sessions that did not complete
    uint32_t bytes;
    fpga_file_stream_status last_status;   // the latest request's
};

// Read the mailbox's sequence as the baseline.  Take it before the program is
// sent, so a request the program makes at once is new.
bool ae350_request_baseline(uint32_t *sequence);

void ae350_file_server_begin(ae350_file_server &server, const char *path, uint32_t baseline);

// Answer the request waiting in the mailbox, if there is one; *served says
// whether there was.  A changed sequence is answered only while the loader is
// in RUN, since a trap also writes the mailbox's words.  Returns false
// when the core stops answering register reads.  `report`, when given, is
// called after each request.
bool ae350_file_server_step(ae350_file_server &server, fpga_file_stream_cancel cancel,
                            void *cancel_context, ae350_file_request_report report,
                            void *report_context, bool *served);

struct ae350_file_serve_result {
    uint32_t state;      // the loader state that ended the service
    bool cancelled;
    bool link_failed;    // the core stopped answering register reads
};

// Serve the running program's requests until the loader leaves RUN or
// `cancel` returns true.  States before RUN (an image still being received)
// are waited through for up to two seconds.
ae350_file_serve_result ae350_serve_file(ae350_file_server &server,
                                         fpga_file_stream_cancel cancel,
                                         void *cancel_context,
                                         ae350_file_request_report report,
                                         void *report_context);
