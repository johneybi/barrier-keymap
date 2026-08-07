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

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void print_usage()
{
    std::cout
        << "usage: input-leap-audiod --mode receive --source HOST [options]\n"
        << "  --source HOST       AOO source host\n"
        << "  --source-port PORT  AOO source UDP port (default: 24801)\n"
        << "  --media-port PORT   local UDP port (default: 24801)\n"
        << "  --source-id ID      AOO source ID (default: 1)\n"
        << "  --latency MS        jitter buffer latency (default: 60)\n"
        << "  --help              show this help\n";
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
    if (end == value || *end != '\0' || parsed < 0) {
        return false;
    }
    output = static_cast<AooId>(parsed);
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    std::string mode;
    std::string source;
    std::uint16_t source_port = inputleap::audio::kDefaultMediaPort;
    std::uint16_t media_port = inputleap::audio::kDefaultMediaPort;
    std::uint16_t latency = inputleap::audio::kDefaultJitterBufferMs;
    AooId source_id = 1;

    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--help") {
            print_usage();
            return EXIT_SUCCESS;
        }
        if (index + 1 >= argc) {
            std::cerr << "missing value for " << option << "\n";
            return EXIT_FAILURE;
        }
        const char* value = argv[++index];
        if (option == "--mode") {
            mode = value;
        } else if (option == "--source") {
            source = value;
        } else if (option == "--source-port") {
            if (!parse_uint16(value, source_port)) {
                std::cerr << "invalid source port\n";
                return EXIT_FAILURE;
            }
        } else if (option == "--media-port") {
            if (!parse_uint16(value, media_port)) {
                std::cerr << "invalid media port\n";
                return EXIT_FAILURE;
            }
        } else if (option == "--source-id") {
            if (!parse_id(value, source_id)) {
                std::cerr << "invalid source id\n";
                return EXIT_FAILURE;
            }
        } else if (option == "--latency") {
            if (!parse_uint16(value, latency)) {
                std::cerr << "invalid latency\n";
                return EXIT_FAILURE;
            }
        } else {
            std::cerr << "unknown option: " << option << "\n";
            return EXIT_FAILURE;
        }
    }

    if (mode != "receive") {
        std::cerr << "only --mode receive is implemented in this milestone\n";
        return EXIT_FAILURE;
    }

    inputleap::audio::AudioRelayConfig config;
    config.media_port = media_port;
    config.jitter_buffer_ms = latency;

    inputleap::audio::AudioReceiver receiver(
        config, source.c_str(), source_port, source_id);
    if (!receiver.start()) {
        return EXIT_FAILURE;
    }

    std::cout << "audio receiver running on UDP " << media_port
              << "; press Enter to stop\n";
    std::string line;
    std::getline(std::cin, line);
    receiver.stop();
    return EXIT_SUCCESS;
}
