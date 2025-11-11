#pragma once

#include <QObject>
#include <deque>
#include <thread>
#include <atomic>
#include <functional>
#include <QHash>
#include <memory>
#include <QVariantHash>
#include <condition_variable>
#include <mutex>
// magic_enum provides compile-time enum to string conversion (header-only)
#include <magic_enum/magic_enum.hpp>

namespace audio {

/**
 * AudioEventProcessor
 * Centralized event processing for audio player operations
 * Serializes all commands through Qt's event loop to prevent deadlocks
 */

class AudioEventProcessor : public QObject
{
    Q_OBJECT
public:
    enum PlayerEventType {
        // Playback Control Events
        PLAY,
        PAUSE,
        STOP,
        PLAYBACK_COMPLETE,
        PLAYBACK_ERROR,
        
        // Error Handling Events
        STREAM_READ_COMPLETED,
        STREAM_READ_ERROR,
        DECODING_FINISHED,
        DECODING_ERROR,
        FRAME_TRANSMISSION_COMPLETED,
        FRAME_TRANSMISSION_ERROR,
        
        // State Change Events
        // SetVolume,
        // SetPlayMode,
        // UpdatePosition,
    };
public:
    explicit AudioEventProcessor(QObject* parent = nullptr);
    ~AudioEventProcessor();
    
    // Event queue management
    void postEvent(PlayerEventType eventType, const QVariantHash& eventData = {});
    
    // High-priority events (processed immediately)
    void postHighPriorityEvent(PlayerEventType eventType, const QVariantHash& eventData = {});

    // Event handler registration
    void setEventHandler(PlayerEventType type, std::function<void(const QVariantHash&)> handler);
    
    // Queue status
    int pendingEventCount() const;
    
    // Control
    void start();
    void stop();
private:
    class PlaybackEvent;
    void processEventUnsafe(std::unique_ptr<PlaybackEvent> event);
    void processEventFunc();
signals:
    void processingError(const QString& error);
    
private:
    mutable std::condition_variable m_queueCondition;
    mutable std::mutex m_queueMutex;
    std::deque<std::unique_ptr<PlaybackEvent>> m_eventQueue;
    std::deque<std::unique_ptr<PlaybackEvent>> m_highPriorityQueue;
    
    std::unique_ptr<std::thread> m_processingThread;
    std::unordered_map<PlayerEventType, std::function<void(const QVariantHash&)>> m_eventHandlers;
    
    std::atomic<bool> m_active{true};
};

class AudioEventProcessor::PlaybackEvent 
{
public:
    // Disable default constructor, copy constructor, and assignment operator
    PlaybackEvent() = delete;
    PlaybackEvent(const PlaybackEvent&) = delete;
    PlaybackEvent& operator=(const PlaybackEvent&) = delete;

    explicit PlaybackEvent(PlayerEventType eventType, const QVariantHash& eventData = {})
        : type(eventType), data(eventData) {}
    virtual ~PlaybackEvent() = default;
    PlayerEventType getType() const { return type; }
    QVariantHash getData() const { return data; }
    virtual QString toString() const {
        // Use magic_enum to get the enum identifier name (e.g. "PLAY", "PAUSE")
        auto name = magic_enum::enum_name(type);
        QString name_q = QString::fromUtf8(name.data(), static_cast<int>(name.size()));
        return QString("PlaybackEvent(Type=%1, DataKeys=[%2])")
            .arg(name_q)
            .arg(data.keys().join(", "));
    }
private:
    PlayerEventType type;
    QVariantHash data;
};

} // namespace audio