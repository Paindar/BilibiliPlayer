#include "cover_cache.h"
#include <config/config_manager.h>
#include <util/md5.h>
#include <log/log_manager.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace util
{
    CoverCache::CoverCache(ConfigManager* configManager)
        : m_configManager(configManager)
    {
        if (!m_configManager) {
            LOG_WARN("CoverCache created with null ConfigManager");
        }
    }

    CoverCache::~CoverCache()
    {
        // No cleanup needed
    }

    QString CoverCache::generateCacheKey(const QString& filePathOrUrl)
    {
        std::string input = filePathOrUrl.toStdString();
        std::string hash = util::md5Hash(input);
        return QString::fromStdString(hash);
    }

    QString CoverCache::detectImageFormat(const QByteArray& data)
    {
        if (data.isEmpty()) {
            return "";
        }

        // Check magic bytes for common image formats
        if (data.startsWith("\xFF\xD8\xFF")) {
            return "jpg";  // JPEG
        } else if (data.startsWith("\x89PNG")) {
            return "png";  // PNG
        } else if (data.startsWith("GIF8")) {
            return "gif";  // GIF
        } else if (data.startsWith("\x00\x00\x01\x00")) {
            return "ico";  // ICO
        } else if (data.startsWith("RIFF") && data.contains("WEBP")) {
            return "webp";  // WebP
        }

        // Default to jpg if unknown
        LOG_DEBUG("CoverCache: Unknown image format, defaulting to jpg");
        return "jpg";
    }

    bool CoverCache::ensureCacheDirectoryExists() const
    {
        if (!m_configManager) {
            LOG_ERROR("CoverCache: ConfigManager is null, cannot ensure cache directory");
            return false;
        }

        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        if (coverCacheDir.isEmpty()) {
            LOG_ERROR("CoverCache: ConfigManager returned empty cache directory");
            return false;
        }

        QString fullPath = m_configManager->getAbsolutePath(coverCacheDir);
        QDir dir;
        if (!dir.mkpath(fullPath)) {
            LOG_ERROR("CoverCache: Failed to create cache directory: {}", fullPath.toStdString());
            return false;
        }

        return true;
    }

    QString CoverCache::getCoverCachePath(const QString& filePathOrUrl) const
    {
        if (!m_configManager) {
            return "";
        }

        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        if (coverCacheDir.isEmpty()) {
            return "";
        }

        QString fullCacheDir = m_configManager->getAbsolutePath(coverCacheDir);
        QString key = generateCacheKey(filePathOrUrl);
        // We'll use jpg as default, but actual extension is determined on save
        return QDir(fullCacheDir).filePath(QString("%1.jpg").arg(key));
    }

    bool CoverCache::hasCachedCover(const QString& filePathOrUrl) const
    {
        if (!ensureCacheDirectoryExists()) {
            return false;
        }

        QString key = generateCacheKey(filePathOrUrl);
        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        QString fullCacheDir = m_configManager->getAbsolutePath(coverCacheDir);

        // Check for any file with this key (different extensions possible)
        QDir dir(fullCacheDir);
        QStringList filters;
        filters << QString("%1.*").arg(key);
        QStringList files = dir.entryList(filters);

        return !files.isEmpty();
    }

    std::optional<QByteArray> CoverCache::getCachedCover(const QString& filePathOrUrl) const
    {
        if (!ensureCacheDirectoryExists()) {
            return std::nullopt;
        }

        QString key = generateCacheKey(filePathOrUrl);
        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        QString fullCacheDir = m_configManager->getAbsolutePath(coverCacheDir);

        // Find the cached file (any extension)
        QDir dir(fullCacheDir);
        QStringList filters;
        filters << QString("%1.*").arg(key);
        QStringList files = dir.entryList(filters);

        if (files.isEmpty()) {
            LOG_DEBUG("CoverCache: No cached cover for {}", filePathOrUrl.toStdString());
            return std::nullopt;
        }

        QString cachedFile = dir.filePath(files[0]);
        QFile file(cachedFile);

        if (!file.open(QIODevice::ReadOnly)) {
            LOG_WARN("CoverCache: Failed to open cached cover file: {}", cachedFile.toStdString());
            return std::nullopt;
        }

        QByteArray data = file.readAll();
        file.close();

        LOG_DEBUG("CoverCache: Retrieved cached cover from {}", cachedFile.toStdString());
        return data;
    }

    std::optional<QString> CoverCache::saveCover(const QString& filePathOrUrl, const QByteArray& coverData)
    {
        if (!ensureCacheDirectoryExists()) {
            LOG_ERROR("CoverCache: Failed to ensure cache directory exists");
            return std::nullopt;
        }

        if (coverData.isEmpty()) {
            LOG_WARN("CoverCache: Cannot cache empty cover data for {}", filePathOrUrl.toStdString());
            return std::nullopt;
        }

        QString key = generateCacheKey(filePathOrUrl);
        QString extension = detectImageFormat(coverData);
        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        QString fullCacheDir = m_configManager->getAbsolutePath(coverCacheDir);
        QString cachePath = QDir(fullCacheDir).filePath(QString("%1.%2").arg(key, extension));

        QFile file(cachePath);
        if (!file.open(QIODevice::WriteOnly)) {
            LOG_ERROR("CoverCache: Failed to open cache file for writing: {}", cachePath.toStdString());
            return std::nullopt;
        }

        qint64 written = file.write(coverData);
        file.close();

        if (written != coverData.size()) {
            LOG_WARN("CoverCache: Incomplete write to cache file: {} bytes written, {} bytes expected",
                    written, coverData.size());
            file.remove();
            return std::nullopt;
        }

        LOG_INFO("CoverCache: Saved cover cache for {} at {}", 
                filePathOrUrl.toStdString(), cachePath.toStdString());
        return cachePath;
    }

    bool CoverCache::removeCachedCover(const QString& filePathOrUrl)
    {
        QString key = generateCacheKey(filePathOrUrl);
        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        QString fullCacheDir = m_configManager->getAbsolutePath(coverCacheDir);

        QDir dir(fullCacheDir);
        QStringList filters;
        filters << QString("%1.*").arg(key);
        QStringList files = dir.entryList(filters);

        if (files.isEmpty()) {
            LOG_DEBUG("CoverCache: No cached cover to remove for {}", filePathOrUrl.toStdString());
            return false;
        }

        QString cachedFile = dir.filePath(files[0]);
        if (QFile(cachedFile).remove()) {
            LOG_INFO("CoverCache: Removed cached cover: {}", cachedFile.toStdString());
            return true;
        } else {
            LOG_WARN("CoverCache: Failed to remove cached cover: {}", cachedFile.toStdString());
            return false;
        }
    }

    int CoverCache::clearCache()
    {
        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        QString fullCacheDir = m_configManager->getAbsolutePath(coverCacheDir);
        QDir dir(fullCacheDir);

        if (!dir.exists()) {
            return 0;
        }

        QStringList files = dir.entryList(QDir::Files);
        int removed = 0;

        for (const auto& file : files) {
            if (dir.remove(file)) {
                removed++;
            }
        }

        LOG_INFO("CoverCache: Cleared cache, removed {} files", removed);
        return removed;
    }

    int64_t CoverCache::getCacheSizeBytes() const
    {
        QString coverCacheDir = m_configManager->getCoverCacheDirectory();
        QString fullCacheDir = m_configManager->getAbsolutePath(coverCacheDir);
        QDir dir(fullCacheDir);

        if (!dir.exists()) {
            return 0;
        }

        int64_t totalSize = 0;
        QStringList files = dir.entryList(QDir::Files);

        for (const auto& file : files) {
            QFileInfo fileInfo(dir.filePath(file));
            totalSize += fileInfo.size();
        }

        return totalSize;
    }
}
