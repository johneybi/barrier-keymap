/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 ESK
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "platform/KarabinerVirtualHIDKeyboard.h"

#include "base/Log.h"

#include <Carbon/Carbon.h>

#include <pqrs/dispatcher.hpp>
#include <pqrs/hid/usage.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <set>
#include <thread>

namespace inputleap {

class KarabinerVirtualHIDKeyboard::Impl {
public:
    using Client = pqrs::karabiner::driverkit::virtual_hid_device_service::client;
    using Parameters = pqrs::karabiner::driverkit::virtual_hid_device_service::virtual_hid_keyboard_parameters;
    using Report = pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::keyboard_input;
    using Modifier = pqrs::karabiner::driverkit::virtual_hid_device_driver::hid_report::modifier;

    ~Impl()
    {
        if (m_client != nullptr) {
            m_client->async_virtual_hid_keyboard_terminate();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            m_client->async_stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            m_client.reset();
        }
        if (m_dispatcherStarted) {
            pqrs::dispatcher::extra::terminate_shared_dispatcher();
        }
    }

    void start()
    {
        if (m_dispatcherStarted) {
            return;
        }

        pqrs::dispatcher::extra::initialize_shared_dispatcher();
        m_dispatcherStarted = true;
        m_client = std::make_unique<Client>();

        m_client->connected.connect([this] {
            Parameters parameters;
            parameters.set_country_code(pqrs::hid::country_code::us);
            m_client->async_virtual_hid_keyboard_initialize(parameters);
        });
        m_client->connect_failed.connect([](const auto& error) {
            LOG_DEBUG1("Karabiner Virtual HID connection failed: %s",
                       error.message().c_str());
        });
        m_client->error_occurred.connect([](const auto& error) {
            LOG_DEBUG1("Karabiner Virtual HID error: %s",
                       error.message().c_str());
        });
        m_client->virtual_hid_keyboard_ready.connect([this](bool ready) {
            m_ready = ready;
            LOG_INFO("Karabiner Virtual HID keyboard ready=%s",
                     ready ? "yes" : "no");
        });
        m_client->async_start();
    }

    bool isReady() const
    {
        return m_ready;
    }

    bool postKey(std::uint8_t virtualKeyCode, bool down)
    {
        if (!m_ready || m_client == nullptr) {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto modifier = modifierForVirtualKey(virtualKeyCode); modifier.has_value()) {
            if (down) {
                m_modifiers.insert(*modifier);
            }
            else {
                m_modifiers.erase(*modifier);
            }
        }
        else if (auto usage = usageForVirtualKey(virtualKeyCode); usage.has_value()) {
            if (down) {
                m_keys.insert(*usage);
            }
            else {
                m_keys.erase(*usage);
            }
        }
        else {
            return false;
        }

        Report report;
        for (auto modifier : m_modifiers) {
            report.modifiers.insert(modifier);
        }
        for (auto usage : m_keys) {
            report.keys.insert(usage);
        }
        m_client->async_post_report(report);
        return true;
    }

private:
    static std::optional<Modifier> modifierForVirtualKey(std::uint8_t key)
    {
        switch (key) {
        case kVK_Control: return Modifier::left_control;
        case kVK_RightControl: return Modifier::right_control;
        case kVK_Shift: return Modifier::left_shift;
        case kVK_RightShift: return Modifier::right_shift;
        case kVK_Option: return Modifier::left_option;
        case kVK_RightOption: return Modifier::right_option;
        case kVK_Command: return Modifier::left_command;
        case kVK_RightCommand: return Modifier::right_command;
        default: return std::nullopt;
        }
    }

