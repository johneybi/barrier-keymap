/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2011 Nick Bolton
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "test/mock/inputleap/MockKeyMap.h"
#include "test/mock/inputleap/MockEventQueue.h"
#include "platform/OSXKeyState.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace inputleap {

namespace {
class InertOSXKeyState : public OSXKeyState {
public:
    InertOSXKeyState(IEventQueue* queue, KeyMap& map) : OSXKeyState(queue, map) {}
    using OSXKeyState::postHIDVirtualKey;
    CGEventFlags deliveredFlags = 0;
    int deliveries = 0;
protected:
    void postKeyboardEvent(std::uint8_t, bool, CGEventFlags flags) override {
        deliveredFlags = flags;
        ++deliveries;
    }
};
}

TEST(OSXKeyStateTests, overlappingModifierSidesRemainActiveUntilBothReleased)
{
    struct Pair { std::uint8_t left, right; CGEventFlags flag; };
    for (const auto& pair : {
            Pair{kVK_Shift, kVK_RightShift, kCGEventFlagMaskShift},
            Pair{kVK_Control, kVK_RightControl, kCGEventFlagMaskControl},
            Pair{kVK_Option, kVK_RightOption, kCGEventFlagMaskAlternate},
            Pair{kVK_Command, kVK_RightCommand, kCGEventFlagMaskCommand}}) {
        for (bool releaseLeftFirst : {false, true}) {
            KeyMap map;
            MockEventQueue queue;
            InertOSXKeyState keys(&queue, map);
            keys.postHIDVirtualKey(pair.left, true);
            keys.postHIDVirtualKey(pair.right, true);
            keys.postHIDVirtualKey(releaseLeftFirst ? pair.left : pair.right, false);
            EXPECT_EQ(pair.flag, keys.deliveredFlags);
            keys.postHIDVirtualKey(kVK_ANSI_A, true);
            EXPECT_EQ(pair.flag, keys.deliveredFlags);
            keys.postHIDVirtualKey(kVK_ANSI_A, false);
            keys.postHIDVirtualKey(releaseLeftFirst ? pair.right : pair.left, false);
            EXPECT_EQ(0u, keys.deliveredFlags);
            EXPECT_EQ(6, keys.deliveries);
        }
    }
}

TEST(OSXKeyStateTests, duplicateModifierDownAndUnmatchedOppositeUpDoNotClearHeldKey)
{
    KeyMap map;
    MockEventQueue queue;
    InertOSXKeyState keys(&queue, map);
    keys.postHIDVirtualKey(kVK_RightOption, true);
    keys.postHIDVirtualKey(kVK_RightOption, true);
    keys.postHIDVirtualKey(kVK_Option, false);
    EXPECT_EQ(kCGEventFlagMaskAlternate, keys.deliveredFlags);
    keys.postHIDVirtualKey(kVK_RightOption, false);
    EXPECT_EQ(0u, keys.deliveredFlags);
}

TEST(OSXKeyStateTests, mapModifiersFromOSX_OSXMask)
{
    inputleap::KeyMap keyMap;
    MockEventQueue eventQueue;
    OSXKeyState keyState(&eventQueue, keyMap);

    KeyModifierMask outMask = 0;

    std::uint32_t shiftMask = 0 | kCGEventFlagMaskShift;
    outMask = keyState.mapModifiersFromOSX(shiftMask);
    EXPECT_EQ(KeyModifierShift, outMask);

    std::uint32_t ctrlMask = 0 | kCGEventFlagMaskControl;
    outMask = keyState.mapModifiersFromOSX(ctrlMask);
    EXPECT_EQ(KeyModifierControl, outMask);

    std::uint32_t altMask = 0 | kCGEventFlagMaskAlternate;
    outMask = keyState.mapModifiersFromOSX(altMask);
    EXPECT_EQ(KeyModifierAlt, outMask);

    std::uint32_t cmdMask = 0 | kCGEventFlagMaskCommand;
    outMask = keyState.mapModifiersFromOSX(cmdMask);
    EXPECT_EQ(KeyModifierSuper, outMask);

    std::uint32_t capsMask = 0 | kCGEventFlagMaskAlphaShift;
    outMask = keyState.mapModifiersFromOSX(capsMask);
    EXPECT_EQ(KeyModifierCapsLock, outMask);

    std::uint32_t numMask = 0 | kCGEventFlagMaskNumericPad;
    outMask = keyState.mapModifiersFromOSX(numMask);
    EXPECT_EQ(KeyModifierNumLock, outMask);
}

} // namespace inputleap
