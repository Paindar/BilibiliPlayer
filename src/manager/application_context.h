#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <functional>

// Forward declarations
class PlaylistManager;
class ConfigManager;
class ThemeManager;
namespace audio { class AudioPlayerController; }
class EventBus;

// Network namespace forward declaration
namespace network {
    class NetworkManager;
}

/**
 * Central application context that holds all global managers
 * Provides controlled access to shared services with proper initialization order
 */
class ApplicationContext : public QObject
{
    Q_OBJECT
    
public:
    static ApplicationContext& instance();
    
    // Initialize all managers with proper dependency order (call once in main.cpp or MainWindow)
    void initialize(const QString& workspaceDir = "");
    
    // Step-by-step initialization for complex scenarios
    void initializePhase1(const QString& workspaceDir); // Core managers (Config)
    void initializePhase2(); // Network-dependent managers
    void initializePhase3(); // Data managers (Playlist, etc.)
    
    // Manager access (returns nullptr if not yet initialized)
    PlaylistManager* playlistManager() const { return m_playlistManager.get(); }
    ConfigManager* configManager() const { return m_configManager.get(); }
    ThemeManager* themeManager() const { return m_themeManager.get(); }
    audio::AudioPlayerController* audioPlayerController() const { return m_audioPlayerController.get(); }
    network::NetworkManager* networkManager() const { return m_networkManager.get(); }
    EventBus* eventBus() const { return m_eventBus.get(); }
    
    // Check initialization status
    bool isInitialized() const { return m_initialized; }
    bool isPhaseInitialized(int phase) const;
    
    // Cleanup
    void shutdown();
    void gracefulShutdown(int timeoutMs = 5000); // Shutdown with timeout for safety
    
signals:
    void managersInitialized();
    void phaseInitialized(int phase);
    void configLoaded();
    void networkReady();
    void dataManagersReady();
    void shutdownStarted();
    void shutdownPhaseCompleted(int phase);
    void shutdownCompleted();
    
private:
    explicit ApplicationContext(QObject* parent = nullptr);
    ~ApplicationContext();
    Q_DISABLE_COPY(ApplicationContext)
    
    // Phase-based initialization
    void initializeConfigManager(const QString& workspaceDir);
    void initializeEventBus();
    void initializeThemeManager();
    void initializeNetworkManager();
    void initializePlaylistManager();
    void initializeAudioPlayerController();
    
    // Cross-manager connections
    void setupManagerConnections();
    
    // Smart pointers for automatic cleanup
    std::unique_ptr<ConfigManager> m_configManager;
    std::shared_ptr<EventBus> m_eventBus;
    std::unique_ptr<ThemeManager> m_themeManager;
    std::unique_ptr<PlaylistManager> m_playlistManager;
    std::unique_ptr<audio::AudioPlayerController> m_audioPlayerController;
    std::shared_ptr<network::NetworkManager> m_networkManager;
    
    bool m_initialized = false;
    int m_currentPhase = 0;
};

// Convenience macros for easy access
#define APP_CONTEXT ApplicationContext::instance()
#define PLAYLIST_MANAGER APP_CONTEXT.playlistManager()
#define CONFIG_MANAGER APP_CONTEXT.configManager()
#define THEME_MANAGER APP_CONTEXT.themeManager()
#define AUDIO_PLAYER_CONTROLLER APP_CONTEXT.audioPlayerController()
#define NETWORK_MANAGER APP_CONTEXT.networkManager()
#define EVENT_BUS APP_CONTEXT.eventBus()