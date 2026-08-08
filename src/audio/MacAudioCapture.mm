/*
 * InputLeap -- macOS CoreAudio device capture
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "MacAudioCapture.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <utility>

namespace inputleap {
namespace audio {
namespace {

bool get_property(AudioObjectID object,
                  AudioObjectPropertyScope scope,
                  AudioObjectPropertySelector selector,
                  void* data,
                  UInt32* size)
{
    AudioObjectPropertyAddress address{
        selector, scope, kAudioObjectPropertyElementMain};
    return AudioObjectGetPropertyData(object, &address, 0, nullptr, size, data) ==
           noErr;
}

std::string get_string_property(AudioObjectID object,
                                AudioObjectPropertySelector selector)
{
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    if (!get_property(object, kAudioObjectPropertyScopeGlobal, selector,
                      &value, &size) || value == nullptr) {
        return {};
    }

    char buffer[512]{};
    const auto result = CFStringGetCString(
        value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(value);
    return result ? std::string(buffer) : std::string();
}

std::uint32_t get_channel_count(AudioDeviceID device,
                                AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress address{
        kAudioDevicePropertyStreamConfiguration,
        scope,
        kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &address, 0, nullptr, &size) !=
        noErr) {
        return 0;
    }

    std::vector<std::byte> storage(size);
    auto* list = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, list) !=
        noErr) {
        return 0;
    }

    std::uint32_t channels = 0;
    for (UInt32 index = 0; index < list->mNumberBuffers; ++index) {
        channels += list->mBuffers[index].mNumberChannels;
    }
    return channels;
}

double get_sample_rate(AudioDeviceID device)
{
    Float64 rate = 0.0;
    UInt32 size = sizeof(rate);
    if (!get_property(device, kAudioObjectPropertyScopeGlobal,
                      kAudioDevicePropertyNominalSampleRate, &rate, &size)) {
        return 0.0;
    }
    return rate;
}

} // namespace

class MacAudioCapture::Impl {
public:
    Impl(const AudioRelayConfig& config,
         const std::string& uid,
         MacAudioFrameCallback callback,
         void* user)
        : m_config(config),
          m_device_uid(uid),
          m_callback(callback),
          m_user(user)
    {
    }

    ~Impl()
    {
        stop();
    }

    bool start(std::string* error);
    void stop();
    bool is_running() const { return m_running; }

private:
    static OSStatus input_callback(void* user,
                                   AudioUnitRenderActionFlags* flags,
                                   const AudioTimeStamp* timestamp,
                                   UInt32 bus,
                                   UInt32 frames,
                                   AudioBufferList*)
    {
        return static_cast<Impl*>(user)->render(
            flags, timestamp, bus, frames);
    }

    OSStatus render(AudioUnitRenderActionFlags* flags,
                    const AudioTimeStamp* timestamp,
                    UInt32 bus,
                    UInt32 frames);
    bool fail(std::string* error, const std::string& message) const;

    AudioRelayConfig m_config;
    std::string m_device_uid;
    MacAudioFrameCallback m_callback = nullptr;
    void* m_user = nullptr;
    AudioUnit m_audio_unit = nullptr;
    AudioDeviceID m_device = kAudioObjectUnknown;
    bool m_running = false;
    std::array<float, kMaxAudioCallbackFrames * 8> m_capture_buffer{};
    AudioBufferList m_buffer_list{};
};

bool MacAudioCapture::Impl::fail(std::string* error,
                                 const std::string& message) const
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool MacAudioCapture::Impl::start(std::string* error)
{
    if (m_running) {
        return true;
    }
    if (m_device_uid.empty()) {
        return fail(error, "an audio device UID is required");
    }
    if (m_config.format.channels == 0 || m_config.format.channels > 8) {
        return fail(error, "unsupported audio channel count");
    }

    std::string devices_error;
    const auto devices = list_mac_audio_devices(&devices_error);
    if (!devices_error.empty()) {
        return fail(error, devices_error);
    }
    const auto device = std::find_if(
        devices.begin(), devices.end(),
        [this](const MacAudioDeviceInfo& candidate) {
            return candidate.uid == m_device_uid;
        });
    if (device == devices.end()) {
        return fail(error, "audio device UID was not found: " + m_device_uid);
    }
    if (device->input_channels < m_config.format.channels) {
        return fail(error, "selected audio device has fewer input channels than required");
    }
    if (std::abs(device->sample_rate -
                 static_cast<double>(m_config.format.sample_rate)) > 0.5) {
        return fail(error, "selected audio device must run at 48000 Hz");
    }
    m_device = static_cast<AudioDeviceID>(device->id);
    std::cerr << "audio: selected CoreAudio device name=\""
              << device->name << "\" uid=\"" << device->uid
              << "\" input-channels=" << device->input_channels
              << " sample-rate=" << device->sample_rate << "\n";

    AudioComponentDescription description{
        kAudioUnitType_Output,
        kAudioUnitSubType_HALOutput,
        kAudioUnitManufacturer_Apple,
        0,
        0};
    const auto component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr) {
        return fail(error, "HAL output audio component is unavailable");
    }
    if (AudioComponentInstanceNew(component, &m_audio_unit) != noErr) {
        return fail(error, "could not create HAL output audio unit");
    }

    UInt32 enable = 1;
    if (AudioUnitSetProperty(m_audio_unit,
                             kAudioOutputUnitProperty_EnableIO,
                             kAudioUnitScope_Input,
                             1,
                             &enable,
                             sizeof(enable)) != noErr) {
        return fail(error, "could not enable CoreAudio input bus");
    }
    enable = 0;
    if (AudioUnitSetProperty(m_audio_unit,
                             kAudioOutputUnitProperty_EnableIO,
                             kAudioUnitScope_Output,
                             0,
                             &enable,
                             sizeof(enable)) != noErr) {
        return fail(error, "could not disable CoreAudio output bus");
    }
    if (AudioUnitSetProperty(m_audio_unit,
                             kAudioOutputUnitProperty_CurrentDevice,
                             kAudioUnitScope_Global,
                             0,
                             &m_device,
                             sizeof(m_device)) != noErr) {
        return fail(error, "could not select CoreAudio device");
    }

    AudioStreamBasicDescription format{};
    format.mSampleRate = m_config.format.sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = m_config.format.channels;
    format.mBitsPerChannel = 32;
    format.mBytesPerFrame = sizeof(float) * format.mChannelsPerFrame;
    format.mBytesPerPacket = format.mBytesPerFrame;
    if (AudioUnitSetProperty(m_audio_unit,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output,
                             1,
                             &format,
                             sizeof(format)) != noErr) {
        return fail(error, "CoreAudio device does not accept 48 kHz float input");
    }

    AURenderCallbackStruct callback{input_callback, this};
    if (AudioUnitSetProperty(m_audio_unit,
                             kAudioOutputUnitProperty_SetInputCallback,
                             kAudioUnitScope_Global,
                             1,
                             &callback,
                             sizeof(callback)) != noErr) {
        return fail(error, "could not install CoreAudio input callback");
    }

    if (AudioUnitInitialize(m_audio_unit) != noErr) {
        return fail(error, "could not initialize CoreAudio audio unit");
    }
    if (AudioOutputUnitStart(m_audio_unit) != noErr) {
        return fail(error, "could not start CoreAudio audio unit");
    }
    m_running = true;
    return true;
}

OSStatus MacAudioCapture::Impl::render(AudioUnitRenderActionFlags* flags,
                                       const AudioTimeStamp* timestamp,
                                       UInt32 bus,
                                       UInt32 frames)
{
    if (!m_running || m_callback == nullptr || frames == 0 ||
        frames > kMaxAudioCallbackFrames) {
        return kAudio_ParamError;
    }

    m_buffer_list.mNumberBuffers = 1;
    m_buffer_list.mBuffers[0].mNumberChannels = m_config.format.channels;
    m_buffer_list.mBuffers[0].mDataByteSize =
        frames * m_config.format.channels * sizeof(float);
    m_buffer_list.mBuffers[0].mData = m_capture_buffer.data();
    const auto status = AudioUnitRender(m_audio_unit,
                                        flags,
                                        timestamp,
                                        bus,
                                        frames,
                                        &m_buffer_list);
    if (status == noErr) {
        m_callback(m_user,
                   m_capture_buffer.data(),
                   frames,
                   m_config.format.channels);
    }
    return status;
}

void MacAudioCapture::Impl::stop()
{
    if (m_audio_unit == nullptr) {
        return;
    }
    if (m_running) {
        AudioOutputUnitStop(m_audio_unit);
        m_running = false;
    }
    AudioUnitUninitialize(m_audio_unit);
    AudioComponentInstanceDispose(m_audio_unit);
    m_audio_unit = nullptr;
}

std::vector<MacAudioDeviceInfo> list_mac_audio_devices(std::string* error)
{
    AudioObjectPropertyAddress address{
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(
            kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr) {
        if (error != nullptr) {
            *error = "could not query CoreAudio devices";
        }
        return {};
    }

    std::vector<AudioDeviceID> ids(size / sizeof(AudioDeviceID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                   &address,
                                   0,
                                   nullptr,
                                   &size,
                                   ids.data()) != noErr) {
        if (error != nullptr) {
            *error = "could not read CoreAudio devices";
        }
        return {};
    }

    std::vector<MacAudioDeviceInfo> devices;
    devices.reserve(ids.size());
    for (const auto id : ids) {
        MacAudioDeviceInfo device;
        device.id = id;
        device.name = get_string_property(id, kAudioObjectPropertyName);
        device.uid = get_string_property(id, kAudioDevicePropertyDeviceUID);
        device.input_channels = get_channel_count(
            id, kAudioObjectPropertyScopeInput);
        device.output_channels = get_channel_count(
            id, kAudioObjectPropertyScopeOutput);
        device.sample_rate = get_sample_rate(id);
        devices.push_back(std::move(device));
    }
    return devices;
}

MacAudioCapture::MacAudioCapture(const AudioRelayConfig& config,
                                 const std::string& device_uid,
                                 MacAudioFrameCallback callback,
                                 void* user)
    : m_impl(std::make_unique<Impl>(config, device_uid, callback, user))
{
}

MacAudioCapture::~MacAudioCapture() = default;

bool MacAudioCapture::start(std::string* error)
{
    return m_impl->start(error);
}

void MacAudioCapture::stop()
{
    m_impl->stop();
}

bool MacAudioCapture::is_running() const
{
    return m_impl->is_running();
}

} // namespace audio
} // namespace inputleap
