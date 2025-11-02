#include "application_context.h"
#include "../playlist/playlist_manager.h"
#include "../config/config_manager.h"
#include "../network/network_manager.h"
#include "../audio/audio_player_controller.h"
#include "../log/log_manager.h"
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

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
    
    // 2. FileManager - depends on config for paths/settings (if needed)
    // Note: FileManager functionality is now integrated into ConfigManager
    
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

// FileManager functionality is now integrated into ConfigManager
// No separate FileManager needed

void ApplicationContext::initializeNetworkManager()
{
    LOG_DEBUG("Creating NetworkManager...");
    
    // Use singleton instance and configure it
    QString platformDir = m_configManager->getPlatformDirectory();
    int timeout = m_configManager->getNetworkTimeout();
    QString proxyUrl = m_configManager->getProxyUrl();

    network::NetworkManager::instance().configure(platformDir, timeout, proxyUrl);

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
    m_audioPlayerController = std::make_unique<AudioPlayerController>(this);
    
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

void ApplicationContext::shutdown()
{
    if (!m_initialized) return;
    
    LOG_INFO("Shutting down ApplicationContext...");
    
    // Save current state before shutdown
    if (m_configManager) {
        m_configManager->saveToFile();
    }
    
    if (m_playlistManager) {
        m_playlistManager->saveAllCategories();
    }
    
    // Shutdown in reverse dependency order
    m_audioPlayerController.reset(); // Phase 3 - Stop audio first
    m_playlistManager.reset();       // Phase 3
    // NetworkManager is singleton - no cleanup needed here
    m_configManager.reset();         // Phase 1
    
    m_initialized = false;
    m_currentPhase = 0;
    
    // Shutdown logging last
    LogManager::instance().shutdown();
    
    LOG_INFO("ApplicationContext shutdown complete");
}