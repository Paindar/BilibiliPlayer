#pragma once

#include <util/circular_buffer.hpp>
#include <streambuf>
#include <istream>
#include <ostream>
#include <mutex>
#include <condition_variable>
#include <memory>

/**
 * @brief A thread-safe realtime pipe using a circular buffer internally.
 *
 * Writer threads obtain an output stream via getOutputStream(), and readers use getInputStream().
 * When the last output stream is destroyed, readers will automatically see EOF.
 */
class RealtimePipe {
public:
    explicit RealtimePipe(size_t capacity = 8192);

    std::shared_ptr<std::istream> getInputStream();
    std::shared_ptr<std::ostream> getOutputStream();

    /**
     * @brief Close both sides, unblocking all waiting operations.
     * This signals EOF to readers and stops any writers.
     */
    void close();

    void closeInput();   // manually close reader side
    void closeOutput();  // manually close writer side (signals EOF)

    /**
     * @brief Returns the number of bytes currently available for reading.
     * Thread-safe and non-blocking.
     */
    size_t available() const;

private:
    class PipeBuffer;
    class OutputStream;
    class InputStream;

    std::shared_ptr<PipeBuffer> buffer_;
};

// ===============================================================
// Internal implementation details
// ===============================================================

class RealtimePipe::PipeBuffer : public std::streambuf {
public:
    explicit PipeBuffer(size_t capacity);
    void closeInput();
    void closeOutput();
    size_t available() const;

protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override;
    std::streamsize xsgetn(char* s, std::streamsize n) override;
    int_type underflow() override;

private:
    util::CircularBuffer<char> buffer_;
    mutable std::mutex mutex_;
    std::condition_variable data_available_;
    std::condition_variable space_available_;
    bool input_closed_ = false;   // reader side closed
    bool output_closed_ = false;  // writer side closed (EOF signal)
    char current_char_ = 0;
};

// ===============================================================
// RAII OutputStream wrapper
// ===============================================================

class RealtimePipe::OutputStream : public std::ostream {
public:
    explicit OutputStream(std::shared_ptr<PipeBuffer> buf)
        : std::ostream(buf.get()), buffer_(std::move(buf)) {}

    // Automatically signals EOF when the last output stream is destroyed
    ~OutputStream() override {
        if (auto b = buffer_) b->closeOutput();
    }

private:
    std::shared_ptr<PipeBuffer> buffer_;
};

class RealtimePipe::InputStream : public std::istream {
public:
    InputStream(std::shared_ptr<PipeBuffer> buf)
            : std::istream(buf.get()), holder(std::move(buf)) {}
private:
    std::shared_ptr<PipeBuffer> holder;
};