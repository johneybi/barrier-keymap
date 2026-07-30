/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2026 Barrier Keymap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "platform/OSXVirtualHIDBridge.h"
#include "test/global/gtest.h"

TEST(OSXVirtualHIDBridgeTests, initializedReportIsValid)
{
    barrier::virtual_hid_bridge::KeyboardReport report;
    barrier::virtual_hid_bridge::initialize(report);

    EXPECT_TRUE(barrier::virtual_hid_bridge::isValid(report));
    EXPECT_EQ(0u, report.m_modifiers);
    EXPECT_EQ(0u, report.m_keyCount);
}

TEST(OSXVirtualHIDBridgeTests, rejectsProtocolVersionMismatch)
{
    barrier::virtual_hid_bridge::KeyboardReport report;
    barrier::virtual_hid_bridge::initialize(report);
    ++report.m_version;

    EXPECT_FALSE(barrier::virtual_hid_bridge::isValid(report));
}

TEST(OSXVirtualHIDBridgeTests, rejectsTooManyKeys)
{
    barrier::virtual_hid_bridge::KeyboardReport report;
    barrier::virtual_hid_bridge::initialize(report);
    report.m_keyCount = barrier::virtual_hid_bridge::kMaxKeys + 1;

    EXPECT_FALSE(barrier::virtual_hid_bridge::isValid(report));
}
