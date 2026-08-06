/*
 * InputLeap -- mouse and keyboard sharing utility
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

#include <algorithm>

namespace inputleap {

static const double kUserEventPollInterval = 0.004;

OSXEventQueueBuffer::OSXEventQueueBuffer(IEventQueue* events) :
    m_event(nullptr),
    m_eventQueue(events),
    m_carbonEventQueue(nullptr)
{
    // do nothing
}

OSXEventQueueBuffer::~OSXEventQueueBuffer()
{
    // release the last event
    if (m_event != nullptr) {
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

    // Carbon cross-thread wake events can remain pending for hundreds of
    // milliseconds on current macOS releases.
    if (timeout < 0.0 || timeout > kUserEventPollInterval) {
        timeout = kUserEventPollInterval;
    }

    EventRef event;
    ReceiveNextEvent(0, nullptr, timeout, false, &event);
}

IEventQueueBuffer::Type OSXEventQueueBuffer::getEvent(Event& event, std::uint32_t& dataID)
{
    // release the previous event
    if (m_event != nullptr) {
        ReleaseEvent(m_event);
        m_event = nullptr;
    }

    if (popUserEvent(dataID)) {
        return kUser;
    }

    // get the next event
    OSStatus error = ReceiveNextEvent(0, nullptr, 0.0, true, &m_event);

    // handle the event
    if (error == eventLoopQuitErr) {
        event = Event(EventType::QUIT);
        return kSystem;
    }
    else if (error != noErr) {
        return kNone;
    }
    else {
        std::uint32_t eventClass = GetEventClass(m_event);
        switch (eventClass) {
        case 'Syne':
            // The Carbon event only wakes the loop. The FIFO preserves the
            // actual event IDs without relying on Carbon queue ordering.
            if (popUserEvent(dataID)) {
                return kUser;
            }
            return kNone;

        default:
            event = Event(EventType::SYSTEM, m_eventQueue->getSystemTarget(),
                          create_event_data<EventRef*>(&m_event));
            return kSystem;
        }
    }
}

bool OSXEventQueueBuffer::addEvent(std::uint32_t dataID)
{
    EventRef event;
    OSStatus error = CreateEvent(
                            kCFAllocatorDefault,
                            'Syne',
                            dataID,
                            0,
                            kEventAttributeNone,
                            &event);

    if (error == noErr) {
        {
            std::lock_guard<std::mutex> lock(m_userEventMutex);
            m_userEvents.push_back(dataID);
        }

        assert(m_carbonEventQueue != nullptr);

        error = PostEventToQueue(
            m_carbonEventQueue,
            event,
            kEventPriorityStandard);

        ReleaseEvent(event);

        if (error != noErr && !removeUserEvent(dataID)) {
            // Another wake-up delivered the queued event concurrently.
            error = noErr;
        }
    }

    return (error == noErr);
}

bool
OSXEventQueueBuffer::isEmpty() const
{
    if (hasUserEvent()) {
        return false;
    }

    EventRef event;
    OSStatus status = ReceiveNextEvent(0, nullptr, 0.0, false, &event);
    return (status == eventLoopTimedOutErr);
}

bool OSXEventQueueBuffer::popUserEvent(std::uint32_t& dataID)
{
    std::lock_guard<std::mutex> lock(m_userEventMutex);
    if (m_userEvents.empty()) {
        return false;
    }

    dataID = m_userEvents.front();
    m_userEvents.pop_front();
    return true;
}

bool OSXEventQueueBuffer::removeUserEvent(std::uint32_t dataID)
{
    std::lock_guard<std::mutex> lock(m_userEventMutex);
    const auto event = std::find(m_userEvents.begin(), m_userEvents.end(), dataID);
    if (event == m_userEvents.end()) {
        return false;
    }

    m_userEvents.erase(event);
    return true;
}

bool OSXEventQueueBuffer::hasUserEvent() const
{
    std::lock_guard<std::mutex> lock(m_userEventMutex);
    return !m_userEvents.empty();
}

} // namespace inputleap
