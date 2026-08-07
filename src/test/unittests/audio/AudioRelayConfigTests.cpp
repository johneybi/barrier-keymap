/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "audio/AudioRelayConfig.h"

#include <gtest/gtest.h>

namespace inputleap {
namespace audio {

TEST(AudioRelayConfigTests, defaultConfigIsValid)
{
    AudioRelayConfig config;
    std::string error;
    EXPECT_TRUE(config.is_valid(&error));
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(960u, config.format.frame_samples());
    EXPECT_LT(config.format.frame_samples(), kMaxAudioCallbackFrames);
}

TEST(AudioRelayConfigTests, inputPortCannotBeUsedForAudio)
{
    AudioRelayConfig config;
    std::string error;
    config.media_port = 24800;
    EXPECT_FALSE(config.is_valid(&error));
    EXPECT_EQ("audio media port must be separate from the input port", error);
}

TEST(AudioRelayConfigTests, rejectsUnsafeJitterBuffer)
{
    AudioRelayConfig config;
    std::string error;
    config.jitter_buffer_ms = 10;
    EXPECT_FALSE(config.is_valid(&error));
    EXPECT_EQ("audio jitter buffer must be between 20 and 500 ms", error);
}

} // namespace audio
} // namespace inputleap
