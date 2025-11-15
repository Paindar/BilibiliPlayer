#include <catch2/catch_test_macros.hpp>
#include <audio/ffmpeg_decoder.h>
#include <audio/audio_frame.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <future>
#include <thread>

using namespace audio;

static std::string write_test_wav(const std::filesystem::path &dir) {
    std::filesystem::create_directories(dir);
    auto path = dir / "test_tone.wav";

    // WAV params
    const uint16_t audio_format = 1; // PCM
    const uint16_t channels = 2;
    const uint32_t sample_rate = 48000;
    const uint16_t bits_per_sample = 16;
    // Ensure we write at least 64KiB of audio data so FFmpeg's startDecode
    // (which expects a minimum buffered amount) does not block. Compute a
    // duration that yields >= 64*1024 bytes of data.
    const uint32_t min_bytes_needed = 64 * 1024;
    double duration_seconds = 0.2; // default 200ms
    {
        const uint16_t block_align_local = channels * (bits_per_sample / 8);
        double min_duration = static_cast<double>(min_bytes_needed) / (static_cast<double>(sample_rate) * static_cast<double>(block_align_local));
        if (duration_seconds < min_duration) duration_seconds = min_duration;
        // round up a bit to be safe
        duration_seconds = std::max(duration_seconds, min_duration + 0.05);
    }

    const uint32_t num_samples_per_channel = static_cast<uint32_t>(sample_rate * duration_seconds);
    const uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
    const uint16_t block_align = channels * (bits_per_sample / 8);
    const uint32_t data_bytes = num_samples_per_channel * block_align;

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) return "";

    // RIFF header
    ofs.write("RIFF", 4);
    uint32_t chunk_size = 36 + data_bytes;
    ofs.write(reinterpret_cast<const char*>(&chunk_size), 4);
    ofs.write("WAVE", 4);

    // fmt subchunk
    ofs.write("fmt ", 4);
    uint32_t subchunk1_size = 16;
    ofs.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    ofs.write(reinterpret_cast<const char*>(&audio_format), 2);
    ofs.write(reinterpret_cast<const char*>(&channels), 2);
    ofs.write(reinterpret_cast<const char*>(&sample_rate), 4);
    ofs.write(reinterpret_cast<const char*>(&byte_rate), 4);
    ofs.write(reinterpret_cast<const char*>(&block_align), 2);
    ofs.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    // data subchunk
    ofs.write("data", 4);
    ofs.write(reinterpret_cast<const char*>(&data_bytes), 4);

    // Write silence (zeros), due to startDecoding need at least 64*1024 bytes buffered
    std::vector<char> zeros(4096, 0);
    uint32_t remaining = data_bytes;
    while (remaining > 0) {
        uint32_t to_write = std::min<uint32_t>(remaining, static_cast<uint32_t>(zeros.size()));
        ofs.write(zeros.data(), to_write);
        remaining -= to_write;
    }

    ofs.close();
    return path.string();
}

TEST_CASE("FFmpegDecoder deterministic WAV decode", "[ffmpeg][wav][integration]") {
    // Create a temp WAV file in the system temp directory
    auto temp_dir = std::filesystem::temp_directory_path() / "BilibiliPlayerTestData";
    std::string wavPath = write_test_wav(temp_dir);
    REQUIRE(!wavPath.empty());

    // Open file stream
    auto file = std::make_shared<std::ifstream>(wavPath, std::ios::binary);
    REQUIRE(file->is_open());
    std::cout<<"Opened test WAV file: " << wavPath <<std::endl;

    // Get file size
    file->seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(file->tellg());
    file->seekg(0, std::ios::beg);
    std::cout<<"Test WAV file size: " << fileSize << " bytes" <<std::endl;

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();

    REQUIRE(decoder.initialize(file, frameQueue) == true);
    std::cout<<"Initialized decoder"<<std::endl;
    REQUIRE(decoder.startDecoding(fileSize) == true);
    std::cout<<"Started decoding WAV file"<<std::endl;
    REQUIRE(decoder.isDecoding() == true);
    std::cout<<"Started decoding WAV file"<<std::endl;

    // Wait for at least one decoded frame with timeout using async
    std::shared_ptr<AudioFrame> frameOut;
    auto popTask = std::async(std::launch::async, [&frameQueue, &frameOut]() {
        frameQueue->waitAndDequeue(frameOut);
        return true;
    });
    std::cout<<"Waiting for decoded frame..."<<std::endl;

    using namespace std::chrono_literals;
    auto status = popTask.wait_for(2000ms);
    std::cout<<"Wait for decoded frame status: " << (status == std::future_status::timeout ? "timeout" : "ready") <<std::endl;
    REQUIRE(status != std::future_status::timeout);
    REQUIRE(frameOut != nullptr);
    std::cout<<"Decoded frame size: " << frameOut->data.size() << " bytes" <<std::endl;

    // Basic checks on the decoded frame
    REQUIRE(frameOut->sample_rate > 0);
    REQUIRE(frameOut->channels > 0);
    REQUIRE(frameOut->data.size() > 0);

    // Clean up
    decoder.pauseDecoding();
    std::this_thread::sleep_for(50ms);
    file->close();
    std::filesystem::remove(wavPath);
}
