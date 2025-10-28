#include "log_manager.h"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>
#include <iostream>

LogManager& LogManager::instance()
{
    static LogManager instance;
    return instance;
}

void LogManager::initialize(const std::string& logDirectory)
{
    if (m_initialized) {
        return;
    }
    
    try {
        m_logDirectory = logDirectory;
        
        // Ensure log directory exists
        std::filesystem::create_directories(logDirectory);
        
        // Create sinks
        std::vector<spdlog::sink_ptr> sinks;
        
        // Console sink (colored output)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        sinks.push_back(console_sink);
        
        // File sink (rotating files)
        std::string logFilePath = logDirectory + "/bilibili_player.log";
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFilePath, 1024 * 1024 * 10, 5); // 10MB per file, keep 5 files
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v");
        sinks.push_back(file_sink);
        
        // Create logger with both sinks
        m_logger = std::make_shared<spdlog::logger>("BilibiliPlayer", sinks.begin(), sinks.end());
        m_logger->set_level(spdlog::level::debug);
        m_logger->flush_on(spdlog::level::warn);
        
        // Register as default logger
        spdlog::register_logger(m_logger);
        spdlog::set_default_logger(m_logger);
        
        m_initialized = true;
        
        LOG_INFO("LogManager initialized successfully");
        LOG_INFO("Log directory: {}", logDirectory);
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize LogManager: " << e.what() << std::endl;
        m_initialized = false;
    }
}

void LogManager::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    LOG_INFO("LogManager shutting down...");
    
    if (m_logger) {
        m_logger->flush();
        spdlog::drop_all();
        m_logger.reset();
    }
    
    m_initialized = false;
}

void LogManager::setLogLevel(LogLevel level)
{
    if (!m_logger) return;
    
    spdlog::level::level_enum spdLevel;
    switch (level) {
        case LogLevel::Trace:    spdLevel = spdlog::level::trace; break;
        case LogLevel::Debug:    spdLevel = spdlog::level::debug; break;
        case LogLevel::Info:     spdLevel = spdlog::level::info; break;
        case LogLevel::Warn:     spdLevel = spdlog::level::warn; break;
        case LogLevel::Error:    spdLevel = spdlog::level::err; break;
        case LogLevel::Critical: spdLevel = spdlog::level::critical; break;
        default:                 spdLevel = spdlog::level::info; break;
    }
    
    m_logger->set_level(spdLevel);
    LOG_DEBUG("Log level changed to: {}", static_cast<int>(level));
}

LogManager::LogLevel LogManager::getLogLevel() const
{
    if (!m_logger) return LogLevel::Info;
    
    switch (m_logger->level()) {
        case spdlog::level::trace:    return LogLevel::Trace;
        case spdlog::level::debug:    return LogLevel::Debug;
        case spdlog::level::info:     return LogLevel::Info;
        case spdlog::level::warn:     return LogLevel::Warn;
        case spdlog::level::err:      return LogLevel::Error;
        case spdlog::level::critical: return LogLevel::Critical;
        default:                      return LogLevel::Info;
    }
}

void LogManager::flush()
{
    if (m_logger) {
        m_logger->flush();
    }
}

void LogManager::setRotationSize(size_t maxFileSize)
{
    LOG_DEBUG("Log rotation size set to: {} bytes", maxFileSize);
    // Note: This would require recreating the file sink with new settings
    // For now, we'll just log the change
}

void LogManager::setMaxFiles(size_t maxFiles)
{
    LOG_DEBUG("Maximum log files set to: {}", maxFiles);
    // Note: This would require recreating the file sink with new settings
    // For now, we'll just log the change
}