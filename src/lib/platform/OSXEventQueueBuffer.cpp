/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2004 Chris Schoeneman
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

#include "platform/OSXEventQueueBuffer.h"

#include "base/Event.h"
#include "base/IEventQueue.h"

//
// EventQueueTimer
//

class EventQueueTimer { };

//
// OSXEventQueueBuffer
//

static const double kUserEventPollInterval = 0.004;

OSXEventQueueBuffer::OSXEventQueueBuffer(IEventQueue* events) :
    m_event(NULL),
    m_eventQueue(events),
    m_carbonEventQueue(NULL),
    m_wakeupPending(false)
{
    // do nothing
}

OSXEventQueueBuffer::~OSXEventQueueBuffer()
{
    // release the last event
    if (m_event != NULL) {
        ReleaseEvent(m_event);
    }
}

void
OSXEventQueueBuffer::init()
{
    m_carbonEventQueue = GetCurrentEventQueue();
}

void
OSXEventQueueBuffer::waitForEvent(double timeout)
{
    if (hasUserEvent()) {
        return;
    }

    // PostEventToQueue can leave cross-thread Syne events pending for hundreds
    // of milliseconds on current macOS releases.  Bound the Carbon wait so
    // the user-event FIFO is checked often enough for interactive input.
    if (timeout < 0.0 || timeout > kUserEventPollInterval) {
        timeout = kUserEventPollInterval;
    }

    EventRef event;
    ReceiveNextEvent(0, NULL, timeout, false, &event);
}

IEventQueueBuffer::Type
OSXEventQueueBuffer::getEvent(Event& event, UInt32& dataID)
{
    // release the previous event
    if (m_event != NULL) {
        ReleaseEvent(m_event);
        m_event = NULL;
    }

    // Barrier events have their own FIFO.  Check it before the Carbon queue so
    // socket readiness cannot sit behind a stream of synthetic system events.
    if (popUserEvent(dataID)) {
        return kUser;
    }

    // get the next event
    OSStatus error = ReceiveNextEvent(0, NULL, 0.0, true, &m_event);

    // handle the event
    if (error == eventLoopQuitErr) {
        event = Event(Event::kQuit);
        return kSystem;
    }
    else if (error != noErr) {
        return kNone;
    }
    else {
        UInt32 eventClass = GetEventClass(m_event);
        switch (eventClass) {
        case 'Syne':
            clearWakeupPending();
            // Syne is only a cross-thread wake-up.  The event ID is kept in
            // m_userEvents so Barrier events can bypass Carbon queue ordering.
            if (popUserEvent(dataID)) {
                return kUser;
            }
            return kNone;

        default:
            event = Event(Event::kSystem,
                        m_eventQueue->getSystemTarget(), &m_event);
            return kSystem;
        }
    }
}

bool
OSXEventQueueBuffer::addEvent(UInt32 dataID)
{
    bool postWakeup = false;
    {
        std::lock_guard<std::mutex> lock(m_userEventMutex);
        const bool wasEmpty = m_userEvents.empty();
        m_userEvents.push_back(dataID);
        if (wasEmpty && !m_wakeupPending) {
            m_wakeupPending = true;
            postWakeup = true;
        }
    }

    // The FIFO is polled at a bounded interval, so one outstanding Carbon
    // event is enough to wake the loop without accumulating stale Syne events.
    if (!postWakeup) {
        return true;
    }

    EventRef event;
    OSStatus error = CreateEvent(
                            kCFAllocatorDefault,
                            'Syne',
                            dataID,
                            0,
                            kEventAttributeNone,
                            &event);

    if (error == noErr) {
        assert(m_carbonEventQueue != NULL);

        error = PostEventToQueue(
            m_carbonEventQueue,
            event,
            kEventPriorityStandard);

        ReleaseEvent(event);
    }

    if (error != noErr) {
        clearWakeupPending();
    }

    // The user event is already stored in the FIFO.  A failed Carbon wake is
    // recovered by the four millisecond poll in waitForEvent().
    return true;
}

bool
OSXEventQueueBuffer::isEmpty() const
{
    if (hasUserEvent()) {
        return false;
    }

    EventRef event;
    OSStatus status = ReceiveNextEvent(0, NULL, 0.0, false, &event);
    return (status == eventLoopTimedOutErr);
}

bool
OSXEventQueueBuffer::popUserEvent(UInt32& dataID)
{
    std::lock_guard<std::mutex> lock(m_userEventMutex);
    if (m_userEvents.empty()) {
        return false;
    }

    dataID = m_userEvents.front();
    m_userEvents.pop_front();
    return true;
}

bool
OSXEventQueueBuffer::hasUserEvent() const
{
    std::lock_guard<std::mutex> lock(m_userEventMutex);
    return !m_userEvents.empty();
}

void
OSXEventQueueBuffer::clearWakeupPending()
{
    std::lock_guard<std::mutex> lock(m_userEventMutex);
    m_wakeupPending = false;
}

EventQueueTimer*
OSXEventQueueBuffer::newTimer(double, bool) const
{
    return new EventQueueTimer;
}

void
OSXEventQueueBuffer::deleteTimer(EventQueueTimer* timer) const
{
    delete timer;
}
