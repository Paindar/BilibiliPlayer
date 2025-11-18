#include "ffmpeg_probe.h"
#include <log/log_manager.h>
#include <QFile>
#include <climits>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace audio
{
    AVFormatContext* FFmpegProbe::openFormatContext(const QString& path)
    {
        std::string pathStr = path.toStdString();
        AVFormatContext* fmtCtx = nullptr;

        if (avformat_open_input(&fmtCtx, pathStr.c_str(), nullptr, nullptr) != 0) {
            LOG_DEBUG("FFmpegProbe: Failed to open file: {}", pathStr);
            return nullptr;
        }

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            LOG_DEBUG("FFmpegProbe: Failed to find stream info for: {}", pathStr);
            avformat_close_input(&fmtCtx);
            return nullptr;
        }

        return fmtCtx;
    }

    AudioMetadata FFmpegProbe::probeMetadata(const QString& path)
    {
        std::string pathStr = path.toStdString();
        AudioMetadata metadata;

        AVFormatContext* fmtCtx = openFormatContext(path);
        if (!fmtCtx) {
            LOG_DEBUG("FFmpegProbe::probeMetadata: Could not open format context for: {}", pathStr);
            return metadata;  // Return empty metadata
        }

        // Probe duration
        if (fmtCtx->duration != AV_NOPTS_VALUE) {
            metadata.durationMs = (fmtCtx->duration * 1000) / AV_TIME_BASE;
        } else {
            // Try to get duration from first audio stream
            int audioStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
            if (audioStreamIndex >= 0) {
                AVStream* stream = fmtCtx->streams[audioStreamIndex];
                if (stream->duration != AV_NOPTS_VALUE) {
                    metadata.durationMs = static_cast<int64_t>(stream->duration * av_q2d(stream->time_base) * 1000);
                } else {
                    metadata.durationMs = INT_MAX;
                }
                // Extract sample rate and channels from audio stream
                metadata.sampleRate = stream->codecpar->sample_rate;
                metadata.channels = stream->codecpar->ch_layout.nb_channels;
            }
        }

        // Probe metadata tags: artist, title, album
        if (fmtCtx->metadata) {
            // Extract artist
            const char* artistTags[] = {"artist", "Artist", "ARTIST", "TPE1", "TP1", nullptr};
            for (int i = 0; artistTags[i] != nullptr; ++i) {
                AVDictionaryEntry* entry = av_dict_get(fmtCtx->metadata, artistTags[i], nullptr, 0);
                if (entry && entry->value) {
                    metadata.artist = QString::fromUtf8(entry->value);
                    break;
                }
            }

            // Fallback to album artist if artist not found
            if (metadata.artist.isEmpty()) {
                const char* albumArtistTags[] = {"album_artist", "Album Artist", "ALBUM_ARTIST", "TPE2", nullptr};
                for (int i = 0; albumArtistTags[i] != nullptr; ++i) {
                    AVDictionaryEntry* entry = av_dict_get(fmtCtx->metadata, albumArtistTags[i], nullptr, 0);
                    if (entry && entry->value) {
                        metadata.artist = QString::fromUtf8(entry->value);
                        break;
                    }
                }
            }

            // Extract title
            AVDictionaryEntry* titleEntry = av_dict_get(fmtCtx->metadata, "title", nullptr, 0);
            if (!titleEntry) titleEntry = av_dict_get(fmtCtx->metadata, "Title", nullptr, 0);
            if (titleEntry && titleEntry->value) {
                metadata.title = QString::fromUtf8(titleEntry->value);
            }

            // Extract album
            AVDictionaryEntry* albumEntry = av_dict_get(fmtCtx->metadata, "album", nullptr, 0);
            if (!albumEntry) albumEntry = av_dict_get(fmtCtx->metadata, "Album", nullptr, 0);
            if (albumEntry && albumEntry->value) {
                metadata.album = QString::fromUtf8(albumEntry->value);
            }
        }

        avformat_close_input(&fmtCtx);

        LOG_DEBUG("FFmpegProbe::probeMetadata for {}: duration={} ms, artist='{}', sampleRate={} Hz, channels={}",
                 pathStr, metadata.durationMs, metadata.artist.toStdString(), metadata.sampleRate, metadata.channels);

        return metadata;
    }

    int64_t FFmpegProbe::probeDuration(const QString& path)
    {
        std::string pathStr = path.toStdString();
        
        // Strategy 1: Check file metadata (Q_UNUSED if not using Qt metadata)
        // (Skipped here as FFmpeg will handle all cases uniformly)
        
        // Strategy 2: FFmpeg format context probe
        AVFormatContext* fmtCtx = openFormatContext(path);
        if (!fmtCtx) {
            LOG_DEBUG("FFmpegProbe: Could not open format context for: {}", pathStr);
            // Fallback to INT_MAX on probe failure
            return static_cast<int64_t>(INT_MAX);
        }

        int64_t duration = INT_MAX;  // Default fallback

        if (fmtCtx->duration != AV_NOPTS_VALUE) {
            // Convert from AV_TIME_BASE units (microseconds) to milliseconds
            duration = (fmtCtx->duration * 1000) / AV_TIME_BASE;
            LOG_DEBUG("FFmpegProbe: Probed duration for {}: {} ms", pathStr, duration);
        } else {
            // Try to get duration from first audio stream
            int audioStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
            if (audioStreamIndex >= 0) {
                AVStream* stream = fmtCtx->streams[audioStreamIndex];
                if (stream->duration != AV_NOPTS_VALUE) {
                    duration = static_cast<int64_t>(stream->duration * av_q2d(stream->time_base) * 1000);
                    LOG_DEBUG("FFmpegProbe: Probed duration from audio stream for {}: {} ms", pathStr, duration);
                } else {
                    LOG_DEBUG("FFmpegProbe: No duration in audio stream for: {}", pathStr);
                }
            } else {
                LOG_DEBUG("FFmpegProbe: No audio stream found for: {}", pathStr);
            }
        }

        avformat_close_input(&fmtCtx);
        return duration;
    }

    int FFmpegProbe::probeSampleRate(const QString& path)
    {
        AVFormatContext* fmtCtx = openFormatContext(path);
        if (!fmtCtx) {
            return 0;
        }

        int sampleRate = 0;

        // Find the first audio stream
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                sampleRate = fmtCtx->streams[i]->codecpar->sample_rate;
                LOG_DEBUG("FFmpegProbe: Probed sample rate for {}: {} Hz", 
                         path.toStdString(), sampleRate);
                break;
            }
        }

        avformat_close_input(&fmtCtx);
        return sampleRate;
    }

    int FFmpegProbe::probeChannelCount(const QString& path)
    {
        AVFormatContext* fmtCtx = openFormatContext(path);
        if (!fmtCtx) {
            return 0;
        }

        int channelCount = 0;

        // Find the first audio stream
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                channelCount = fmtCtx->streams[i]->codecpar->ch_layout.nb_channels;
                LOG_DEBUG("FFmpegProbe: Probed channel count for {}: {}", 
                         path.toStdString(), channelCount);
                break;
            }
        }

        avformat_close_input(&fmtCtx);
        return channelCount;
    }

    QString FFmpegProbe::probeArtist(const QString& path)
    {
        AVFormatContext* fmtCtx = openFormatContext(path);
        if (!fmtCtx) {
            return "";
        }

        QString artist;

        // Try to extract artist from format metadata
        if (fmtCtx->metadata) {
            // Try common artist tags in order of preference
            const char* artistTags[] = {"artist", "Artist", "ARTIST", "TPE1", "TP1", nullptr};
            
            for (int i = 0; artistTags[i] != nullptr; ++i) {
                AVDictionaryEntry* entry = av_dict_get(fmtCtx->metadata, artistTags[i], nullptr, 0);
                if (entry && entry->value) {
                    artist = QString::fromUtf8(entry->value);
                    LOG_DEBUG("FFmpegProbe: Found artist tag '{}' for {}: '{}'", 
                             artistTags[i], path.toStdString(), artist.toStdString());
                    break;
                }
            }

            // Fallback to album artist if artist not found
            if (artist.isEmpty()) {
                const char* albumArtistTags[] = {"album_artist", "Album Artist", "ALBUM_ARTIST", "TPE2", nullptr};
                
                for (int i = 0; albumArtistTags[i] != nullptr; ++i) {
                    AVDictionaryEntry* entry = av_dict_get(fmtCtx->metadata, albumArtistTags[i], nullptr, 0);
                    if (entry && entry->value) {
                        artist = QString::fromUtf8(entry->value);
                        LOG_DEBUG("FFmpegProbe: Found album artist tag '{}' for {}: '{}'", 
                                 albumArtistTags[i], path.toStdString(), artist.toStdString());
                        break;
                    }
                }
            }
        }

        avformat_close_input(&fmtCtx);
        
        if (artist.isEmpty()) {
            LOG_DEBUG("FFmpegProbe: No artist metadata found for: {}", path.toStdString());
        }
        
        return artist;
    }

}

