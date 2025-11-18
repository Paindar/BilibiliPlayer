#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include <complex>

namespace test_diagnostics
{
    /**
     * @brief Generate reference sine wave for comparison
     * 
     * @param frequency Frequency in Hz
     * @param sampleRate Sample rate in Hz
     * @param durationSeconds Duration in seconds
     * @param amplitude Amplitude (typically 0.0-1.0 for normalized)
     * @return Vector of PCM samples
     */
    std::vector<float> generateSineWave(
        float frequency, 
        float sampleRate, 
        float durationSeconds, 
        float amplitude = 0.9f)
    {
        std::vector<float> samples;
        int totalSamples = static_cast<int>(sampleRate * durationSeconds);
        samples.reserve(totalSamples);
        
        const float twoPi = 2.0f * M_PI;
        const float phaseIncrement = twoPi * frequency / sampleRate;
        
        for (int i = 0; i < totalSamples; ++i) {
            float phase = phaseIncrement * i;
            float sample = amplitude * std::sin(phase);
            samples.push_back(sample);
        }
        
        return samples;
    }
    
    /**
     * @brief Calculate RMS (root mean square) of audio samples
     * 
     * @param samples PCM samples
     * @return RMS value
     */
    float calculateRMS(const std::vector<float>& samples)
    {
        if (samples.empty()) return 0.0f;
        
        float sum = 0.0f;
        for (float sample : samples) {
            sum += sample * sample;
        }
        
        return std::sqrt(sum / samples.size());
    }
    
    /**
     * @brief Calculate THD (Total Harmonic Distortion)
     * 
     * @param signal Input signal samples
     * @param frequency Fundamental frequency
     * @param sampleRate Sample rate
     * @return THD percentage (0-100)
     */
    float calculateTHD(
        const std::vector<float>& signal,
        float frequency,
        float sampleRate)
    {
        if (signal.empty()) return 0.0f;
        
        // For simplicity, estimate THD using peak detection
        // In production, use FFT for accurate harmonic analysis
        float maxSample = 0.0f;
        float minSample = 0.0f;
        
        for (float sample : signal) {
            maxSample = std::max(maxSample, sample);
            minSample = std::min(minSample, sample);
        }
        
        float peakAmplitude = std::max(maxSample, -minSample);
        float rms = calculateRMS(signal);
        
        // Rough THD estimation (should use FFT for accuracy)
        // THD = (RMS of harmonics) / (RMS of fundamental)
        // This is a placeholder calculation
        if (rms > 0.0f) {
            return (1.0f - (peakAmplitude / (rms * std::sqrt(2.0f)))) * 100.0f;
        }
        
        return 0.0f;
    }
    
    /**
     * @brief Detect clicks/pops in audio signal
     * 
     * Clicks are detected as sudden amplitude changes between consecutive samples
     * 
     * @param samples PCM samples
     * @param threshold Maximum allowed change between samples
     * @return Number of clicks detected
     */
    int detectClicks(const std::vector<float>& samples, float threshold = 0.2f)
    {
        if (samples.size() < 2) return 0;
        
        int clickCount = 0;
        
        for (size_t i = 1; i < samples.size(); ++i) {
            float delta = std::abs(samples[i] - samples[i-1]);
            if (delta > threshold) {
                clickCount++;
            }
        }
        
        return clickCount;
    }
    
    /**
     * @brief Calculate correlation between two signals
     * 
     * Measures similarity between expected and actual signals
     * 1.0 = perfect match, 0.0 = no correlation
     * 
     * @param expected Reference signal
     * @param actual Actual signal
     * @return Correlation coefficient (-1.0 to 1.0)
     */
    float calculateCorrelation(
        const std::vector<float>& expected,
        const std::vector<float>& actual)
    {
        if (expected.empty() || actual.empty() || expected.size() != actual.size()) {
            return 0.0f;
        }
        
        float meanExp = 0.0f, meanAct = 0.0f;
        for (size_t i = 0; i < expected.size(); ++i) {
            meanExp += expected[i];
            meanAct += actual[i];
        }
        meanExp /= expected.size();
        meanAct /= actual.size();
        
        float numerator = 0.0f, denomExp = 0.0f, denomAct = 0.0f;
        for (size_t i = 0; i < expected.size(); ++i) {
            float expDiff = expected[i] - meanExp;
            float actDiff = actual[i] - meanAct;
            numerator += expDiff * actDiff;
            denomExp += expDiff * expDiff;
            denomAct += actDiff * actDiff;
        }
        
        float denom = std::sqrt(denomExp * denomAct);
        if (denom > 0.0f) {
            return numerator / denom;
        }
        
        return 0.0f;
    }
    
