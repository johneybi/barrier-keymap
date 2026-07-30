/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Barrier Keymap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <stdint.h>
#include <string>
#include <sys/types.h>

namespace barrier {
namespace virtual_hid_bridge {

static const uint32_t kMagic = 0x42564844u; // "BVHD"
static const uint16_t kVersion = 1;
static const uint16_t kKeyboardReport = 1;
static const size_t kMaxKeys = 32;

enum Modifier {
    kLeftControl = 1u << 0,
    kLeftShift = 1u << 1,
    kLeftOption = 1u << 2,
    kLeftCommand = 1u << 3,
    kRightControl = 1u << 4,
    kRightShift = 1u << 5,
    kRightOption = 1u << 6,
    kRightCommand = 1u << 7
};

struct KeyboardReport {
    uint32_t m_magic;
    uint16_t m_version;
    uint16_t m_type;
    uint8_t m_modifiers;
    uint8_t m_keyCount;
    uint8_t m_reserved[2];
    uint16_t m_keys[kMaxKeys];
};

inline void
initialize(KeyboardReport& report)
{
    report.m_magic = kMagic;
    report.m_version = kVersion;
    report.m_type = kKeyboardReport;
    report.m_modifiers = 0;
    report.m_keyCount = 0;
    report.m_reserved[0] = 0;
    report.m_reserved[1] = 0;
    for (size_t i = 0; i < kMaxKeys; ++i) {
        report.m_keys[i] = 0;
    }
}

inline bool
isValid(const KeyboardReport& report)
{
    return report.m_magic == kMagic &&
        report.m_version == kVersion &&
        report.m_type == kKeyboardReport &&
        report.m_keyCount <= kMaxKeys;
}

inline std::string
socketPath(uid_t uid)
{
    return "/var/run/barrier-keymap-vhid-" +
        std::to_string(static_cast<unsigned long long>(uid)) + ".sock";
}

} // namespace virtual_hid_bridge
} // namespace barrier
