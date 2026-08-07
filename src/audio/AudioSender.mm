/*
 * InputLeap -- macOS system audio sender
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "AudioSender.h"

#include <aoo.h>
#include <aoo_client.hpp>
#include <aoo_source.hpp>
#include <codec/aoo_opus.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <thread>

namespace inputleap {
namespace audio {
namespace {

constexpr std::size_t kMaxCaptureFrames = 4096;
constexpr std::size_t kMaxChannels = 8;

} // namespace

class AudioSenderImpl;

} // namespace audio
} // namespace inputleap

@interface ScreenAudioOutput : NSObject <SCStreamOutput, SCStreamDelegate>
@property(nonatomic, assign) inputleap::audio::AudioSenderImpl* owner;
@end

namespace inputleap {
namespace audio {

class AudioSenderImpl {
public:
    AudioSenderImpl(const AudioRelayConfig& config, AooId source_id)
        : m_config(config), m_source_id(source_id)
    {
    }

    ~AudioSenderImpl()
    {
        stop();
    }

    bool start();
    void stop();
    bool is_running() const { return m_running.load(); }

    void handle_sample(CMSampleBufferRef sample_buffer);
    void handle_stream_error(NSError* error);

private:
    bool setup_network();
    bool setup_capture();
    bool start_capture();
    void stop_capture();
    void network_send_loop();
    void network_receive_loop();

    AudioRelayConfig m_config;
    AooId m_source_id;
    AooSource::Ptr m_source;
    AooClient::Ptr m_client;

    __strong SCStream* m_stream = nil;
    __strong ScreenAudioOutput* m_output = nil;
    dispatch_queue_t m_capture_queue = nil;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_capture_failed{false};
    bool m_aoo_initialized = false;
    bool m_capture_started = false;
    std::thread m_send_thread;
    std::thread m_receive_thread;

    std::array<float, kMaxChannels * kMaxCaptureFrames> m_channel_buffer{};
    std::array<AooSample*, kMaxChannels> m_channel_pointers{};
};

} // namespace audio
} // namespace inputleap

@implementation ScreenAudioOutput

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
    if (type == SCStreamOutputTypeAudio && self.owner != nullptr) {
        self.owner->handle_sample(sampleBuffer);
    }
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
    if (self.owner != nullptr) {
        self.owner->handle_stream_error(error);
    }
}

@end

namespace inputleap {
namespace audio {

bool AudioSenderImpl::setup_network()
{
    m_source = AooSource::create(m_source_id);
    m_client = AooClient::create();
    if (!m_source || !m_client) {
        std::cerr << "audio: could not create AOO source/client objects\n";
        return false;
    }

    m_source->setEventHandler(
        [](void* user, const AooEvent* event, AooThreadLevel) {
            auto* sender = static_cast<AudioSenderImpl*>(user);
            if (event->type == kAooEventInvite) {
                std::cerr << "audio: accepting sink invitation\n";
                const auto result = sender->m_source->handleInvite(
                    event->invite.endpoint, event->invite.token, kAooTrue);
                if (result != kAooOk) {
                    std::cerr << "audio: could not accept sink invitation: "
                              << aoo_strerror(result) << "\n";
                }
            } else if (event->type == kAooEventUninvite) {
                const auto result = sender->m_source->handleUninvite(
                    event->uninvite.endpoint, event->uninvite.token, kAooTrue);
                if (result != kAooOk) {
                    std::cerr << "audio: could not accept sink uninvite: "
                              << aoo_strerror(result) << "\n";
                }
            }
        },
        this,
        kAooEventModeCallback);

    AooClientSettings settings;
    settings.portNumber = m_config.media_port;
    if (m_client->setup(settings) != kAooOk) {
        std::cerr << "audio: could not bind AOO media port "
                  << m_config.media_port << "\n";
        return false;
    }

    if (m_source->setup(m_config.format.channels,
                        m_config.format.sample_rate,
                        m_config.format.frame_samples(),
                        0) != kAooOk) {
        std::cerr << "audio: could not configure AOO source\n";
        return false;
    }

    AooFormatOpus format;
    AooFormatOpus_init(&format,
                       m_config.format.channels,
                       m_config.format.sample_rate,
                       m_config.format.frame_samples(),
                       OPUS_APPLICATION_AUDIO);
    if (m_source->setFormat(format.header) != kAooOk) {
        std::cerr << "audio: could not configure Opus stream format\n";
        return false;
    }
    AooSource_setOpusBitrate(
        m_source.get(), nullptr, m_config.bitrate_kbps * 1000);

    if (m_client->addSource(m_source.get()) != kAooOk) {
        std::cerr << "audio: could not register AOO source\n";
        return false;
    }
    return true;
}

bool AudioSenderImpl::setup_capture()
{
    if (@available(macOS 13.0, *)) {
        dispatch_semaphore_t content_semaphore = dispatch_semaphore_create(0);
        __block SCShareableContent* content = nil;
        __block NSError* content_error = nil;

        [SCShareableContent getShareableContentWithCompletionHandler:
            ^(SCShareableContent* shareable_content, NSError* error) {
                content = shareable_content;
                content_error = error;
                dispatch_semaphore_signal(content_semaphore);
            }];
        dispatch_semaphore_wait(content_semaphore, DISPATCH_TIME_FOREVER);

        if (content_error != nil || content == nil ||
            content.displays.count == 0) {
            std::cerr << "audio: could not access a shareable display";
            if (content_error != nil) {
                std::cerr << ": "
                          << content_error.localizedDescription.UTF8String;
            }
            std::cerr << "\nGrant Screen Recording permission and try again.\n";
            return false;
        }

        auto* display = content.displays.firstObject;
        auto* filter = [[SCContentFilter alloc]
            initWithDisplay:display excludingWindows:@[]];
        auto* configuration = [[SCStreamConfiguration alloc] init];
        configuration.capturesAudio = YES;
        configuration.excludesCurrentProcessAudio = YES;
        configuration.sampleRate = m_config.format.sample_rate;
        configuration.channelCount = m_config.format.channels;
        configuration.queueDepth = 3;

        m_capture_queue = dispatch_queue_create(
            "com.johneybi.input-leap-keymap.audio-capture",
            DISPATCH_QUEUE_SERIAL);
        m_output = [[ScreenAudioOutput alloc] init];
        m_output.owner = this;
        m_stream = [[SCStream alloc] initWithFilter:filter
                                       configuration:configuration
                                            delegate:m_output];
        if (m_stream == nil) {
            std::cerr << "audio: could not create ScreenCaptureKit stream\n";
            return false;
        }

        NSError* output_error = nil;
        if (![m_stream addStreamOutput:m_output
                                  type:SCStreamOutputTypeAudio
                    sampleHandlerQueue:m_capture_queue
                                error:&output_error]) {
            std::cerr << "audio: could not add audio stream output";
            if (output_error != nil) {
                std::cerr << ": " << output_error.localizedDescription.UTF8String;
            }
            std::cerr << "\n";
            return false;
        }
        return true;
    }

    std::cerr << "audio: system audio capture requires macOS 13.0 or newer\n";
    return false;
}

bool AudioSenderImpl::start_capture()
{
    if (@available(macOS 13.0, *)) {
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block NSError* start_error = nil;
        [m_stream startCaptureWithCompletionHandler:^(NSError* error) {
            start_error = error;
            dispatch_semaphore_signal(semaphore);
        }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        if (start_error != nil) {
            std::cerr << "audio: could not start ScreenCaptureKit: "
                      << start_error.localizedDescription.UTF8String << "\n";
            return false;
        }
        m_capture_started = true;
        return true;
    }
    return false;
}

void AudioSenderImpl::stop_capture()
{
    if (m_stream == nil) {
        return;
    }

    if (@available(macOS 13.0, *)) {
        if (!m_capture_started) {
            m_stream = nil;
            m_output = nil;
            m_capture_queue = nil;
            return;
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        [m_stream stopCaptureWithCompletionHandler:^(NSError*) {
            dispatch_semaphore_signal(semaphore);
        }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }
    m_capture_started = false;
    m_stream = nil;
    m_output = nil;
    m_capture_queue = nil;
}

bool AudioSenderImpl::start()
{
    if (m_running.load()) {
        return true;
    }

    std::string error;
    if (!m_config.is_valid(&error)) {
        std::cerr << "audio: invalid configuration: " << error << "\n";
        return false;
    }
    if (aoo_initialize(nullptr) != kAooOk) {
        std::cerr << "audio: could not initialize AOO\n";
        return false;
    }
    m_aoo_initialized = true;
    if (!setup_network() || !setup_capture()) {
        stop();
        return false;
    }

    m_running.store(true);
    m_send_thread = std::thread(&AudioSenderImpl::network_send_loop, this);
    m_receive_thread = std::thread(&AudioSenderImpl::network_receive_loop, this);
    m_source->startStream(0, nullptr);
    if (!start_capture()) {
        stop();
        return false;
    }

    std::cout << "audio: capturing macOS system audio on UDP "
              << m_config.media_port << "\n";
    return true;
}

void AudioSenderImpl::stop()
{
    const bool was_running = m_running.exchange(false);
    if (was_running && m_source) {
        m_source->stopStream(0);
    }
    stop_capture();

    if (m_client) {
        m_client->stop();
    }
    if (m_send_thread.joinable()) {
        m_send_thread.join();
    }
    if (m_receive_thread.joinable()) {
        m_receive_thread.join();
    }
    m_source.reset();
    m_client.reset();
    if (m_aoo_initialized) {
        aoo_terminate();
        m_aoo_initialized = false;
    }
}

void AudioSenderImpl::network_send_loop()
{
    while (m_running.load()) {
        m_client->send(0.1);
    }
}

void AudioSenderImpl::network_receive_loop()
{
    while (m_running.load()) {
        m_client->receive(0.1);
    }
}

void AudioSenderImpl::handle_stream_error(NSError* error)
{
    m_capture_failed.store(true);
    if (error != nil) {
        std::cerr << "audio: ScreenCaptureKit stopped: "
                  << error.localizedDescription.UTF8String << "\n";
    }
}

void AudioSenderImpl::handle_sample(CMSampleBufferRef sample_buffer)
{
    if (!m_running.load() || m_capture_failed.load() ||
        !CMSampleBufferIsValid(sample_buffer)) {
        return;
    }

    const auto frame_count = CMSampleBufferGetNumSamples(sample_buffer);
    const auto channel_count = m_config.format.channels;
    if (frame_count == 0 || frame_count > kMaxCaptureFrames ||
        channel_count == 0 || channel_count > kMaxChannels) {
        return;
    }

    auto* format_description = CMSampleBufferGetFormatDescription(sample_buffer);
    auto* asbd = format_description != nullptr
        ? CMAudioFormatDescriptionGetStreamBasicDescription(format_description)
        : nullptr;
    if (asbd == nullptr || asbd->mBitsPerChannel != 32) {
        return;
    }

    std::array<std::byte, sizeof(AudioBufferList) +
                          (kMaxChannels - 1) * sizeof(AudioBuffer)> storage{};
    auto* audio_buffers = reinterpret_cast<AudioBufferList*>(storage.data());
    audio_buffers->mNumberBuffers = kMaxChannels;
    CMBlockBufferRef block_buffer = nullptr;
    const auto buffer_size = static_cast<size_t>(storage.size());
    if (CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
            sample_buffer, nullptr, audio_buffers, buffer_size,
            kCFAllocatorDefault, kCFAllocatorDefault, 0, &block_buffer) != noErr) {
        return;
    }

    const bool non_interleaved =
        (asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
    const bool is_float = (asbd->mFormatFlags & kAudioFormatFlagIsFloat) != 0;
    const auto bytes_per_sample = asbd->mBitsPerChannel / 8;
    const auto available_channels =
        non_interleaved ? audio_buffers->mNumberBuffers
                        : audio_buffers->mBuffers[0].mNumberChannels;

    for (std::size_t channel = 0; channel < channel_count; ++channel) {
        m_channel_pointers[channel] =
            m_channel_buffer.data() + channel * kMaxCaptureFrames;
        for (std::size_t frame = 0; frame < frame_count; ++frame) {
            float value = 0.0f;
            if (channel < available_channels) {
                const auto buffer_index = non_interleaved ? channel : 0;
                const auto& buffer = audio_buffers->mBuffers[buffer_index];
                const auto* bytes = static_cast<const std::uint8_t*>(buffer.mData);
                const auto sample_index = non_interleaved
                    ? frame
                    : frame * available_channels + channel;
                if (bytes != nullptr &&
                    (sample_index + 1) * bytes_per_sample <= buffer.mDataByteSize) {
                    if (is_float) {
                        std::memcpy(&value, bytes + sample_index * bytes_per_sample,
                                    sizeof(value));
                    } else {
                        std::int32_t integer_value = 0;
                        std::memcpy(&integer_value,
                                    bytes + sample_index * bytes_per_sample,
                                    sizeof(integer_value));
                        value = static_cast<float>(integer_value) /
                                static_cast<float>(INT32_MAX);
                    }
                }
            }
            m_channel_pointers[channel][frame] = value;
        }
    }

    m_source->process(m_channel_pointers.data(),
                      static_cast<AooInt32>(frame_count),
                      aoo_getCurrentNtpTime());
    m_client->notify();
    if (block_buffer != nullptr) {
        CFRelease(block_buffer);
    }
}

AudioSender::AudioSender(const AudioRelayConfig& config, AooId source_id)
    : m_impl(std::make_unique<AudioSenderImpl>(config, source_id))
{
}

AudioSender::~AudioSender() = default;

bool AudioSender::start()
{
    return m_impl->start();
}

void AudioSender::stop()
{
    m_impl->stop();
}

bool AudioSender::is_running() const
{
    return m_impl->is_running();
}

} // namespace audio
} // namespace inputleap
