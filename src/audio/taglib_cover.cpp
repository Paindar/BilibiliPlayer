#include "taglib_cover.h"
#include <log/log_manager.h>
#include <QFile>
#include <QFileInfo>
#include <fstream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

namespace audio
{
    std::vector<uint8_t> TagLibCoverExtractor::extractViaTagLib(const QString& path)
    {
        std::vector<uint8_t> empty;
        std::string pathStr = path.toStdString();
        
        // Try to open with TagLib
        // Note: TagLib support would require including TagLib headers here
        // For now, return empty to trigger FFmpeg fallback
        LOG_DEBUG("TagLibCoverExtractor: TagLib extraction not yet implemented, will try FFmpeg fallback");
        return empty;
    }

    std::vector<uint8_t> TagLibCoverExtractor::extractViaFFmpeg(const QString& path)
    {
        std::vector<uint8_t> result;
        std::string pathStr = path.toStdString();
        
        AVFormatContext* fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, pathStr.c_str(), nullptr, nullptr) != 0) {
            LOG_DEBUG("TagLibCoverExtractor: Failed to open file for cover extraction: {}", pathStr);
            return result;
        }

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            LOG_DEBUG("TagLibCoverExtractor: Failed to find stream info for: {}", pathStr);
            avformat_close_input(&fmtCtx);
            return result;
        }

        // Look for attached pictures in metadata
        AVDictionaryEntry* tag = nullptr;
        while ((tag = av_dict_get(fmtCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            if (tag->key != nullptr && tag->value != nullptr) {
                LOG_DEBUG("TagLibCoverExtractor: Found metadata tag: {}={}", tag->key, tag->value);
            }
        }

        // Search for video/picture streams (typically index 0 if present)
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                // This is likely attached picture/cover
                // In a real implementation, we'd decode the first frame
                // For now, log and continue
                LOG_DEBUG("TagLibCoverExtractor: Found potential picture stream at index {}", i);
                // Note: Actual extraction would require libavcodec for decoding
            }
        }

        avformat_close_input(&fmtCtx);
        
        if (result.empty()) {
            LOG_DEBUG("TagLibCoverExtractor: No cover found in file: {}", pathStr);
        }
        
        return result;
    }

    std::vector<uint8_t> TagLibCoverExtractor::extractCover(const QString& path)
    {
        // Strategy: Try TagLib first, then FFmpeg
        auto coverData = extractViaTagLib(path);
        
        if (coverData.empty()) {
            LOG_DEBUG("TagLibCoverExtractor: TagLib extraction failed, trying FFmpeg fallback");
            coverData = extractViaFFmpeg(path);
        }
        
        if (coverData.empty()) {
            LOG_INFO("TagLibCoverExtractor: No cover art found in: {}", path.toStdString());
        } else {
            LOG_INFO("TagLibCoverExtractor: Successfully extracted cover from: {} ({} bytes)", 
                    path.toStdString(), coverData.size());
        }
        
        return coverData;
    }

    bool TagLibCoverExtractor::extractAndSaveCover(const QString& audioPath, const QString& outputPath)
    {
        auto coverData = extractCover(audioPath);
        
        if (coverData.empty()) {
            LOG_WARN("TagLibCoverExtractor: No cover data to save for: {}", audioPath.toStdString());
            return false;
        }

        // Ensure output directory exists
        QFileInfo fileInfo(outputPath);
        QFile outFile(outputPath);
        
        if (!outFile.open(QIODevice::WriteOnly)) {
            LOG_ERROR("TagLibCoverExtractor: Failed to open output file for writing: {}", 
                     outputPath.toStdString());
            return false;
        }

        size_t bytesWritten = outFile.write(reinterpret_cast<const char*>(coverData.data()), 
                                           static_cast<qint64>(coverData.size()));
        outFile.close();

        if (bytesWritten != coverData.size()) {
            LOG_ERROR("TagLibCoverExtractor: Failed to write all cover data to: {}", 
                     outputPath.toStdString());
            return false;
        }

        LOG_INFO("TagLibCoverExtractor: Cover saved to: {} ({} bytes)", 
                outputPath.toStdString(), bytesWritten);
        return true;
    }

    QString TagLibCoverExtractor::detectImageFormat(const std::vector<uint8_t>& data)
    {
        if (data.size() < 8) {
            return "";
        }

        // PNG signature: 89 50 4E 47 08 0D 0A 1A
        if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
            return "png";
        }

        // JPEG signature: FF D8 FF
        if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
            return "jpeg";
        }

        // GIF signature: 47 49 46 38 (GIF8)
        if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46 && data[3] == 0x38) {
            return "gif";
        }

        // WebP signature: RIFF ... WEBP
        if (data.size() >= 12 && data[0] == 0x52 && data[1] == 0x49 && 
            data[2] == 0x46 && data[3] == 0x46 && 
            data[8] == 0x57 && data[9] == 0x45 && data[10] == 0x42 && data[11] == 0x50) {
            return "webp";
        }

        return "unknown";
    }
}
