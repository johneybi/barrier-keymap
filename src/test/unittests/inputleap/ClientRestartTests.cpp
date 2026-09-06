#include "inputleap/ClientApp.h"
#include "base/EventQueue.h"
#include "base/EventQueueTimer.h"
#include <gtest/gtest.h>

namespace inputleap {
namespace {

// Count real ClientApp scheduling calls without opening a screen or socket.
class RetryQueue : public EventQueue {
public:
    int scheduled = 0;
    int cancelled = 0;
    double delay = 0;
    EventQueueTimer timer;

    EventQueueTimer* newOneShotTimer(double duration, const EventTarget*) override
    {
        ++scheduled;
        delay = duration;
        return &timer;
    }
    void deleteTimer(EventQueueTimer* value) override
    {
        EXPECT_EQ(&timer, value);
        ++cancelled;
    }
};

TEST(ClientRestartTests, DuplicateFailureKeepsOneTimerAndOneBackoffStep)
{
    RetryQueue queue;
    ClientApp app(&queue, nullptr);
    app.scheduleClientRestart(app.nextRestartTimeout());
    app.scheduleClientRestart(app.nextRestartTimeout());
    EXPECT_EQ(1, queue.scheduled);
    EXPECT_DOUBLE_EQ(1.0, queue.delay);
    EXPECT_DOUBLE_EQ(2.0, app.nextRestartTimeout());
    app.stopClient();
    EXPECT_EQ(1, queue.cancelled);
    app.stopClient();
    EXPECT_EQ(1, queue.cancelled);
}

TEST(ClientRestartTests, SuccessfulConnectionCancelsPendingRetryAndResetsDelay)
{
    RetryQueue queue;
    ClientApp app(&queue, nullptr);
    app.scheduleClientRestart(app.nextRestartTimeout());
    app.handle_client_connected();
    EXPECT_EQ(1, queue.cancelled);
    EXPECT_DOUBLE_EQ(1.0, app.nextRestartTimeout());
    app.scheduleClientRestart(app.nextRestartTimeout());
    EXPECT_EQ(2, queue.scheduled);
    EXPECT_DOUBLE_EQ(1.0, queue.delay);
    app.stopClient();
}

TEST(ClientRestartTests, RepeatedOutagesBackOffToThirtySeconds)
{
    RetryQueue queue;
    ClientApp app(&queue, nullptr);
    for (double expected : {1.0, 2.0, 4.0, 8.0, 16.0, 30.0, 30.0}) {
        app.scheduleClientRestart(app.nextRestartTimeout());
        EXPECT_DOUBLE_EQ(expected, queue.delay);
        app.cancelClientRestart();
    }
}

} // namespace
} // namespace inputleap
