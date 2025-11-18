#pragma once

#include <QString>
#include <vector>
#include <optional>
#include <memory>

namespace audio
{
    /**
     * @brief TagLib-based cover art extractor for local audio files
     * 
     * Extracts album artwork from audio files using TagLib library.
     * Falls back to FFmpeg if TagLib extraction fails.
     * Returns raw image bytes (PNG/JPEG) without interpretation.
     */
    class TagLibCoverExtractor
    {
    public:
        /**
         * @brief Extract cover art from audio file
         * 
         * Attempts to extract cover art using the following strategy:
         * 1. TagLib metadata extraction (FLAC, MP3, MP4, etc.)
         * 2. FFmpeg embedded artwork extraction as fallback
         * 3. Returns empty vector if no cover found
         * 
         * @param path Absolute path to audio file
         * @return Vector of bytes representing image data (PNG/JPEG),
         *         or empty vector if no cover found
         */
        static std::vector<uint8_t> extractCover(const QString& path);

        /**
         * @brief Extract cover art and save to file
         * 
         * Convenience method that extracts cover and writes to disk.
         * 
         * @param audioPath Path to audio file
         * @param outputPath Path where to save extracted cover
         * @return true if cover was found and saved, false otherwise
         */
        static bool extractAndSaveCover(const QString& audioPath, const QString& outputPath);

        /**
         * @brief Detect image format from data
         * 
         * Analyzes header bytes to detect PNG, JPEG, etc.
         * 
         * @param data Image bytes
         * @return Format string ("png", "jpeg", "unknown", or empty if too short)
         */
        static QString detectImageFormat(const std::vector<uint8_t>& data);

    private:
        /**
         * @brief Extract cover using TagLib
         * 
         * @param path File path
         * @return Image bytes or empty vector on failure
         */
        static std::vector<uint8_t> extractViaTagLib(const QString& path);

        /**
         * @brief Extract cover using FFmpeg as fallback
         * 
         * @param path File path
         * @return Image bytes or empty vector on failure
         */
        static std::vector<uint8_t> extractViaFFmpeg(const QString& path);
    };
}
