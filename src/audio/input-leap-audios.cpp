/*
 * InputLeap -- macOS system audio sender command
 * Copyright (C) 2026 The InputLeap Keymap Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "AudioSender.h"
#include "AudioProcessWait.h"
#include "MacAudioCapture.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void print_usage()
{
    std::cout
        << "usage: input-leap-audios [options]\n"
        << "  --list-audio-devices       list CoreAudio devices and exit\n"
        << "  --capture-mode MODE        device (default) or screen\n"
        << "  --audio-device-uid UID     CoreAudio device UID for device mode\n"
        << "  --media-port PORT  local AOO UDP port (default: 24801)\n"
        << "  --source-id ID     source ID (default: 1)\n"
        << "  --help             show this help\n";
}

bool parse_uint16(const char* value, std::uint16_t& output)
{
    char* end = nullptr;
    const auto parsed = std::strtoul(value, &end, 10);
    if (end == value || *end != '\0' || parsed > 65535) {
        return false;
    }
    output = static_cast<std::uint16_t>(parsed);
    return true;
}

bool parse_id(const char* value, AooId& output)
{
    char* end = nullptr;
    const auto parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 0 || parsed > INT32_MAX) {
        return false;
    }
    output = static_cast<AooId>(parsed);
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    std::uint16_t media_port = inputleap::audio::kDefaultMediaPort;
    AooId source_id = 1;
    inputleap::audio::AudioCaptureOptions capture_options;

    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--help") {
            print_usage();
            return EXIT_SUCCESS;
        }
        if (option == "--list-audio-devices") {
            std::string error;
            const auto devices = inputleap::audio::list_mac_audio_devices(&error);
            if (!error.empty()) {
                std::cerr << "audio: " << error << "\n";
                return EXIT_FAILURE;
            }
            for (const auto& device : devices) {
                std::cout << "id=" << device.id
                          << " name=\"" << device.name << "\""
                          << " uid=\"" << device.uid << "\""
                          << " input-channels=" << device.input_channels
                          << " output-channels=" << device.output_channels
                          << " sample-rate=" << std::fixed
                          << std::setprecision(0) << device.sample_rate << "\n";
            }
            return EXIT_SUCCESS;
        }
        if (index + 1 >= argc) {
            std::cerr << "missing value for " << option << "\n";
            return EXIT_FAILURE;
        }
        const char* value = argv[++index];
        if (option == "--media-port") {
            if (!parse_uint16(value, media_port)) {
                std::cerr << "invalid media port\n";
                return EXIT_FAILURE;
            }
        } else if (option == "--source-id") {
            if (!parse_id(value, source_id)) {
                std::cerr << "invalid source id\n";
                return EXIT_FAILURE;
            }
        } else if (option == "--capture-mode") {
            if (std::string(value) == "device") {
                capture_options.mode = inputleap::audio::AudioCaptureMode::Device;
            } else if (std::string(value) == "screen") {
                capture_options.mode = inputleap::audio::AudioCaptureMode::Screen;
            } else {
                std::cerr << "capture mode must be device or screen\n";
                return EXIT_FAILURE;
            }
        } else if (option == "--audio-device-uid") {
            capture_options.device_uid = value;
        } else {
            std::cerr << "unknown option: " << option << "\n";
            return EXIT_FAILURE;
        }
    }

    if (capture_options.mode == inputleap::audio::AudioCaptureMode::Device &&
        capture_options.device_uid.empty()) {
        std::cerr << "audio: --audio-device-uid is required in device mode\n";
        return EXIT_FAILURE;
    }

    inputleap::audio::AudioRelayConfig config;
    config.media_port = media_port;

    inputleap::audio::AudioSender sender(config, source_id, capture_options);
    if (!sender.start()) {
        return EXIT_FAILURE;
    }

    std::cout << "audio sender running; press Ctrl+C to stop\n";
    inputleap::audio::wait_for_audio_shutdown();
    sender.stop();
    return EXIT_SUCCESS;
}
