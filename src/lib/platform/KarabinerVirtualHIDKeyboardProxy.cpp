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

#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

namespace inputleap {

class KarabinerVirtualHIDKeyboard::Impl {
public:
    ~Impl()
    {
        m_stopping = true;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            closeSocket();
        }
        if (m_connectThread.joinable()) {
            m_connectThread.join();
        }
    }

    void start()
    {
        const char* socketPath = std::getenv("INPUTLEAP_VHID_HELPER_SOCKET");
        if (socketPath == nullptr || *socketPath == '\0') {
            LOG_DEBUG1("Karabiner Virtual HID helper socket is not configured");
            return;
        }

        m_socketPath = socketPath;
        m_connectThread = std::thread([this] { connectLoop(); });
    }

    bool isReady() const
    {
        return m_ready;
    }

    bool postKey(std::uint8_t virtualKeyCode, bool down)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_socketFd < 0 || !m_ready) {
            return false;
        }

        const std::uint8_t packet[2] = {
            virtualKeyCode, static_cast<std::uint8_t>(down ? 1 : 0)};
        const ssize_t sent = send(m_socketFd, packet, sizeof(packet), MSG_DONTWAIT);
        if (sent != static_cast<ssize_t>(sizeof(packet))) {
            closeSocket();
            return false;
        }
        return true;
    }

private:
    void connectLoop()
    {
        while (!m_stopping) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_socketFd < 0) {
                    const int socketFd = socket(AF_UNIX, SOCK_DGRAM, 0);
                    if (socketFd >= 0) {
                        sockaddr_un address{};
                        address.sun_family = AF_UNIX;
                        if (m_socketPath.size() < sizeof(address.sun_path)) {
                            std::strncpy(address.sun_path, m_socketPath.c_str(),
                                         sizeof(address.sun_path) - 1);
                            if (connect(socketFd,
                                        reinterpret_cast<const sockaddr*>(&address),
                                        sizeof(address)) == 0) {
                                m_socketFd = socketFd;
                                m_ready = true;
                                LOG_INFO("Karabiner Virtual HID proxy ready=yes");
                            }
                            else {
                                close(socketFd);
                            }
                        }
                        else {
                            close(socketFd);
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void closeSocket()
    {
        if (m_socketFd >= 0) {
            close(m_socketFd);
            m_socketFd = -1;
        }
        m_ready = false;
    }

    std::string m_socketPath;
    int m_socketFd = -1;
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_stopping{false};
    std::mutex m_mutex;
    std::thread m_connectThread;
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
