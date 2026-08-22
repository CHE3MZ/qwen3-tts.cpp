#pragma once
// audio_player.h — minimal blocking playback of in-memory PCM via miniaudio.
// Used by the CLI --speak flag; no disk I/O involved.

#include <cstdint>
#include <vector>

namespace qwen3_tts {

// Plays interleaved float32 mono samples at the given sample rate.
// Blocks until playback completes. Returns false on device error
// (error detail printed to stderr).
bool play_audio_samples(const float * samples, int32_t n_samples,
                        int32_t sample_rate);

} // namespace qwen3_tts
