// SPDX-License-Identifier: GPL-2.0-or-later
#include "inputleap/ClipboardSender.h"
#include "inputleap/protocol_types.h"
#include "base/EventQueue.h"
#include "base/EventQueueTimer.h"
#include <gtest/gtest.h>
#include <vector>

namespace inputleap {
namespace {
class ClipboardQueue : public EventQueue {
public:
    EventQueueTimer timer;
    const EventTarget* target = nullptr;
    bool pending = false;
    EventQueueTimer* newOneShotTimer(double duration, const EventTarget* value) override
    {
        EXPECT_FALSE(pending);
        EXPECT_DOUBLE_EQ(0.01, duration);
        target = value;
        pending = true;
        return &timer;
    }
    void deleteTimer(EventQueueTimer* value) override
    {
        EXPECT_EQ(&timer, value);
        EXPECT_TRUE(pending);
        pending = false;
    }
    void fire() { dispatchEvent(Event(EventType::TIMER, target)); }
};

TEST(ClipboardSenderTests, LargeBinarySnapshotEmitsOnlyOneChunkPerTick)
{
    ClipboardQueue queue;
    std::vector<ClipboardChunk> chunks;
    ClipboardSender sender(&queue, [&](const auto& chunk) { chunks.push_back(chunk); });
    std::string data(100000, '\0');
    data[40000] = 'x';
    sender.send(data, 0, 42);
    EXPECT_TRUE(chunks.empty());
    for (unsigned i = 0; i < 6; ++i) {
        ASSERT_TRUE(queue.pending);
        queue.fire();
        EXPECT_EQ(i + 1, chunks.size());
    }
    EXPECT_FALSE(queue.pending);
    ASSERT_EQ(6u, chunks.size());
    EXPECT_EQ(kDataStart, chunks.front().mark_);
    EXPECT_EQ("100000", chunks.front().data_);
    EXPECT_EQ(kDataEnd, chunks.back().mark_);
    std::string received;
    for (unsigned i = 1; i < 5; ++i) {
        EXPECT_EQ(kDataChunk, chunks[i].mark_);
        EXPECT_EQ(42u, chunks[i].sequence_);
        EXPECT_LE(chunks[i].data_.size(), 32768u);
        received += chunks[i].data_;
    }
    EXPECT_EQ(data, received);
}

TEST(ClipboardSenderTests, ActiveTransferFinishesAndPendingSnapshotsCoalesce)
{
    ClipboardQueue queue;
    std::vector<ClipboardChunk> chunks;
    ClipboardSender sender(&queue, [&](const auto& chunk) { chunks.push_back(chunk); });
    sender.send("first", 0, 1);
    queue.fire(); // START: must not replace this transfer halfway through.
    sender.send("obsolete", 0, 2);
    sender.send("selection", 1, 3);
    sender.send("latest", 0, 4);
    for (unsigned i = 0; queue.pending && i < 20; ++i) {
        queue.fire();
    }
    EXPECT_FALSE(queue.pending);
    ASSERT_EQ(9u, chunks.size());
    EXPECT_EQ("first", chunks[1].data_);
    EXPECT_EQ(kDataEnd, chunks[2].mark_);
    EXPECT_EQ(4u, chunks[3].sequence_);
    EXPECT_EQ("latest", chunks[4].data_);
    EXPECT_EQ(kDataEnd, chunks[5].mark_);
    EXPECT_EQ(1u, chunks[6].id_);
    EXPECT_EQ("selection", chunks[7].data_);
    EXPECT_EQ(kDataEnd, chunks[8].mark_);
}

TEST(ClipboardSenderTests, CancelAndDestructionDiscardPendingWork)
{
    ClipboardQueue queue;
    unsigned writes = 0;
    {
        ClipboardSender sender(&queue, [&](const auto&) { ++writes; });
        sender.send("cancelled", 0, 1);
        sender.cancel();
        EXPECT_FALSE(queue.pending);
        queue.fire(); // A late event after cancellation must not send data.
        EXPECT_EQ(0u, writes);
        sender.send("destroyed", 0, 2);
    }
    EXPECT_FALSE(queue.pending);
    EXPECT_EQ(0u, writes);
}

TEST(ClipboardSenderTests, EmptySnapshotAndInvalidId)
{
    ClipboardQueue queue;
    std::vector<ClipboardChunk> chunks;
    ClipboardSender sender(&queue, [&](const auto& chunk) { chunks.push_back(chunk); });
    sender.send("invalid", kClipboardEnd, 1);
    EXPECT_FALSE(queue.pending);
    sender.send("", 0, 2);
    queue.fire();
    queue.fire();
    EXPECT_FALSE(queue.pending);
    ASSERT_EQ(2u, chunks.size());
    EXPECT_EQ(kDataStart, chunks[0].mark_);
    EXPECT_EQ("0", chunks[0].data_);
    EXPECT_EQ(kDataEnd, chunks[1].mark_);
}
} // namespace
} // namespace inputleap
