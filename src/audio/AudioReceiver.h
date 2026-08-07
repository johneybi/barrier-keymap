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

#include "audio/AudioRelayConfig.h"

#include <aoo.h>
#include <aoo_client.hpp>
#include <aoo_sink.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <miniaudio.h>

namespace inputleap {
namespace audio {

class AudioReceiver {
public:
    AudioReceiver(const AudioRelayConfig& config,
                  const char* source_host,
                  std::uint16_t source_port,
                  AooId source_id);
    ~AudioReceiver();

    bool start();
    void stop();
    bool is_running() const { return m_running.load(); }

private:
    static void audio_callback(ma_device* device,
                               void* output,
                               const void* input,
                               ma_uint32 frame_count);

    bool setup_network();
    bool setup_audio_device();
    void network_send_loop();
    void network_receive_loop();

    AudioRelayConfig m_config;
    std::string m_source_host;
    std::uint16_t m_source_port;
    AooId m_source_id;

    AooSockAddrStorage m_source_address{};
    AooAddrSize m_source_address_size = 0;
    AooEndpoint m_source_endpoint{};
    AooSink::Ptr m_sink;
    AooClient::Ptr m_client;

    ma_device m_device{};
    bool m_device_initialized = false;
    std::atomic<bool> m_running{false};
    std::thread m_send_thread;
    std::thread m_receive_thread;

    std::array<float, 8 * kMaxAudioCallbackFrames> m_channel_buffer{};
    std::array<AooSample*, 8> m_channel_pointers{};
};

} // namespace audio
} // namespace inputleap
