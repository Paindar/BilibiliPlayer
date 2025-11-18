#pragma once

#include <QString>
#include <QByteArray>
#include <optional>
#include <memory>

// Forward declaration
class ConfigManager;

namespace util
{
    /**
     * @brief Cover image cache manager for local and streaming media
     * 
     * Manages persistent caching of cover images extracted from audio files.
     * Uses MD5 hash of file path/URL as cache key.
     * 
     * Cache location: `{workspace_cache_dir}/covers/`
     * Cache files: `{md5_hash}.{extension}` (e.g., `abc123def.jpg`)
     */
    class CoverCache
    {
    public:
        explicit CoverCache(ConfigManager* configManager);
        ~CoverCache();

        /**
         * @brief Check if cover exists in cache
         * 
         * @param filePathOrUrl Source audio file path or URL
         * @return true if cached cover exists, false otherwise
         */
        bool hasCachedCover(const QString& filePathOrUrl) const;

        /**
         * @brief Retrieve cached cover image data
         * 
         * @param filePathOrUrl Source audio file path or URL
         * @return Cover image bytes, or std::nullopt if not cached
         */
        std::optional<QByteArray> getCachedCover(const QString& filePathOrUrl) const;

        /**
         * @brief Store cover image in cache
         * 
         * Automatically determines extension from image data (JPEG, PNG, etc.)
         * Creates cache directory if needed.
         * 
         * @param filePathOrUrl Source audio file path or URL
         * @param coverData Raw image bytes
         * @return Cache file path if successful, std::nullopt on error
         */
        std::optional<QString> saveCover(const QString& filePathOrUrl, const QByteArray& coverData);

        /**
         * @brief Get full path to cached cover file
         * 
         * Returns path whether file exists or not (useful for checking what path would be used).
         * 
         * @param filePathOrUrl Source audio file path or URL
         * @return Full cache file path
         */
        QString getCoverCachePath(const QString& filePathOrUrl) const;

        /**
         * @brief Remove cover from cache
         * 
         * @param filePathOrUrl Source audio file path or URL
         * @return true if removed successfully, false if not cached
         */
        bool removeCachedCover(const QString& filePathOrUrl);

        /**
         * @brief Clear entire cover cache
         * 
         * Removes all cached cover files.
         * 
         * @return Number of files removed
         */
        int clearCache();

        /**
         * @brief Get size of cover cache in bytes
         * 
         * @return Total size of cached cover files
         */
        int64_t getCacheSizeBytes() const;

    private:
        /**
         * @brief Generate MD5 hash for cache key
         * 
         * @param filePathOrUrl Source file path or URL
         * @return Hex string MD5 hash
         */
        static QString generateCacheKey(const QString& filePathOrUrl);

        /**
         * @brief Detect image format from data
         * 
         * @param data Image bytes
         * @return File extension (e.g., "jpg", "png", "webp"), or empty string if unknown
         */
        static QString detectImageFormat(const QByteArray& data);

        /**
         * @brief Ensure cache directory exists
         * 
         * Creates `{workspace_cache_dir}/covers/` if needed.
         * 
         * @return true if directory exists or was created successfully
         */
        bool ensureCacheDirectoryExists() const;

        ConfigManager* m_configManager;
    };
}
