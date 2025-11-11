#include "log_manager.h"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

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
        
        // Load optional properties file (log.properties) to allow runtime configuration
        // Search order: <logDirectory>/log.properties, ./log.properties, ./config/log.properties
        std::unordered_map<std::string, std::string> props;
        std::vector<std::filesystem::path> candidates = {
            std::filesystem::current_path() / "config" / "log.properties",
            std::filesystem::path(logDirectory) / "log.properties",
            std::filesystem::current_path() / "log.properties"
        };
        for (const auto &p : candidates) {
            if (std::filesystem::exists(p)) {
                std::ifstream ifs(p);
                if (ifs) {
                    std::string line;
                    while (std::getline(ifs, line)) {
                        // trim
                        auto start = line.find_first_not_of(" \t\r\n");
                        if (start == std::string::npos) continue;
                        if (line[start] == '#') continue;
                        auto eq = line.find('=', start);
                        if (eq == std::string::npos) continue;
                        std::string key = line.substr(start, eq - start);
                        std::string val = line.substr(eq + 1);
                        // trim trailing whitespace
                        auto end = val.find_last_not_of(" \t\r\n");
                        if (end != std::string::npos) val = val.substr(0, end + 1);
                        // lowercase key
                        for (auto &c : key) c = static_cast<char>(::tolower(c));
                        props[key] = val;
                    }
                }
                break; // use first found properties file
            }
        }
        
        // Ensure log directory exists
        std::filesystem::create_directories(logDirectory);
        
        // Create sinks
        std::vector<spdlog::sink_ptr> sinks;
        
        // Console sink (colored output)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        // allow override via properties file
        if (props.count("console_level")) {
            std::string lvl = props["console_level"];
            for (auto &c : lvl) c = static_cast<char>(::toupper(c));
            if (lvl == "TRACE") console_sink->set_level(spdlog::level::trace);
            else if (lvl == "DEBUG") console_sink->set_level(spdlog::level::debug);
            else if (lvl == "INFO") console_sink->set_level(spdlog::level::info);
            else if (lvl == "WARN") console_sink->set_level(spdlog::level::warn);
            else if (lvl == "ERROR") console_sink->set_level(spdlog::level::err);
            else if (lvl == "CRITICAL") console_sink->set_level(spdlog::level::critical);
        } else {
            console_sink->set_level(spdlog::level::info);
        }
        if (props.count("console_pattern")) {
            console_sink->set_pattern(props["console_pattern"]);
        } else {
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        }
        sinks.push_back(console_sink);
        
        // File sink (rotating files)
        std::string fileName = "bilibili_player.log";
        if (props.count("file")) fileName = props["file"];
        std::string logFilePath = std::filesystem::path(logDirectory).append(fileName).string();
        size_t maxFileSize = 1024ULL * 1024ULL * 10ULL; // default 10MB
        size_t maxFiles = 5;
        if (props.count("max_size")) {
            try { maxFileSize = static_cast<size_t>(std::stoull(props["max_size"])); } catch(...) {}
        }
        if (props.count("max_files")) {
            try { maxFiles = static_cast<size_t>(std::stoul(props["max_files"])); } catch(...) {}
        }
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFilePath, maxFileSize, maxFiles);
        // allow override for file level
        if (props.count("file_level")) {
            std::string lvl = props["file_level"];
            for (auto &c : lvl) c = static_cast<char>(::toupper(c));
            if (lvl == "TRACE") file_sink->set_level(spdlog::level::trace);
            else if (lvl == "DEBUG") file_sink->set_level(spdlog::level::debug);
            else if (lvl == "INFO") file_sink->set_level(spdlog::level::info);
            else if (lvl == "WARN") file_sink->set_level(spdlog::level::warn);
            else if (lvl == "ERROR") file_sink->set_level(spdlog::level::err);
            else if (lvl == "CRITICAL") file_sink->set_level(spdlog::level::critical);
            else file_sink->set_level(spdlog::level::trace);
        } else {
            file_sink->set_level(spdlog::level::trace);
        }
        if (props.count("file_pattern")) {
            file_sink->set_pattern(props["file_pattern"]);
        } else {
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v");
        }
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