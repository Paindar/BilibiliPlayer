#include "audio_event_processor.h"
#include <log/log_manager.h>
#include <QDateTime>
#include <queue>
#include "audio_event_processor.h"
#include <log/log_manager.h>

namespace audio {

AudioEventProcessor::AudioEventProcessor(QObject* parent)
    : QObject(parent)
{
    LOG_DEBUG(" AudioEventProcessor created");
}

AudioEventProcessor::~AudioEventProcessor()
{
    stop();
    LOG_DEBUG(" AudioEventProcessor destroyed");
}

void AudioEventProcessor::postEvent(PlayerEventType eventType, const QVariantHash& eventData)
{
    std::unique_lock<std::mutex> locker(m_queueMutex);

    m_eventQueue.push_back(std::make_unique<PlaybackEvent>(eventType, eventData));
    LOG_DEBUG(" Posted event: {} (queue size: {})",
                magic_enum::enum_name(eventType).data(), m_eventQueue.size());
    LOG_DEBUG(" Scheduled processing after posting event, queue size: {}", m_eventQueue.size());
    m_queueCondition.notify_one();
    locker.unlock();
}
    
void AudioEventProcessor::postHighPriorityEvent(PlayerEventType eventType, const QVariantHash& eventData)
{
    std::unique_lock<std::mutex> locker(m_queueMutex);

    m_highPriorityQueue.push_back(std::make_unique<PlaybackEvent>(eventType, eventData));
    LOG_DEBUG(" Posted high-priority event: {} (high-priority queue size: {})",
                magic_enum::enum_name(eventType).data(), m_highPriorityQueue.size());
    LOG_DEBUG(" Scheduled processing after posting high-priority event, queue size: {}", m_highPriorityQueue.size());
    m_queueCondition.notify_one();
    locker.unlock();
}

void AudioEventProcessor::setEventHandler(PlayerEventType type, std::function<void(const QVariantHash&)> handler)
{
    m_eventHandlers[type] = handler;
    LOG_DEBUG(" Event handler registered, event type: {}", magic_enum::enum_name(type).data());
}

int AudioEventProcessor::pendingEventCount() const
{
    std::scoped_lock locker(m_queueMutex);
    return m_eventQueue.size() + m_highPriorityQueue.size();
}

void AudioEventProcessor::start()
{
    if (m_processingThread)
        return;
    m_active.store(true);
    m_processingThread = std::make_unique<std::thread>(&AudioEventProcessor::processEventFunc, this);
    LOG_INFO(" AudioEventProcessor started");
}

void AudioEventProcessor::stop()
{
    m_active.store(false);
    m_queueCondition.notify_all();
    if (m_processingThread && m_processingThread->joinable()) {
        m_processingThread->join();
        m_processingThread.reset();
    }
    // Clear pending events
    std::scoped_lock locker(m_queueMutex);
    int clearedEvents = m_eventQueue.size() + m_highPriorityQueue.size();
    m_eventQueue.clear();
    m_highPriorityQueue.clear();
    
    LOG_INFO(" AudioEventProcessor stopped, cleared {} pending events", clearedEvents);
}

void AudioEventProcessor::processEventUnsafe(std::unique_ptr<PlaybackEvent> event)
{
    if (!event) return;
    
    auto handlerIt = m_eventHandlers.find(event->getType());
    if (handlerIt != m_eventHandlers.end()) {
        try {
            handlerIt->second(event->getData());
            LOG_DEBUG("Processed event: {}", event->toString().toStdString());
        } catch (const std::exception& e) {
            LOG_ERROR("Error in event handler: {}", e.what());
            emit processingError(QString::fromStdString(e.what()));
        }
    } else {
        LOG_WARN(" No handler for event type: {}", static_cast<int>(event->getType()));
    }
}

void AudioEventProcessor::processEventFunc()
{
    try {
        // Process high-priority events first
        while (m_active.load()) {
            std::queue<std::unique_ptr<PlaybackEvent>> eventsToProcess;
            while (m_active.load()) {
                std::unique_lock<std::mutex> locker(m_queueMutex);
                while (!m_highPriorityQueue.empty()) {
                    eventsToProcess.push(std::move(m_highPriorityQueue.front()));
                    m_highPriorityQueue.pop_front();
                }
                while (!m_eventQueue.empty()) {
                    eventsToProcess.push(std::move(m_eventQueue.front()));
                    m_eventQueue.pop_front();
                }
                if (eventsToProcess.empty()) {
                    m_queueCondition.wait(locker);
                } else {
                    break;
                }
            }
            if (!m_active.load()) break;
            while (!eventsToProcess.empty() && m_active.load()) {
                auto event = std::move(eventsToProcess.front());
                eventsToProcess.pop();
                processEventUnsafe(std::move(event));
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing events: {}", e.what());
        emit processingError(QString::fromStdString(e.what()));
    }
    LOG_DEBUG(" Event processing thread exiting");
}

} // namespace audio