#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * AudioCapture handles recording audio from the DAW
 *
 * Thread Safety:
 * - captureAudio() is called from the AUDIO THREAD (processBlock)
 * - All other methods are called from the MESSAGE THREAD
 * - Uses std::atomic for thread-safe state sharing
 *
 * Features:
 * - Lock-free audio capture from processBlock
 * - Up to 60 seconds of recording
 * - Real-time level metering (peak + RMS)
 * - Waveform generation for UI
 */
class AudioCapture
{
public:
    AudioCapture();
    ~AudioCapture();

    //==============================================================================
    // Configuration - call from prepareToPlay() or message thread
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    //==============================================================================
    // Recording control - call from MESSAGE THREAD only
    void startRecording(const juce::String& recordingId = "");
    juce::AudioBuffer<float> stopRecording();
    bool isRecording() const { return recording.load(); }

    //==============================================================================
    // Audio capture - call from AUDIO THREAD (processBlock) only
    // MUST be lock-free and allocation-free
    void captureAudio(const juce::AudioBuffer<float>& buffer);

    //==============================================================================
    // Recording info - thread-safe reads
    double getRecordingLengthSeconds() const;
    int getRecordingLengthSamples() const { return recordingPosition.load(); }
    int getMaxRecordingSamples() const { return maxRecordingSamples; }
    double getMaxRecordingLengthSeconds() const;
    float getRecordingProgress() const; // 0.0 - 1.0
    bool isBufferFull() const;

    //==============================================================================
    // Level metering - thread-safe, updated during captureAudio()
    float getPeakLevel(int channel) const;
    float getRMSLevel(int channel) const;
    void resetLevels();

    //==============================================================================
    // Export utilities - call from MESSAGE THREAD
    juce::String generateWaveformSVG(const juce::AudioBuffer<float>& buffer, int width = 400, int height = 100);

    // Get sample rate for export
    double getSampleRate() const { return currentSampleRate; }
    int getNumChannels() const { return currentNumChannels; }

private:
    //==============================================================================
    // Thread-safe state (accessed from both threads)
    std::atomic<bool> recording { false };
    std::atomic<int> recordingPosition { 0 };

    // Level metering (written on audio thread, read on message thread)
    static constexpr int MaxChannels = 2;
    std::atomic<float> peakLevels[MaxChannels] { {0.0f}, {0.0f} };
    std::atomic<float> rmsLevels[MaxChannels] { {0.0f}, {0.0f} };

    // RMS calculation state (audio thread only)
    float rmsSums[MaxChannels] = { 0.0f, 0.0f };
    int rmsSampleCount = 0;
    static constexpr int RMSWindowSamples = 2048; // ~46ms @ 44.1kHz

    //==============================================================================
    // Configuration (set on message thread before recording)
    juce::String currentRecordingId;
    double currentSampleRate = 44100.0;
    int currentNumChannels = 2;
    int maxRecordingSamples = 0; // 60 seconds max

    //==============================================================================
    // Recording buffer (allocated on message thread, written on audio thread)
    juce::AudioBuffer<float> recordingBuffer;

    // Recorded data (message thread only)
    juce::AudioBuffer<float> recordedAudio;
    bool hasRecordedData = false;

    //==============================================================================
    void initializeBuffers();
    void updateLevels(const juce::AudioBuffer<float>& buffer, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioCapture)
};