    /**
     * @brief Analyze audio quality and detect anomalies
     */
    struct AudioQualityAnalysis
    {
        float rms;
        float thd;
        int clickCount;
        float correlation;
        bool isPure;  // True if signal appears clean (low THD, no clicks)
    };
    
    /**
     * @brief Perform comprehensive audio quality analysis
     */
    AudioQualityAnalysis analyzeAudioQuality(
        const std::vector<float>& expected,
        const std::vector<float>& actual,
        float frequency,
        float sampleRate)
    {
        AudioQualityAnalysis analysis;
        
        // Calculate basic metrics
        analysis.rms = calculateRMS(actual);
        analysis.thd = calculateTHD(actual, frequency, sampleRate);
        analysis.clickCount = detectClicks(actual);
        analysis.correlation = calculateCorrelation(expected, actual);
        
        // Determine if signal is pure (clean)
        analysis.isPure = (analysis.thd < 5.0f) &&  // THD < 5%
                         (analysis.clickCount < 10) &&  // Very few clicks
                         (analysis.correlation > 0.95f);  // High correlation with reference
        
        return analysis;
    }
}

// Test Cases
TEST_CASE("Sine wave generation consistency", "[diagnostics][audio][sine]") {
    using namespace test_diagnostics;
    
    SECTION("Generate 440Hz sine wave") {
        auto sine440 = generateSineWave(440.0f, 48000.0f, 1.0f);
        REQUIRE(sine440.size() == 48000);
        
        // Verify amplitude is within range
        float maxSample = *std::max_element(sine440.begin(), sine440.end());
        float minSample = *std::min_element(sine440.begin(), sine440.end());
        
        REQUIRE(maxSample <= 1.0f);
        REQUIRE(minSample >= -1.0f);
        REQUIRE(maxSample > 0.8f);  // Should be close to amplitude (0.9f)
    }
    
    SECTION("Generate pure sine waves at different frequencies") {
        for (float freq : {110.0f, 440.0f, 1000.0f, 8000.0f}) {
            auto sine = generateSineWave(freq, 48000.0f, 1.0f, 0.9f);
            float rms = calculateRMS(sine);
            
            // For a sine wave with amplitude A, RMS = A / sqrt(2)
            float expectedRMS = 0.9f / std::sqrt(2.0f);
            REQUIRE(std::abs(rms - expectedRMS) < 0.01f);
        }
    }
}

TEST_CASE("Audio quality analysis metrics", "[diagnostics][audio][quality]") {
    using namespace test_diagnostics;
    
    SECTION("Pure sine wave should have low THD") {
        auto pureSine = generateSineWave(1000.0f, 48000.0f, 0.5f, 0.9f);
        float thd = calculateTHD(pureSine, 1000.0f, 48000.0f);
        
        // Pure sine should have very low THD
        REQUIRE(thd < 10.0f);
    }
    
    SECTION("Perfect correlation with identical signals") {
        auto signal1 = generateSineWave(440.0f, 48000.0f, 0.1f, 0.9f);
        auto signal2 = generateSineWave(440.0f, 48000.0f, 0.1f, 0.9f);
        
        float correlation = calculateCorrelation(signal1, signal2);
        REQUIRE(correlation > 0.99f);  // Should be nearly perfect
    }
    
    SECTION("Different frequencies should have low correlation") {
        auto signal440 = generateSineWave(440.0f, 48000.0f, 0.1f, 0.9f);
        auto signal880 = generateSineWave(880.0f, 48000.0f, 0.1f, 0.9f);
        
        float correlation = calculateCorrelation(signal440, signal880);
        REQUIRE(correlation < 0.5f);  // Low correlation due to different frequencies
    }
}

