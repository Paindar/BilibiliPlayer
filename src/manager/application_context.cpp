#include "application_context.h"
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include <chrono>

#include <playlist/playlist_manager.h>
#include <config/config_manager.h>
#include <ui/theme/theme_manager.h>
#include <network/network_manager.h>
#include <log/log_manager.h>
#include <audio/audio_player_controller.h>
#include <event/event_bus.hpp>

ApplicationContext& ApplicationContext::instance()
{
    static ApplicationContext instance;
    return instance;
}

ApplicationContext::ApplicationContext(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_currentPhase(0)
{
}

ApplicationContext::~ApplicationContext() = default;

void ApplicationContext::initialize(const QString& workspaceDir)
{
    if (m_initialized) {
        LOG_WARN("ApplicationContext already initialized");
        return;
    }
    
    // Determine workspace directory
    QString workspace = workspaceDir;
    if (workspace.isEmpty()) {
        // Use directory where executable is located
        workspace = QCoreApplication::applicationDirPath();
    }
    
    // Initialize logging first
    QDir wsDir(workspace);
    QString logDir = wsDir.absoluteFilePath("log");
    LogManager::instance().initialize(logDir.toStdString());
    
    LOG_INFO("Initializing ApplicationContext with workspace: {}", workspace.toStdString());
    LOG_INFO("Dependency-aware initialization sequence starting...");
    
    try {
        // Initialize in phases to handle dependencies
        initializePhase1(workspace); // Core managers first
        initializePhase2();          // Network and dependent managers
        initializePhase3();          // Data managers last
        
        setupManagerConnections();
        
        m_initialized = true;
        emit managersInitialized();
        
        LOG_INFO("ApplicationContext initialized successfully");
        
    } catch (const std::exception& e) {
        LOG_CRITICAL("Failed to initialize ApplicationContext: {}", e.what());
        throw;
    }
}

void ApplicationContext::initializePhase1(const QString& workspaceDir)
{
    LOG_INFO("Phase 1: Initializing core managers (Config, File)");
    
    // 1. ConfigManager first - needed by everyone
    initializeConfigManager(workspaceDir);
    
    // 2. EventBus - core event system
    initializeEventBus();
    
    // 3. ThemeManager - UI theming system
    initializeThemeManager();
    m_currentPhase = 1;
    emit phaseInitialized(1);
    emit configLoaded();
}

void ApplicationContext::initializePhase2()
{
    LOG_INFO("Phase 2: Initializing network managers");
    
    // 3. NetworkManager - depends on config for API settings, proxies, etc.
    initializeNetworkManager();
    
    m_currentPhase = 2;
    emit phaseInitialized(2);
    emit networkReady();
}

void ApplicationContext::initializePhase3()
{
    LOG_INFO("Phase 3: Initializing data managers");
    
    // 4. PlaylistManager - depends on config and potentially network
    initializePlaylistManager();
    
    // 5. AudioPlayerController - depends on PlaylistManager and NetworkManager
    initializeAudioPlayerController();
    
    m_currentPhase = 3;
    emit phaseInitialized(3);
    emit dataManagersReady();
}

void ApplicationContext::initializeConfigManager(const QString& workspaceDir)
{
    LOG_DEBUG("Creating ConfigManager...");
    m_configManager = std::make_unique<ConfigManager>(this);
    
    // Initialize with workspace directory
    m_configManager->initialize(workspaceDir);
    
    LOG_DEBUG("ConfigManager initialized and config loaded");
}

void ApplicationContext::initializeEventBus()
{
    LOG_DEBUG("Creating EventBus...");
    // EventBus uses a factory Create() returning shared_ptr
    m_eventBus = EventBus::Create();
    LOG_DEBUG("EventBus initialized");
}

void ApplicationContext::initializeThemeManager()
{
    LOG_DEBUG("Creating ThemeManager...");
    m_themeManager = std::make_unique<ThemeManager>(this);
    
    // Load theme from config if available
    if (m_configManager) {
        QString theme = m_configManager->getTheme();
        if (!theme.isEmpty()) {
            m_themeManager->loadTheme(theme);
            LOG_DEBUG("Loaded theme from config: {}", theme.toStdString());
        }
    }
    
    LOG_DEBUG("ThemeManager initialized");
}

// FileManager functionality is now integrated into ConfigManager
// No separate FileManager needed

void ApplicationContext::initializeNetworkManager()
{
    LOG_DEBUG("Creating NetworkManager...");
    
    // Use singleton instance and configure it
    QString platformDir = m_configManager->getPlatformDirectory();
    int timeout = m_configManager->getNetworkTimeout();
    QString proxyUrl = m_configManager->getProxyUrl();
    if (QFile::exists(platformDir) == false) {
        QDir().mkpath(platformDir);
        LOG_INFO("Created platform directory at {}", platformDir.toStdString());
    }
    m_networkManager = std::make_shared<network::NetworkManager>();
    m_networkManager->configure(platformDir, timeout, proxyUrl);

    LOG_DEBUG("NetworkManager initialized with config settings");
}

void ApplicationContext::initializePlaylistManager()
{
    LOG_DEBUG("Creating PlaylistManager...");
    
    // PlaylistManager needs ConfigManager
    m_playlistManager = std::make_unique<PlaylistManager>(m_configManager.get(), this);
    
    // Initialize and load existing playlists
    m_playlistManager->initialize();
    
    LOG_DEBUG("PlaylistManager initialized and playlists loaded");
}

void ApplicationContext::initializeAudioPlayerController()
{
    LOG_DEBUG("Creating AudioPlayerController...");
    
    // AudioPlayerController needs PlaylistManager and NetworkManager
    m_audioPlayerController = std::make_unique<audio::AudioPlayerController>(this);
    m_audioPlayerController->loadConfig();
    
    LOG_DEBUG("AudioPlayerController initialized");
}

void ApplicationContext::setupManagerConnections()
{
    LOG_DEBUG("Setting up cross-manager signal connections...");
    
    // Config changes should update other managers
    connect(m_configManager.get(), &ConfigManager::themeChanged,
            this, [this](const QString& theme) {
                LOG_INFO("Theme changed to: {}", theme.toStdString());
                // Notify UI components via signal
            });
    
    connect(m_configManager.get(), &ConfigManager::languageChanged,
            this, [this](const QString& language) {
                LOG_INFO("Language changed to: {}", language.toStdString());
                // Reload UI strings if needed
            });
    // Network errors should be logged (NetworkManager is singleton, so connect differently if needed)
    // For now, NetworkManager will log its own errors through LogManager
    
    LOG_DEBUG("Cross-manager connections established");
}

bool ApplicationContext::isPhaseInitialized(int phase) const
{
    return m_currentPhase >= phase;
}

void ApplicationContext::gracefulShutdown(int timeoutMs)
{
    if (!m_initialized) {
        LOG_DEBUG("ApplicationContext already shutdown or not initialized");
        return;
    }
    
    LOG_INFO("Starting graceful shutdown with timeout: {}ms", timeoutMs);
    LOG_DEBUG("ApplicationContext::gracefulShutdown called on thread {} (main thread {})",
              (void*)QThread::currentThread(), (void*)QCoreApplication::instance()->thread());
    emit shutdownStarted();
    
    // Use QTimer for timeout protection
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    bool timeoutOccurred = false;
    
    connect(&timeoutTimer, &QTimer::timeout, [&timeoutOccurred]() {
        timeoutOccurred = true;
        LOG_WARN("Shutdown timeout occurred - forcing immediate shutdown");
    });
    
    timeoutTimer.start(timeoutMs);
    
    // Perform shutdown with timeout protection
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        shutdown();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        if (timeoutOccurred) {
            LOG_WARN("Shutdown completed after timeout ({}ms)", duration);
        } else {
            LOG_INFO("Graceful shutdown completed in {}ms", duration);
        }
        
    } catch (...) {
        LOG_ERROR("Exception during graceful shutdown - performing emergency shutdown");
        shutdown(); // Try one more time
    }
    
    timeoutTimer.stop();
}

