/*
 * InputLeap -- macOS system audio sender
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include "audio/AudioRelayConfig.h"

#include <aoo_types.h>

#include <cstdint>
#include <memory>

namespace inputleap {
namespace audio {

class AudioSenderImpl;

class AudioSender {
public:
    AudioSender(const AudioRelayConfig& config, AooId source_id);
    ~AudioSender();

    bool start();
    void stop();
    bool is_running() const;

private:
    std::unique_ptr<AudioSenderImpl> m_impl;
};

} // namespace audio
} // namespace inputleap