    static std::optional<std::uint16_t> usageForVirtualKey(std::uint8_t key)
    {
        using namespace pqrs::hid::usage::keyboard_or_keypad;
        switch (key) {
        case kVK_ANSI_A: return type_safe::get(keyboard_a);
        case kVK_ANSI_B: return type_safe::get(keyboard_b);
        case kVK_ANSI_C: return type_safe::get(keyboard_c);
        case kVK_ANSI_D: return type_safe::get(keyboard_d);
        case kVK_ANSI_E: return type_safe::get(keyboard_e);
        case kVK_ANSI_F: return type_safe::get(keyboard_f);
        case kVK_ANSI_G: return type_safe::get(keyboard_g);
        case kVK_ANSI_H: return type_safe::get(keyboard_h);
        case kVK_ANSI_I: return type_safe::get(keyboard_i);
        case kVK_ANSI_J: return type_safe::get(keyboard_j);
        case kVK_ANSI_K: return type_safe::get(keyboard_k);
        case kVK_ANSI_L: return type_safe::get(keyboard_l);
        case kVK_ANSI_M: return type_safe::get(keyboard_m);
        case kVK_ANSI_N: return type_safe::get(keyboard_n);
        case kVK_ANSI_O: return type_safe::get(keyboard_o);
        case kVK_ANSI_P: return type_safe::get(keyboard_p);
        case kVK_ANSI_Q: return type_safe::get(keyboard_q);
        case kVK_ANSI_R: return type_safe::get(keyboard_r);
        case kVK_ANSI_S: return type_safe::get(keyboard_s);
        case kVK_ANSI_T: return type_safe::get(keyboard_t);
        case kVK_ANSI_U: return type_safe::get(keyboard_u);
        case kVK_ANSI_V: return type_safe::get(keyboard_v);
        case kVK_ANSI_W: return type_safe::get(keyboard_w);
        case kVK_ANSI_X: return type_safe::get(keyboard_x);
        case kVK_ANSI_Y: return type_safe::get(keyboard_y);
        case kVK_ANSI_Z: return type_safe::get(keyboard_z);
        case kVK_ANSI_0: return type_safe::get(keyboard_0);
        case kVK_ANSI_1: return type_safe::get(keyboard_1);
        case kVK_ANSI_2: return type_safe::get(keyboard_2);
        case kVK_ANSI_3: return type_safe::get(keyboard_3);
        case kVK_ANSI_4: return type_safe::get(keyboard_4);
        case kVK_ANSI_5: return type_safe::get(keyboard_5);
        case kVK_ANSI_6: return type_safe::get(keyboard_6);
        case kVK_ANSI_7: return type_safe::get(keyboard_7);
        case kVK_ANSI_8: return type_safe::get(keyboard_8);
        case kVK_ANSI_9: return type_safe::get(keyboard_9);
        case kVK_ANSI_Minus: return type_safe::get(keyboard_hyphen);
        case kVK_ANSI_Equal: return type_safe::get(keyboard_equal_sign);
        case kVK_ANSI_LeftBracket: return type_safe::get(keyboard_open_bracket);
        case kVK_ANSI_RightBracket: return type_safe::get(keyboard_close_bracket);
        case kVK_ANSI_Backslash: return type_safe::get(keyboard_backslash);
        case kVK_ANSI_Semicolon: return type_safe::get(keyboard_semicolon);
        case kVK_ANSI_Quote: return type_safe::get(keyboard_quote);
        case kVK_ANSI_Comma: return type_safe::get(keyboard_comma);
        case kVK_ANSI_Period: return type_safe::get(keyboard_period);
        case kVK_ANSI_Slash: return type_safe::get(keyboard_slash);
        case kVK_ANSI_Grave: return type_safe::get(keyboard_grave_accent_and_tilde);
        case kVK_Space: return type_safe::get(keyboard_spacebar);
        case kVK_Return: return type_safe::get(keyboard_return_or_enter);
        case kVK_Tab: return type_safe::get(keyboard_tab);
        case kVK_Delete: return type_safe::get(keyboard_delete_or_backspace);
        case kVK_Escape: return type_safe::get(keyboard_escape);
        case kVK_Help: return type_safe::get(keyboard_help);
        case kVK_ForwardDelete: return type_safe::get(keyboard_delete_forward);
        case kVK_Home: return type_safe::get(keyboard_home);
        case kVK_End: return type_safe::get(keyboard_end);
        case kVK_PageUp: return type_safe::get(keyboard_page_up);
        case kVK_PageDown: return type_safe::get(keyboard_page_down);
        case kVK_LeftArrow: return type_safe::get(keyboard_left_arrow);
        case kVK_RightArrow: return type_safe::get(keyboard_right_arrow);
        case kVK_UpArrow: return type_safe::get(keyboard_up_arrow);
        case kVK_DownArrow: return type_safe::get(keyboard_down_arrow);
        case kVK_CapsLock: return type_safe::get(keyboard_caps_lock);
        case kVK_ANSI_Keypad0: return type_safe::get(keypad_0);
        case kVK_ANSI_Keypad1: return type_safe::get(keypad_1);
        case kVK_ANSI_Keypad2: return type_safe::get(keypad_2);
        case kVK_ANSI_Keypad3: return type_safe::get(keypad_3);
        case kVK_ANSI_Keypad4: return type_safe::get(keypad_4);
        case kVK_ANSI_Keypad5: return type_safe::get(keypad_5);
        case kVK_ANSI_Keypad6: return type_safe::get(keypad_6);
        case kVK_ANSI_Keypad7: return type_safe::get(keypad_7);
        case kVK_ANSI_Keypad8: return type_safe::get(keypad_8);
        case kVK_ANSI_Keypad9: return type_safe::get(keypad_9);
        case kVK_ANSI_KeypadDecimal: return type_safe::get(keypad_period);
        case kVK_ANSI_KeypadEquals: return type_safe::get(keypad_equal_sign);
        case kVK_ANSI_KeypadMultiply: return type_safe::get(keypad_asterisk);
        case kVK_ANSI_KeypadPlus: return type_safe::get(keypad_plus);
        case kVK_ANSI_KeypadMinus: return type_safe::get(keypad_hyphen);
        case kVK_ANSI_KeypadDivide: return type_safe::get(keypad_slash);
        case kVK_ANSI_KeypadEnter: return type_safe::get(keypad_enter);
        case kVK_ANSI_KeypadClear: return type_safe::get(keypad_num_lock);
        case kVK_F1: return type_safe::get(keyboard_f1);
        case kVK_F2: return type_safe::get(keyboard_f2);
        case kVK_F3: return type_safe::get(keyboard_f3);
        case kVK_F4: return type_safe::get(keyboard_f4);
        case kVK_F5: return type_safe::get(keyboard_f5);
        case kVK_F6: return type_safe::get(keyboard_f6);
        case kVK_F7: return type_safe::get(keyboard_f7);
        case kVK_F8: return type_safe::get(keyboard_f8);
        case kVK_F9: return type_safe::get(keyboard_f9);
        case kVK_F10: return type_safe::get(keyboard_f10);
        case kVK_F11: return type_safe::get(keyboard_f11);
        case kVK_F12: return type_safe::get(keyboard_f12);
        case kVK_F13: return type_safe::get(keyboard_f13);
        case kVK_F14: return type_safe::get(keyboard_f14);
        case kVK_F15: return type_safe::get(keyboard_f15);
        case kVK_F16: return type_safe::get(keyboard_f16);
        case kVK_F17: return type_safe::get(keyboard_f17);
        case kVK_F18: return type_safe::get(keyboard_f18);
        case kVK_F19: return type_safe::get(keyboard_f19);
        case kVK_F20: return type_safe::get(keyboard_f20);
        default: return std::nullopt;
        }
    }

    std::unique_ptr<Client> m_client;
    std::set<std::uint16_t> m_keys;
    std::set<Modifier> m_modifiers;
    std::mutex m_mutex;
    std::atomic<bool> m_ready{false};
    bool m_dispatcherStarted = false;
};

KarabinerVirtualHIDKeyboard::KarabinerVirtualHIDKeyboard() :
    m_impl(std::make_unique<Impl>())
{
}

KarabinerVirtualHIDKeyboard::~KarabinerVirtualHIDKeyboard() = default;

void KarabinerVirtualHIDKeyboard::start()
{
    m_impl->start();
}

bool KarabinerVirtualHIDKeyboard::isReady() const
{
    return m_impl->isReady();
}

bool KarabinerVirtualHIDKeyboard::postKey(std::uint8_t virtualKeyCode, bool down)
{
    return m_impl->postKey(virtualKeyCode, down);
}

} // namespace inputleap
