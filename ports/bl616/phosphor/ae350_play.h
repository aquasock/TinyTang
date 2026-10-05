// TinyTang: Tang-Control's utils/ae350_play.h (Apache-2.0), ported; see
// ae350_play.cpp.
#pragma once

// Stream the resident AE350 player image and then the SD audio file at
// full_path (a FatFS path such as "/sd/music/track.flac"), leaving cpu_mode
// set so the decoded PCM reaches the pcm_sink and playback proceeds
// asynchronously.  Returns true on success.  On failure returns false and,
// when error_out is non-null, sets it to a short message.
bool ae350_play_file(const char *full_path, const char **error_out);
