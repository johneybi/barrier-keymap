/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "AudioRelayConfig.h"

namespace inputleap {
namespace audio {

std::uint16_t AudioFormat::frame_samples() const
{
    return static_cast<std::uint16_t>(sample_rate * frame_duration_ms / 1000);
}

bool AudioFormat::is_valid() const
{
    return sample_rate >= 8000 && sample_rate <= 192000 &&
           channels >= 1 && channels <= 8 &&
           frame_duration_ms >= 5 && frame_duration_ms <= 60 &&
           (sample_rate * frame_duration_ms) % 1000 == 0 &&
           frame_samples() > 0;
}

bool AudioRelayConfig::is_valid(std::string* error) const
{
    const auto fail = [error](const char* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    if (media_port == 0 || media_port == 24800) {
        return fail("audio media port must be separate from the input port");
    }
    if (jitter_buffer_ms < 20 || jitter_buffer_ms > 500) {
        return fail("audio jitter buffer must be between 20 and 500 ms");
    }
    if (bitrate_kbps < 16 || bitrate_kbps > 512) {
        return fail("audio bitrate must be between 16 and 512 kbps");
    }
    if (!format.is_valid()) {
        return fail("audio format is invalid");
    }
    return true;
}

} // namespace audio
} // namespace inputleap
