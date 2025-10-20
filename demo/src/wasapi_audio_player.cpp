#include "wasapi_audio_player.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <comdef.h>

WASAPIAudioPlayer::WASAPIAudioPlayer(int sample_rate, int channels, int bits_per_sample) 
    : device_enumerator_(nullptr), audio_device_(nullptr), 
      audio_client_(nullptr), render_client_(nullptr), 
      wave_format_(nullptr), buffer_frame_count_(0), initialized_(false), is_playing_(false),
      input_sample_rate_(sample_rate), input_channels_(channels), input_bits_per_sample_(bits_per_sample) {
    
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to initialize COM" << std::endl;
        return;
    }
    
    // Create device enumerator
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, 
                          __uuidof(IMMDeviceEnumerator), (void**)&device_enumerator_);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to create device enumerator" << std::endl;
        return;
    }
    
    // Get default audio endpoint
    hr = device_enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &audio_device_);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to get default audio endpoint" << std::endl;
        return;
    }
    
    // Activate audio client
    hr = audio_device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client_);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to activate audio client" << std::endl;
        return;
    }
    
    // Get the device's preferred format first
    WAVEFORMATEX* device_format = nullptr;
    hr = audio_client_->GetMixFormat(&device_format);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to get mix format" << std::endl;
        return;
    }
    
    // Set up our desired wave format
    wave_format_ = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
    wave_format_->wFormatTag = WAVE_FORMAT_PCM;
    wave_format_->nChannels = static_cast<WORD>(channels);
    wave_format_->nSamplesPerSec = static_cast<DWORD>(sample_rate);
    wave_format_->wBitsPerSample = static_cast<WORD>(bits_per_sample);
    wave_format_->nBlockAlign = static_cast<WORD>((channels * bits_per_sample) / 8);
    wave_format_->nAvgBytesPerSec = sample_rate * wave_format_->nBlockAlign;
    wave_format_->cbSize = 0;
    
    // Check if our format is supported, if not use the closest supported format
    WAVEFORMATEX* closest_format = nullptr;
    hr = audio_client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, wave_format_, &closest_format);
    
    if (hr == S_FALSE) {
        // Our format is not supported, but we got a closest match
        CoTaskMemFree(wave_format_);
        wave_format_ = closest_format;
    } else if (FAILED(hr)) {
        // Format not supported and no closest match, fall back to device format
        CoTaskMemFree(wave_format_);
        wave_format_ = device_format;
        device_format = nullptr; // Prevent double free
    }
    
    if (device_format) {
        CoTaskMemFree(device_format);
    }
    

    
    // Initialize audio client with the supported format
    hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, wave_format_, nullptr);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to initialize audio client: 0x" << std::hex << hr << std::endl;
        return;
    }
    
    // Get buffer frame count
    hr = audio_client_->GetBufferSize(&buffer_frame_count_);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to get buffer size" << std::endl;
        return;
    }
    
    // Get render client
    hr = audio_client_->GetService(__uuidof(IAudioRenderClient), (void**)&render_client_);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Failed to get render client" << std::endl;
        return;
    }
    
    // Don't start the audio client here - let the user call start() explicitly
    initialized_ = true;
}

WASAPIAudioPlayer::~WASAPIAudioPlayer() {
    cleanup();
}

void WASAPIAudioPlayer::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (audio_client_) {
        audio_client_->Stop();
        audio_client_->Release();
        audio_client_ = nullptr;
    }
    
    is_playing_ = false;
    
    if (render_client_) {
        render_client_->Release();
        render_client_ = nullptr;
    }
    
    if (audio_device_) {
        audio_device_->Release();
        audio_device_ = nullptr;
    }
    
    if (device_enumerator_) {
        device_enumerator_->Release();
        device_enumerator_ = nullptr;
    }
    
    if (wave_format_) {
        CoTaskMemFree(wave_format_);
        wave_format_ = nullptr;
    }
    
    CoUninitialize();
}