void ApplicationContext::shutdown()
{
    if (!m_initialized) {
        LOG_DEBUG("ApplicationContext already shutdown or not initialized");
        return;
    }
    
    LOG_INFO("Shutting down ApplicationContext...");
    
    try {
        // Phase 3 shutdown: Data managers and audio (reverse order)
        LOG_INFO("Shutdown Phase 3: Stopping audio and data managers");
        
        // Stop audio playback first to prevent audio glitches during shutdown
        if (m_audioPlayerController) {
            LOG_DEBUG("Stopping audio playback...");
            m_audioPlayerController->stop(); // Graceful stop before destruction
            m_audioPlayerController->saveConfig();
        }
        
        // Save playlist data before destroying manager
        if (m_playlistManager) {
            LOG_DEBUG("Saving playlist data...");
            m_playlistManager->saveAllCategories();
        }
        
        // Destroy Phase 3 managers
        m_audioPlayerController.reset();
        m_playlistManager.reset();
        emit shutdownPhaseCompleted(3);
        
        // Phase 2 shutdown: Network managers
        LOG_INFO("Shutdown Phase 2: Network cleanup");
        if (m_networkManager) {
            LOG_DEBUG("Saving network configuration...");
            m_networkManager->saveConfiguration();
        }
        m_networkManager.reset();
        emit shutdownPhaseCompleted(2);
        
        // Phase 1 shutdown: Core managers (config last)
        LOG_INFO("Shutdown Phase 1: Core manager cleanup");
        if (m_configManager) {
            LOG_DEBUG("Saving configuration...");
            m_configManager->saveToFile();
        }
        m_configManager.reset();
        emit shutdownPhaseCompleted(1);
        
        // Reset state flags
        m_initialized = false;
        m_currentPhase = 0;
        
        emit shutdownCompleted();
        LOG_INFO("ApplicationContext shutdown complete - all phases cleaned up successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error during ApplicationContext shutdown: {}", e.what());
        // Force reset even if error occurred to prevent zombie state
        m_audioPlayerController.reset();
        m_playlistManager.reset();
        m_networkManager.reset();
        m_configManager.reset();
        m_initialized = false;
        m_currentPhase = 0;
        
        emit shutdownCompleted(); // Still emit completion even on error
    }
    
    // Shutdown logging system last (after all other logging is complete)
    LogManager::instance().flush();
    LogManager::instance().shutdown();
}