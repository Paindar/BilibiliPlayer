#include "ffmpeg_log.h"
#include <log/log_manager.h>
#include <mutex>

extern "C" {
#include <libavutil/log.h>
}

namespace {
std::mutex g_ffmpeg_log_mutex;

static void ffmpeg_log_callback(void* /*ptr*/, int level, const char* fmt, va_list vl) {
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, vl);
    std::lock_guard<std::mutex> lk(g_ffmpeg_log_mutex);
    // Map ffmpeg levels to spdlog levels
    if (level <= AV_LOG_PANIC) {
        LOG_FFMPEG_CRITICAL("{}", buf);
    } else if (level <= AV_LOG_FATAL) {
        LOG_FFMPEG_CRITICAL("{}", buf);
    } else if (level <= AV_LOG_ERROR) {
        LOG_FFMPEG_ERROR("{}", buf);
    } else if (level <= AV_LOG_WARNING) {
        LOG_FFMPEG_WARN("{}", buf);
    } else if (level <= AV_LOG_INFO) {
        LOG_FFMPEG_INFO("{}", buf);
    } else {
        LOG_FFMPEG_DEBUG("{}", buf);
    }
}
} // namespace

namespace audio {
void InstallFFmpegLogCallback() {
    // Install callback and set a reasonable log level
    av_log_set_callback(&ffmpeg_log_callback);
    av_log_set_level(AV_LOG_INFO);
}
} // namespace audio
