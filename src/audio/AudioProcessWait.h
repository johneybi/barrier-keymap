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

#include <chrono>
#include <csignal>
#include <thread>

namespace inputleap {
namespace audio {

inline volatile std::sig_atomic_t g_audio_shutdown_requested = 0;

inline void request_audio_shutdown(int)
{
    g_audio_shutdown_requested = 1;
}

inline void wait_for_audio_shutdown()
{
    std::signal(SIGINT, request_audio_shutdown);
    std::signal(SIGTERM, request_audio_shutdown);
    while (g_audio_shutdown_requested == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

} // namespace audio
} // namespace inputleap
