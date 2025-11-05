#pragma once

#include <cstdint>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <windows.h>


/**
 * WASAPIAudioOutputUnsafe - low-level WASAPI audio output handler (not thread-safe)
 */
class WASAPIAudioOutputUnsafe {
public:
    explicit WASAPIAudioOutputUnsafe();
    ~WASAPIAudioOutputUnsafe();
    bool initialize(int sample_rate, int channels, int bits_per_sample = 16);
    void playAudioData(const uint8_t* data, int size);
    bool isInitialized() const;
    

    bool start();
    bool pause();
    bool stop();
    bool isPlaying() const;
    int getAvailableFrames() const;  // Get number of frames available in buffer

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
    // Input format (used for conversion)
    int input_sample_rate_;
    int input_channels_;
    int input_bits_per_sample_;
    // Private helper for conversion
    void convertAndWriteAudio(const uint8_t* data, int size, BYTE* buffer_data, UINT32 frames_to_copy);
    // Cleanup resources
    void cleanup();
};