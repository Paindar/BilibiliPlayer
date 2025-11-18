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
}
