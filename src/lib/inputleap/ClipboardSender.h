// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "base/EventTarget.h"
#include "inputleap/ClipboardChunk.h"
#include <deque>
#include <functional>
#include <memory>

namespace inputleap {
class IEventQueue;
class EventQueueTimer;

// One sender per connection. Never interleave START/DATA/END transfers.
// Pacing bounds event-loop work, not the socket's output buffer size.
class ClipboardSender : public EventTarget {
public:
    using Writer = std::function<void(const ClipboardChunk&)>;
    ClipboardSender(IEventQueue* events, Writer writer);
    ~ClipboardSender();
    ClipboardSender(const ClipboardSender&) = delete;
    ClipboardSender& operator=(const ClipboardSender&) = delete;
    void send(std::string data, ClipboardID id, std::uint32_t sequence);
    void cancel();

private:
    struct Transfer {
        std::string data;
        ClipboardID id;
        std::uint32_t sequence;
        std::size_t offset = 0;
        bool started = false;
    };
    void schedule();
    void tick();
    IEventQueue* events_;
    Writer writer_;
    EventQueueTimer* timer_ = nullptr;
    std::unique_ptr<Transfer> active_;
    std::deque<Transfer> pending_;
};
} // namespace inputleap
