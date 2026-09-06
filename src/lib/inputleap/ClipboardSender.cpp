// SPDX-License-Identifier: GPL-2.0-or-later
#include "inputleap/ClipboardSender.h"
#include "base/IEventQueue.h"
#include "base/EventQueueTimer.h"
#include <utility>

namespace inputleap {
ClipboardSender::ClipboardSender(IEventQueue* events, Writer writer) :
    events_(events), writer_(std::move(writer))
{
    events_->add_handler(EventType::TIMER, this, [this](const auto&) { tick(); });
}

ClipboardSender::~ClipboardSender()
{
    cancel();
    events_->remove_handler(EventType::TIMER, this);
}

void ClipboardSender::cancel()
{
    if (timer_) {
        events_->deleteTimer(timer_);
        timer_ = nullptr;
    }
    active_.reset();
    pending_.clear();
}

void ClipboardSender::send(std::string data, ClipboardID id, std::uint32_t sequence)
{
    if (id >= kClipboardEnd) {
        return;
    }
    // Keep only the latest queued snapshot per clipboard. An already-started
    // transfer must finish: old peers do not support a cancellation message.
    for (auto& transfer : pending_) {
        if (transfer.id == id) {
            transfer = Transfer{std::move(data), id, sequence};
            return;
        }
    }
    pending_.push_back(Transfer{std::move(data), id, sequence});
    schedule();
}

void ClipboardSender::schedule()
{
    if (!timer_ && (active_ || !pending_.empty())) {
        // At most one 32 KiB chunk per 10 ms (~3.1 MiB/s before overhead).
        timer_ = events_->newOneShotTimer(0.01, this);
    }
}

void ClipboardSender::tick()
{
    if (!timer_) {
        return;
    }
    events_->deleteTimer(timer_);
    timer_ = nullptr;
    if (!active_ && !pending_.empty()) {
        active_ = std::make_unique<Transfer>(std::move(pending_.front()));
        pending_.pop_front();
    }
    if (!active_) {
        return;
    }
    ClipboardChunk chunk;
    auto& transfer = *active_;
    if (!transfer.started) {
        chunk = ClipboardChunk::start(transfer.id, transfer.sequence, transfer.data.size());
        transfer.started = true;
    } else if (transfer.offset < transfer.data.size()) {
        chunk = ClipboardChunk::data(transfer.id, transfer.sequence,
                                    transfer.data.substr(transfer.offset, 32 * 1024));
        transfer.offset += chunk.data_.size();
    } else {
        chunk = ClipboardChunk::end(transfer.id, transfer.sequence);
        active_.reset();
    }
    writer_(chunk);
    schedule();
}
} // namespace inputleap
