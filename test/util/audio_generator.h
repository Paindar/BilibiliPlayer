#pragma once

#include <QString>

namespace test_audio {

// Write a PCM WAV file of given duration (seconds). Returns true on success.
bool write_silence_wav(const QString& outPath, double seconds,
                       int sampleRate = 44100, int channels = 1, int bitsPerSample = 16);

// Write a sine tone WAV file of given frequency (Hz) and duration (seconds).
bool write_sine_wav(const QString& outPath, double frequencyHz, double seconds,
                    int sampleRate = 44100, int channels = 1, int bitsPerSample = 16);

} // namespace test_audio
