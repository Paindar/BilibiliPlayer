#pragma once

#include <future>
#include <optional>
#include <QString>
#include <memory>
#include <climits>
extern "C" {
    #include <libavformat/avformat.h>
}

namespace audio
{
    /**
     * @brief FFmpeg audio file probe utility for metadata extraction
     * 
     * Provides lightweight synchronous methods to query audio file metadata
     * (duration, sample rate, channels) without full decode or resampling.
     * 
     * Strategy:
     * - For duration: try metadata first → FFmpeg probe → fallback to INT_MAX
     * - Avoids redundant probes by checking file metadata before opening format context
     * - Used primarily by PlaylistManager for local media cover/duration updates
     */
    class FFmpegProbe
    {
    public:
        /**
         * @brief Synchronously probe audio file duration with fallback strategy
         * 
         * Tries duration from file metadata first; if unavailable, opens FFmpeg 
         * format context for header-based duration; if still unavailable, returns INT_MAX.
         * 
         * @param path Absolute path to audio file (local file path or file:// URL)
         * @return Duration in milliseconds: 
         *         - metadata duration if available
         *         - FFmpeg-probed duration if header contains it
         *         - INT_MAX if no duration available (allows UI to show "Unknown")
         *         - -1 if file cannot be opened
         */
        static int64_t probeDuration(const QString& path);

        /**
         * @brief Synchronously probe sample rate from file header
         * 
         * Opens FFmpeg format context briefly to extract sample rate.
         * 
         * @param path Absolute path to audio file
         * @return Sample rate in Hz, or 0 if probe fails
         */
        static int probeSampleRate(const QString& path);

        /**
         * @brief Synchronously probe channel count from file header
         * 
         * Opens FFmpeg format context briefly to extract channel layout.
         * 
         * @param path Absolute path to audio file
         * @return Number of channels, or 0 if probe fails
         */
        static int probeChannelCount(const QString& path);

    private:
        /**
         * @brief Internal helper for opening AVFormatContext
         * 
         * @param path File path
         * @return AVFormatContext pointer (caller must call avformat_close_input), or nullptr on error
         */
        static AVFormatContext* openFormatContext(const QString& path);
    };
}
