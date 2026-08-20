/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 ESK
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include <cstdint>
#include <memory>

namespace inputleap {

class KarabinerVirtualHIDKeyboard {
public:
    KarabinerVirtualHIDKeyboard();
    ~KarabinerVirtualHIDKeyboard();

    KarabinerVirtualHIDKeyboard(const KarabinerVirtualHIDKeyboard&) = delete;
    KarabinerVirtualHIDKeyboard& operator=(const KarabinerVirtualHIDKeyboard&) = delete;

    void start();
    bool isReady() const;
    bool postKey(std::uint8_t virtualKeyCode, bool down);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace inputleap