void WASAPIAudioPlayer::playAudioData(const uint8_t* data, int size) {
    if (!initialized_ || !is_playing_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    UINT32 frames_available;
    HRESULT hr = audio_client_->GetCurrentPadding(&frames_available);
    if (FAILED(hr)) return;
    
    UINT32 frames_to_write = buffer_frame_count_ - frames_available;
    if (frames_to_write == 0) return;
    
    // Calculate how many frames we have in the input data using stored format
    const UINT32 input_bytes_per_sample = input_bits_per_sample_ / 8;
    const UINT32 input_bytes_per_frame = input_channels_ * input_bytes_per_sample;
    UINT32 input_frames = size / input_bytes_per_frame;
    
    if (input_frames == 0) return;
    
    // Handle sample rate conversion if needed
    UINT32 output_frames = input_frames;
    if (wave_format_->nSamplesPerSec != static_cast<DWORD>(input_sample_rate_)) {
        // Simple sample rate conversion ratio
        double ratio = static_cast<double>(wave_format_->nSamplesPerSec) / static_cast<double>(input_sample_rate_);
        output_frames = static_cast<UINT32>(input_frames * ratio);
    }
    
    // Write only what we can fit
    UINT32 frames_to_copy = std::min(frames_to_write, output_frames);
    
    if (frames_to_copy > 0) {
        BYTE* buffer_data;
        hr = render_client_->GetBuffer(frames_to_copy, &buffer_data);
        if (SUCCEEDED(hr)) {
            convertAndWriteAudio(data, size, buffer_data, frames_to_copy);
            
            hr = render_client_->ReleaseBuffer(frames_to_copy, 0);
            if (FAILED(hr)) {
                std::cerr << "WASAPI: Failed to release buffer: 0x" << std::hex << hr << std::endl;
            }
        }
    }
}

void WASAPIAudioPlayer::convertAndWriteAudio(const uint8_t* data, int size, BYTE* buffer_data, UINT32 frames_to_copy) {
    const UINT32 input_bytes_per_sample = input_bits_per_sample_ / 8;
    const UINT32 input_bytes_per_frame = input_channels_ * input_bytes_per_sample;
    UINT32 input_frames = size / input_bytes_per_frame;
    
    if (wave_format_->wBitsPerSample == input_bits_per_sample_ && 
        wave_format_->nChannels == static_cast<WORD>(input_channels_) && 
        wave_format_->nSamplesPerSec == static_cast<DWORD>(input_sample_rate_)) {
        // Direct copy for exact match
        memcpy(buffer_data, data, frames_to_copy * input_bytes_per_frame);
    } else if (wave_format_->wBitsPerSample == 32) {
        // Convert to 32-bit float
        float* output_samples = reinterpret_cast<float*>(buffer_data);
        
        for (UINT32 frame = 0; frame < frames_to_copy; frame++) {
            // Sample rate conversion: pick nearest input frame
            UINT32 input_frame = frame;
            if (wave_format_->nSamplesPerSec != static_cast<DWORD>(input_sample_rate_)) {
                double ratio = static_cast<double>(input_sample_rate_) / static_cast<double>(wave_format_->nSamplesPerSec);
                input_frame = static_cast<UINT32>(frame * ratio);
                if (input_frame >= input_frames) input_frame = input_frames - 1;
            }
            
            // Convert channels (handle different channel configurations)
            for (UINT32 ch = 0; ch < wave_format_->nChannels; ch++) {
                // Use left channel for mono output, or map channels appropriately
                UINT32 input_channel = ch < static_cast<UINT32>(input_channels_) ? ch : 0;
                
                // Handle different input bit depths
                float sample_float = 0.0f;
                if (input_bits_per_sample_ == 16) {
                    const int16_t* input_16 = reinterpret_cast<const int16_t*>(data);
                    int16_t sample = input_16[input_frame * input_channels_ + input_channel];
                    sample_float = sample / 32768.0f;
                } else if (input_bits_per_sample_ == 32) {
                    const float* input_32 = reinterpret_cast<const float*>(data);
                    sample_float = input_32[input_frame * input_channels_ + input_channel];
                }
                
                output_samples[frame * wave_format_->nChannels + ch] = sample_float;
            }
        }
    } else {
        // Fallback: copy available data with basic format handling
        UINT32 bytes_to_copy = std::min(static_cast<UINT32>(size), frames_to_copy * wave_format_->nBlockAlign);
        memcpy(buffer_data, data, bytes_to_copy);
    }
}

bool WASAPIAudioPlayer::isInitialized() const {
    return initialized_;
}

WASAPIAudioPlayer::OutputFormat WASAPIAudioPlayer::getOutputFormat() const {
    if (!wave_format_) {
        return {0, 0, 0};
    }
    
    return {
        static_cast<int>(wave_format_->nSamplesPerSec),
        static_cast<int>(wave_format_->nChannels),
        static_cast<int>(wave_format_->wBitsPerSample)
    };
}

bool WASAPIAudioPlayer::start() {
    if (!initialized_) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_playing_) return true; // Already playing
    
    HRESULT hr = audio_client_->Start();
    if (SUCCEEDED(hr)) {
        is_playing_ = true;
        return true;
    }
    
    std::cerr << "WASAPI: Failed to start audio client: 0x" << std::hex << hr << std::endl;
    return false;
}

bool WASAPIAudioPlayer::pause() {
    if (!initialized_) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_playing_) return true; // Already paused
    
    HRESULT hr = audio_client_->Stop();
    if (SUCCEEDED(hr)) {
        is_playing_ = false;
        return true;
    }
    
    std::cerr << "WASAPI: Failed to pause audio client: 0x" << std::hex << hr << std::endl;
    return false;
}

bool WASAPIAudioPlayer::stop() {
    if (!initialized_) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    HRESULT hr = audio_client_->Stop();
    if (SUCCEEDED(hr)) {
        // Reset the audio client to clear any buffered audio
        hr = audio_client_->Reset();
        is_playing_ = false;
        
        if (SUCCEEDED(hr)) {
            return true;
        } else {
            std::cerr << "WASAPI: Failed to reset audio client: 0x" << std::hex << hr << std::endl;
            return false;
        }
    }
    
    std::cerr << "WASAPI: Failed to stop audio client: 0x" << std::hex << hr << std::endl;
    return false;
}

bool WASAPIAudioPlayer::isPlaying() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_playing_;
}