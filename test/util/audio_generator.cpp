#include <util/audio_generator.h>

#include <QFile>
#include <QDir>
#include <QtGlobal>
#include <cmath>

namespace {
    // Helper to write little-endian integer into QByteArray
    template<typename T>
    void append_le(QByteArray &b, T value) {
        for (size_t i = 0; i < sizeof(T); ++i) {
            b.append(static_cast<char>((value >> (8 * i)) & 0xFF));
        }
    }
}

namespace test_audio {

bool write_wav_common(const QString& outPath, const QByteArray& pcmData, int sampleRate, int channels, int bitsPerSample) {
    QDir dir = QFileInfo(outPath).dir();
    if (!dir.exists()) dir.mkpath(".");

    QFile out(outPath);
    if (!out.open(QFile::WriteOnly)) return false;

    QByteArray header;
    // RIFF header
    header.append("RIFF");
    quint32 fileSizeMinus8 = 36 + static_cast<quint32>(pcmData.size());
    append_le<quint32>(header, fileSizeMinus8);
    header.append("WAVE");

    // fmt chunk
    header.append("fmt ");
    append_le<quint32>(header, 16); // PCM
    append_le<quint16>(header, 1); // audio format PCM
    append_le<quint16>(header, static_cast<quint16>(channels));
    append_le<quint32>(header, static_cast<quint32>(sampleRate));
    quint32 byteRate = sampleRate * channels * (bitsPerSample / 8);
    append_le<quint32>(header, byteRate);
    quint16 blockAlign = static_cast<quint16>(channels * (bitsPerSample / 8));
    append_le<quint16>(header, blockAlign);
    append_le<quint16>(header, static_cast<quint16>(bitsPerSample));

    // data chunk
    header.append("data");
    append_le<quint32>(header, static_cast<quint32>(pcmData.size()));

    out.write(header);
    out.write(pcmData);
    out.close();
    return true;
}

bool write_silence_wav(const QString& outPath, double seconds, int sampleRate, int channels, int bitsPerSample) {
    if (seconds <= 0.0) seconds = 0.1;
    int bytesPerSample = bitsPerSample / 8;
    qint64 totalSamples = static_cast<qint64>(std::ceil(seconds * sampleRate));
    qint64 totalFrames = totalSamples;
    qint64 totalBytes = totalFrames * channels * bytesPerSample;

    QByteArray pcm;
    pcm.resize(static_cast<int>(totalBytes));
    // already zeroed

    return write_wav_common(outPath, pcm, sampleRate, channels, bitsPerSample);
}

bool write_sine_wav(const QString& outPath, double frequencyHz, double seconds, int sampleRate, int channels, int bitsPerSample) {
    if (seconds <= 0.0) seconds = 0.1;
    if (frequencyHz <= 0.0) frequencyHz = 440.0;
    int bytesPerSample = bitsPerSample / 8;
    qint64 totalSamples = static_cast<qint64>(std::ceil(seconds * sampleRate));

    QByteArray pcm;
    pcm.resize(static_cast<int>(totalSamples * channels * bytesPerSample));

    const double twoPiF = 2.0 * M_PI * frequencyHz;
    const double amplitude = (bitsPerSample == 16) ? 0.6 * 32767.0 : 0.6 * 127.0;

    char* data = pcm.data();
    for (qint64 i = 0; i < totalSamples; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(sampleRate);
        double sampleVal = sin(twoPiF * t) * amplitude;
        if (bitsPerSample == 16) {
            qint16 s = static_cast<qint16>(std::lround(sampleVal));
            // write little-endian
            int offset = static_cast<int>(i * channels * 2);
            for (int ch = 0; ch < channels; ++ch) {
                data[offset + ch * 2 + 0] = static_cast<char>(s & 0xFF);
                data[offset + ch * 2 + 1] = static_cast<char>((s >> 8) & 0xFF);
            }
        } else if (bitsPerSample == 8) {
            quint8 s = static_cast<quint8>(std::lround(sampleVal + 128.0));
            int offset = static_cast<int>(i * channels);
            for (int ch = 0; ch < channels; ++ch) data[offset + ch] = static_cast<char>(s);
        }
    }

    return write_wav_common(outPath, pcm, sampleRate, channels, bitsPerSample);
}

} // namespace test_audio