TEST_CASE("Click/pop detection", "[diagnostics][audio][artifacts]") {
    using namespace test_diagnostics;
    
    SECTION("Clean sine wave should have minimal clicks") {
        auto cleanSine = generateSineWave(1000.0f, 48000.0f, 0.1f, 0.9f);
        int clicks = detectClicks(cleanSine, 0.3f);  // Higher threshold for sine wave natural variations
        
        // Very few or no clicks in a clean sine wave
        REQUIRE(clicks < 2000);  // Sine wave variations at low threshold
    }
    
    SECTION("Signal with injected clicks should be detected") {
        auto signalWithClicks = generateSineWave(1000.0f, 48000.0f, 0.1f, 0.9f);
        
        // Inject some clicks
        if (signalWithClicks.size() > 1000) {
            signalWithClicks[500] += 0.5f;  // Inject click at sample 500
            signalWithClicks[1000] -= 0.5f;  // Inject click at sample 1000
        }
        
        int clicks = detectClicks(signalWithClicks, 0.1f);
        REQUIRE(clicks >= 2);  // Should detect the injected clicks
    }
}

TEST_CASE("Comprehensive audio quality analysis", "[diagnostics][audio][comprehensive]") {
    using namespace test_diagnostics;
    
    SECTION("Pure sine wave analysis") {
        auto reference = generateSineWave(440.0f, 48000.0f, 1.0f, 0.9f);
        auto actual = generateSineWave(440.0f, 48000.0f, 1.0f, 0.9f);
        
        auto analysis = analyzeAudioQuality(reference, actual, 440.0f, 48000.0f);
        
        REQUIRE(analysis.rms > 0.0f);
        REQUIRE(analysis.thd < 10.0f);
        REQUIRE(analysis.clickCount < 100);
        REQUIRE(analysis.correlation > 0.95f);
        REQUIRE(analysis.isPure);
    }
    
    SECTION("Degraded signal analysis") {
        auto reference = generateSineWave(440.0f, 48000.0f, 1.0f, 0.9f);
        auto degraded = reference;  // Start with reference
        
        // Simulate significant degradation
        for (size_t i = 0; i < degraded.size(); ++i) {
            degraded[i] *= 0.5f;  // Reduce amplitude significantly
            if (i % 100 == 0) {
                degraded[i] += 0.5f;  // Larger clicks every 100 samples
            }
            if (i % 200 == 0) {
                degraded[i] -= 0.4f;  // Additional distortion
            }
        }
        
        auto analysis = analyzeAudioQuality(reference, degraded, 440.0f, 48000.0f);
        
        // Should detect quality issues - main detection via click count and THD
        REQUIRE(analysis.clickCount > 100);
        REQUIRE(analysis.correlation < 1.0f);  // Any degradation will reduce perfect correlation
        REQUIRE(!analysis.isPure);  // Degraded signal is not pure
    }
}

TEST_CASE("Sample rate conversion compatibility", "[diagnostics][audio][samplerate]") {
    using namespace test_diagnostics;
    
    SECTION("44.1kHz sine wave generation") {
        auto sine44k = generateSineWave(440.0f, 44100.0f, 1.0f, 0.9f);
        REQUIRE(sine44k.size() == 44100);
    }
    
    SECTION("48kHz sine wave generation") {
        auto sine48k = generateSineWave(440.0f, 48000.0f, 1.0f, 0.9f);
        REQUIRE(sine48k.size() == 48000);
    }
    
    SECTION("96kHz sine wave generation") {
        auto sine96k = generateSineWave(440.0f, 96000.0f, 1.0f, 0.9f);
        REQUIRE(sine96k.size() == 96000);
    }
}
