/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "AudioReceiver.h"

#include <aoo.h>

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>

namespace inputleap {
namespace audio {
namespace {

void on_aoo_event(void*, const AooEvent* event, AooThreadLevel)
{
    if (event != nullptr) {
        std::cerr << "audio: AOO event " << event->type << "\n";
    }
}

} // namespace

AudioReceiver::AudioReceiver(const AudioRelayConfig& config,
                             const char* source_host,
                             std::uint16_t source_port,
                             AooId source_id)
    : m_config(config),
      m_source_host(source_host != nullptr ? source_host : ""),
      m_source_port(source_port),
      m_source_id(source_id)
{
}

AudioReceiver::~AudioReceiver()
{
    stop();
}

bool AudioReceiver::setup_network()
{
    m_sink = AooSink::create(1);
    m_client = AooClient::create();
    if (!m_sink || !m_client) {
        std::cerr << "audio: could not create AOO source/sink objects\n";
        return false;
    }

    m_sink->setEventHandler(on_aoo_event, nullptr, kAooEventModeCallback);
    m_sink->setLatency(m_config.jitter_buffer_ms * 0.001);

    AooClientSettings settings;
    settings.portNumber = m_config.media_port;
    if (m_client->setup(settings) != kAooOk) {
        std::cerr << "audio: could not bind AOO media port "
                  << m_config.media_port << "\n";
        return false;
    }

    if (m_sink->setup(m_config.format.channels,
                     m_config.format.sample_rate,
                     m_config.format.frame_samples(),
                     0) != kAooOk) {
        std::cerr << "audio: could not configure AOO sink\n";
        return false;
    }

    m_client->addSink(m_sink.get());

    m_source_address_size = sizeof(m_source_address);
    // AOO's UDP server uses a dual-stack socket on platforms with IPv6.
    // Build numeric IPv4 targets as IPv4-mapped IPv6 addresses so sendto()
    // uses the same address family as that socket. Fall back to DNS for
    // hostnames.
    auto resolve_result = aoo_ipEndpointToSockAddr(
        m_source_host.c_str(), m_source_port, kAooSocketDualStack,
        &m_source_address, &m_source_address_size);
    if (resolve_result != kAooOk) {
        m_source_address_size = sizeof(m_source_address);
        resolve_result = aoo_resolveIpEndpoint(
            m_source_host.c_str(), m_source_port, settings.socketType,
            &m_source_address, &m_source_address_size);
    }
    if (resolve_result != kAooOk) {
        std::cerr << "audio: could not resolve source " << m_source_host
                  << ":" << m_source_port << "\n";
        return false;
    }

    std::array<AooChar, 64> endpoint_host{};
    AooSize endpoint_host_size = endpoint_host.size();
    AooUInt16 endpoint_port = 0;
    AooSocketFlags endpoint_type = kAooSocketDefault;
    if (aoo_sockAddrToIpEndpoint(
            &m_source_address, m_source_address_size,
            endpoint_host.data(), &endpoint_host_size,
            &endpoint_port, &endpoint_type) == kAooOk) {
        std::cerr << "audio: resolved source endpoint " << endpoint_host.data()
                  << ":" << endpoint_port
                  << " socket-flags=" << endpoint_type << "\n";
    }

    m_source_endpoint.address = &m_source_address;
    m_source_endpoint.addrlen = m_source_address_size;
    m_source_endpoint.id = m_source_id;
    return true;
}

bool AudioReceiver::setup_audio_device()
{
    auto device_config = ma_device_config_init(ma_device_type_playback);
    device_config.playback.format = ma_format_f32;
    device_config.playback.channels = m_config.format.channels;
    device_config.sampleRate = m_config.format.sample_rate;
    device_config.periodSizeInFrames = m_config.format.frame_samples();
    device_config.dataCallback = audio_callback;
    device_config.pUserData = this;

    if (ma_device_init(nullptr, &device_config, &m_device) != MA_SUCCESS) {
        std::cerr << "audio: could not open the default playback device\n";
        return false;
    }
    m_device_initialized = true;

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "audio: could not start the playback device\n";
        ma_device_uninit(&m_device);
        m_device_initialized = false;
        return false;
    }
    return true;
}

bool AudioReceiver::start()
{
    if (m_running.load()) {
        return true;
    }

    std::string error;
    if (!m_config.is_valid(&error)) {
        std::cerr << "audio: invalid configuration: " << error << "\n";
        return false;
    }
    if (m_source_host.empty() || m_source_port == 0) {
        std::cerr << "audio: source host and port are required\n";
        return false;
    }
    if (aoo_initialize(nullptr) != kAooOk) {
        std::cerr << "audio: could not initialize AOO\n";
        return false;
    }
    if (!setup_network() || !setup_audio_device()) {
        stop();
        return false;
    }

    m_running.store(true);
    m_send_thread = std::thread(&AudioReceiver::network_send_loop, this);
    m_receive_thread = std::thread(&AudioReceiver::network_receive_loop, this);
    const auto invite_result = m_sink->inviteSource(m_source_endpoint, nullptr);
    if (invite_result != kAooOk) {
        std::cerr << "audio: could not invite source: "
                  << aoo_strerror(invite_result) << "\n";
        stop();
        return false;
    }
    std::cerr << "audio: inviting source " << m_source_host << ":"
              << m_source_port << " id=" << m_source_id << "\n";
    return true;
}

void AudioReceiver::stop()
{
    const bool was_running = m_running.exchange(false);
    if (!was_running && !m_device_initialized && !m_client) {
        return;
    }

    if (m_sink && m_source_endpoint.address != nullptr) {
        m_sink->uninviteSource(m_source_endpoint);
    }
    if (m_client) {
        m_client->stop();
    }
    if (m_send_thread.joinable()) {
        m_send_thread.join();
    }
    if (m_receive_thread.joinable()) {
        m_receive_thread.join();
    }
    if (m_device_initialized) {
        ma_device_uninit(&m_device);
        m_device_initialized = false;
    }
    m_sink.reset();
    m_client.reset();
    aoo_terminate();
}

void AudioReceiver::network_send_loop()
{
    while (m_running.load()) {
        m_client->send(0.1);
    }
}

void AudioReceiver::network_receive_loop()
{
    while (m_running.load()) {
        m_client->receive(0.1);
    }
}

void AudioReceiver::audio_callback(ma_device* device,
                                   void* output,
                                   const void*,
                                   ma_uint32 frame_count)
{
    auto* receiver = static_cast<AudioReceiver*>(device->pUserData);
    if (receiver == nullptr || receiver->m_sink == nullptr ||
        frame_count > 4096) {
        std::memset(output, 0, frame_count * device->playback.channels * sizeof(float));
        return;
    }

    const auto channels = receiver->m_config.format.channels;
    auto* interleaved = static_cast<float*>(output);
    for (std::uint16_t channel = 0; channel < channels; ++channel) {
        receiver->m_channel_pointers[channel] =
            receiver->m_channel_buffer.data() + channel * 4096;
    }

    receiver->m_sink->process(receiver->m_channel_pointers.data(),
                              static_cast<AooInt32>(frame_count),
                              aoo_getCurrentNtpTime(),
                              nullptr,
                              nullptr);

    for (ma_uint32 frame = 0; frame < frame_count; ++frame) {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            interleaved[frame * channels + channel] =
                receiver->m_channel_pointers[channel][frame];
        }
    }
    receiver->m_client->notify();
}

} // namespace audio
} // namespace inputleap
