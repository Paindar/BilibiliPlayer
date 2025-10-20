#pragma once

#include <cstdint>
#include <mutex>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <windows.h>

/**
 * Modern Windows WASAPI audio player for low-latency real-time audio output.
 * 
 * Features:
 * - Automatic format negotiation with audio device
 * - Sample rate and bit depth conversion
 * - Channel mapping support
 * - Thread-safe audio data submission
 * - Minimal latency buffering
 * 
 * Usage:
 *   WASAPIAudioPlayer player(44100, 2, 16);
 *   if (player.isInitialized()) {
 *       player.playAudioData(audio_data, size);
 *   }
 */
class WASAPIAudioPlayer {
public:
    /**
     * Constructor - initializes WASAPI with desired audio format
     * @param sample_rate Input audio sample rate (e.g., 44100, 48000)
     * @param channels Input audio channel count (1=mono, 2=stereo)
     * @param bits_per_sample Input audio bit depth (16 or 32)
     */
    WASAPIAudioPlayer(int sample_rate, int channels, int bits_per_sample = 16);
    
    /**
     * Destructor - cleans up WASAPI resources
     */
    ~WASAPIAudioPlayer();
    
    /**
     * Submit audio data for playback
     * @param data Raw audio data in the format specified in constructor
     * @param size Size of data in bytes
     */
    void playAudioData(const uint8_t* data, int size);
    
    /**
     * Check if the audio player initialized successfully
     * @return true if ready for playback, false if initialization failed
     */
    bool isInitialized() const;
    
    /**
     * Start audio playback (resume if paused)
     * @return true if successful, false if failed
     */
    bool start();
    
    /**
     * Pause audio playback (can be resumed with start())
     * @return true if successful, false if failed
     */
    bool pause();
    
    /**
     * Stop audio playback and clear buffer
     * @return true if successful, false if failed
     */
    bool stop();
    
    /**
     * Check if audio is currently playing
     * @return true if playing, false if paused/stopped
     */
    bool isPlaying() const;
    
    /**
     * Get the negotiated output format information
     */
    struct OutputFormat {
        int sample_rate;
        int channels;
        int bits_per_sample;
    };
    
    OutputFormat getOutputFormat() const;

private:
    // WASAPI COM interfaces
    IMMDeviceEnumerator* device_enumerator_;
    IMMDevice* audio_device_;
    IAudioClient* audio_client_;
    IAudioRenderClient* render_client_;
    WAVEFORMATEX* wave_format_;
    
    // Audio buffer management
    UINT32 buffer_frame_count_;
    bool initialized_;
    bool is_playing_;
    mutable std::mutex mutex_;
    
    // Input format for conversions
    int input_sample_rate_;
    int input_channels_;
    int input_bits_per_sample_;
    
    // Private methods
    void cleanup();
    void convertAndWriteAudio(const uint8_t* data, int size, BYTE* buffer_data, UINT32 frames_to_copy);
};