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
     * @brief Audio metadata structure containing duration, artist, and other information
     */
    struct AudioMetadata {
        int64_t durationMs = INT_MAX;      // Duration in milliseconds (INT_MAX if unknown)
        QString artist;                     // Artist/uploader name (empty if not found)
        int sampleRate = 0;                // Sample rate in Hz (0 if unknown)
        int channels = 0;                  // Channel count (0 if unknown)
        QString title;                     // Track title (empty if not found)
        QString album;                     // Album name (empty if not found)
        
        /**
         * @brief Check if metadata indicates a valid audio file
         * @return true if at least duration or sample rate was probed successfully
         */
        bool isValid() const {
            return (durationMs != INT_MAX || sampleRate > 0);
        }
    };

    /**
     * @brief FFmpeg audio file probe utility for metadata extraction
     * 
     * Provides lightweight synchronous methods to query audio file metadata
     * (duration, sample rate, channels, artist) without full decode or resampling.
     * 
     * Strategy:
     * - For duration: try metadata first → FFmpeg probe → fallback to INT_MAX
     * - For artist: check common tags (artist, album_artist) with fallback to empty
     * - Avoids redundant probes by checking file metadata before opening format context
     * - Used primarily by PlaylistManager for local media metadata updates
     */
    class FFmpegProbe
    {
    public:
        /**
         * @brief Synchronously probe comprehensive audio metadata in one pass
         * 
         * Efficiently extracts all available metadata by opening the file once:
         * - Duration (ms)
         * - Artist/uploader (from metadata tags)
         * - Sample rate (Hz)
         * - Channels
         * - Title
         * - Album
         * 
         * @param path Absolute path to audio file (local file path or file:// URL)
         * @return AudioMetadata struct with probed information; use isValid() to check validity
         */
        static AudioMetadata probeMetadata(const QString& path);

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

        /**
         * @brief Synchronously probe artist/uploader from file metadata
         * 
         * Attempts to extract artist metadata from common tags:
         * - TPE1 / Artist / Creator tags
         * Falls back to album artist if artist not found
         * 
         * @param path Absolute path to audio file
         * @return Artist name if found, empty string if not available
         */
        static QString probeArtist(const QString& path);

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

