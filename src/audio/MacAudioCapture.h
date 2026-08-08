/*
 * InputLeap -- macOS CoreAudio device capture
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include "audio/AudioRelayConfig.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace inputleap {
namespace audio {

struct MacAudioDeviceInfo {
    std::uint32_t id = 0;
    std::string name;
    std::string uid;
    std::uint32_t input_channels = 0;
    std::uint32_t output_channels = 0;
    double sample_rate = 0.0;
};

std::vector<MacAudioDeviceInfo> list_mac_audio_devices(
    std::string* error = nullptr);

using MacAudioFrameCallback = void (*)(void* user,
                                        const float* interleaved,
                                        std::uint32_t frames,
                                        std::uint16_t channels);

class MacAudioCapture {
public:
    MacAudioCapture(const AudioRelayConfig& config,
                    const std::string& device_uid,
                    MacAudioFrameCallback callback,
                    void* user);
    ~MacAudioCapture();

    bool start(std::string* error = nullptr);
    void stop();
    bool is_running() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace audio
} // namespace inputleap
