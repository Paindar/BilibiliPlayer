#pragma once

#include <vector>
#include <util/safe_queue.hpp>
#include <memory>
#include <cstdint>

struct AudioFrame {
    std::vector<uint8_t> data;
    int sample_rate;
    int channels;
    int bits_per_sample{16};
    int64_t pts;
    
    AudioFrame(size_t size) : data(size) {}
    
    // Duration in seconds of this frame (based on data size, channels, sample rate and bits per sample)
    double durationSeconds() const {
        if (sample_rate <= 0 || channels <= 0 || bits_per_sample <= 0) return 0.0;
        int bytes_per_sample = bits_per_sample / 8;
        if (bytes_per_sample <= 0) return 0.0;
        // number of samples per channel
        int64_t samples = static_cast<int64_t>(data.size()) / (static_cast<int64_t>(channels) * bytes_per_sample);
        if (samples <= 0) return 0.0;
        return static_cast<double>(samples) / static_cast<double>(sample_rate);
    }

    // Duration in milliseconds
    int64_t durationMs() const {
        double s = durationSeconds();
        return static_cast<int64_t>(s * 1000.0 + 0.5);
    }
};

// Audio format information extracted from decoded stream
struct AudioFormat {
    int sample_rate;
    int channels;
    int bits_per_sample;
    
    constexpr bool isValid() const { return sample_rate > 0 && channels > 0 && bits_per_sample > 0; }
};

typedef SafeQueue<std::shared_ptr<AudioFrame>> AudioFrameQueue;