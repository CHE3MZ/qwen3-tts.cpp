// audio_player.cpp — miniaudio-backed blocking playback for the CLI --speak flag.
// The MINIAUDIO_IMPLEMENTATION translation unit lives here (only place).

#include "audio_player.h"

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace qwen3_tts {

namespace {

struct PlayContext {
    const float * samples;
    size_t         total;
    std::atomic<size_t> read_cursor;
};

void data_callback(ma_device * device, void * output, const void * /*input*/,
                   ma_uint32 frame_count) {
    PlayContext * ctx = (PlayContext *)device->pUserData;
    if (!ctx) return;

    float * out = (float *)output;
    ma_uint32 frames_to_do = frame_count;
    while (frames_to_do > 0) {
        size_t remaining = ctx->total - ctx->read_cursor.load(std::memory_order_relaxed);
        if (remaining == 0) {
            std::memset(out, 0, frames_to_do * sizeof(float));
            break;
        }
        ma_uint32 n = (ma_uint32)((remaining < frames_to_do) ? remaining : frames_to_do);
        std::memcpy(out, ctx->samples + ctx->read_cursor.load(std::memory_order_relaxed),
                    n * sizeof(float));
        ctx->read_cursor.fetch_add(n, std::memory_order_relaxed);
        out += n;
        frames_to_do -= n;
    }
}

} // namespace

bool play_audio_samples(const float * samples, int32_t n_samples,
                        int32_t sample_rate) {
    if (!samples || n_samples <= 0 || sample_rate <= 0) return false;

    PlayContext ctx;
    ctx.samples = samples;
    ctx.total   = (size_t)n_samples;
    ctx.read_cursor.store(0, std::memory_order_relaxed);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate        = (ma_uint32)sample_rate;
    config.dataCallback      = data_callback;
    config.pUserData         = &ctx;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to open audio playback device\n");
        return false;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to start audio playback device\n");
        ma_device_uninit(&device);
        return false;
    }

    // Wait until the callback has consumed everything, then drain a little
    // extra so the tail isn't clipped by device shutdown.
    while (ctx.read_cursor.load(std::memory_order_relaxed) < ctx.total) {
        ma_sleep(10);
    }
    ma_sleep(150);

    ma_device_uninit(&device);
    return true;
}

} // namespace qwen3_tts
