#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/format.h>

/**
 * Log Manager - centralized logging using spdlog
 * Handles all application logging with different levels and outputs
 */
class LogManager
{
public:
    enum class LogLevel {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Critical = 5
    };

    static LogManager& instance();
    
    // Initialize logging system
    void initialize(const std::string& logDirectory = "log");
    void shutdown();
    
    // Logging methods
    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        if (m_logger) {
            m_logger->trace(fmt::format(format, std::forward<Args>(args)...));
            m_logger->flush();
        }
    }
    
    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        if (m_logger) {
            m_logger->debug(fmt::format(format, std::forward<Args>(args)...));
            m_logger->flush();
        }
    }
    
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        if (m_logger) {
            m_logger->info(fmt::format(format, std::forward<Args>(args)...));
            m_logger->flush();
        }
    }
    
    template<typename... Args>
    void warn(const std::string& format, Args&&... args) {
        if (m_logger) {
            m_logger->warn(fmt::format(format, std::forward<Args>(args)...));
            m_logger->flush();
        }
    }
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        if (m_logger) {
            m_logger->error(fmt::format(format, std::forward<Args>(args)...));
            m_logger->flush();
        }
    }
    
    template<typename... Args>
    void critical(const std::string& format, Args&&... args) {
        if (m_logger) {
            m_logger->critical(fmt::format(format, std::forward<Args>(args)...));
            m_logger->flush();
        }
    }
    
    // Log level management
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    
    // File management
    void flush();
    void setRotationSize(size_t maxFileSize);
    void setMaxFiles(size_t maxFiles);
    // FFmpeg-specific logging
    template<typename... Args>
    void ffmpeg_trace(const std::string& format, Args&&... args) {
        auto lg = spdlog::get("ffmpeg"); if (lg) { lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, fmt::format(format, std::forward<Args>(args)...)); lg->flush(); }
    }
    template<typename... Args>
    void ffmpeg_debug(const std::string& format, Args&&... args) {
        auto lg = spdlog::get("ffmpeg"); if (lg) { lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, fmt::format(format, std::forward<Args>(args)...)); lg->flush(); }
    }
    template<typename... Args>
    void ffmpeg_info(const std::string& format, Args&&... args) {
        auto lg = spdlog::get("ffmpeg"); if (lg) { lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, fmt::format(format, std::forward<Args>(args)...)); lg->flush(); }
    }
    template<typename... Args>
    void ffmpeg_warn(const std::string& format, Args&&... args) {
        auto lg = spdlog::get("ffmpeg"); if (lg) { lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, fmt::format(format, std::forward<Args>(args)...)); lg->flush(); }
    }
    template<typename... Args>
    void ffmpeg_error(const std::string& format, Args&&... args) {
        auto lg = spdlog::get("ffmpeg"); if (lg) { lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, fmt::format(format, std::forward<Args>(args)...)); lg->flush(); }
    }
    template<typename... Args>
    void ffmpeg_critical(const std::string& format, Args&&... args) {
        auto lg = spdlog::get("ffmpeg"); if (lg) { lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, fmt::format(format, std::forward<Args>(args)...)); lg->flush(); }
    }
    
private:
    LogManager() = default;
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    
    std::shared_ptr<spdlog::logger> m_logger;
    std::shared_ptr<spdlog::logger> m_ffmpeg_logger;
    std::string m_logDirectory;
    bool m_initialized = false;
};

// Convenience macros for easy logging. These forward source location to spdlog so
// sink patterns using [%s:%#] get populated. The message itself is the provided
// format and args (no automatic function/line prefix) to avoid duplicate info
// when the sink pattern already includes file/line.
#define LOG_TRACE(...) do { auto _lg = spdlog::get("BilibiliPlayer"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__); } while(0)
#define LOG_DEBUG(...) do { auto _lg = spdlog::get("BilibiliPlayer"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__); } while(0)
#define LOG_INFO(...)  do { auto _lg = spdlog::get("BilibiliPlayer"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__); } while(0)
#define LOG_WARN(...)  do { auto _lg = spdlog::get("BilibiliPlayer"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn,  __VA_ARGS__); } while(0)
#define LOG_ERROR(...) do { auto _lg = spdlog::get("BilibiliPlayer"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__); } while(0)
#define LOG_CRITICAL(...) do { auto _lg = spdlog::get("BilibiliPlayer"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__); } while(0)

// FFmpeg-specific logging macros
#define LOG_FFMPEG_TRACE(...) do { auto _lg = spdlog::get("ffmpeg"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__); } while(0)
#define LOG_FFMPEG_DEBUG(...) do { auto _lg = spdlog::get("ffmpeg"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__); } while(0)
#define LOG_FFMPEG_INFO(...) do { auto _lg = spdlog::get("ffmpeg"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__); } while(0)
#define LOG_FFMPEG_WARN(...) do { auto _lg = spdlog::get("ffmpeg"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__); } while(0)
#define LOG_FFMPEG_ERROR(...) do { auto _lg = spdlog::get("ffmpeg"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__); } while(0)
#define LOG_FFMPEG_CRITICAL(...) do { auto _lg = spdlog::get("ffmpeg"); if (_lg) _lg->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__); } while(0)