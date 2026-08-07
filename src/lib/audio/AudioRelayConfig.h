/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <string>

namespace inputleap {
namespace audio {

constexpr std::uint32_t kDefaultSampleRate = 48000;
constexpr std::uint16_t kDefaultChannels = 2;
constexpr std::uint16_t kDefaultFrameDurationMs = 20;
constexpr std::uint16_t kDefaultJitterBufferMs = 60;
constexpr std::uint16_t kDefaultMediaPort = 24801;
constexpr std::uint16_t kMaxAudioCallbackFrames = 4096;

struct AudioFormat {
    std::uint32_t sample_rate = kDefaultSampleRate;
    std::uint16_t channels = kDefaultChannels;
    std::uint16_t frame_duration_ms = kDefaultFrameDurationMs;

    std::uint16_t frame_samples() const;
    bool is_valid() const;
};

struct AudioRelayConfig {
    std::uint16_t media_port = kDefaultMediaPort;
    std::uint16_t jitter_buffer_ms = kDefaultJitterBufferMs;
    std::uint16_t bitrate_kbps = 128;
    AudioFormat format;

    bool is_valid(std::string* error = nullptr) const;
};

} // namespace audio
} // namespace inputleap
