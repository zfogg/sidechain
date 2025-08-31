#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * AudioCapture handles recording audio from the DAW
 * 
 * Features:
 * - Circular buffer for continuous capture
 * - Loop recording with start/stop
 * - Audio format conversion
 * - Waveform generation for UI
 */
class AudioCapture
{
public:
    AudioCapture();
    ~AudioCapture();
    
    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    
    // Recording control
    void startRecording(const juce::String& recordingId);
    juce::AudioBuffer<float> stopRecording();
    bool isRecording() const { return recording; }
    
    // Audio capture during processBlock
    void captureAudio(const juce::AudioBuffer<float>& buffer);
    
    // Utilities
    double getRecordingLengthSeconds() const;
    juce::String generateWaveformSVG(const juce::AudioBuffer<float>& buffer, int width = 400, int height = 100);
    
private:
    // Recording state
    bool recording = false;
    juce::String currentRecordingId;
    
    // Audio settings
    double currentSampleRate = 44100.0;
    int currentNumChannels = 2;
    
    // Recording buffer (circular buffer for continuous capture)
    juce::AudioBuffer<float> recordingBuffer;
    int recordingPosition = 0;
    int maxRecordingSamples = 0; // 30 seconds max
    
    // Recorded data
    juce::AudioBuffer<float> recordedAudio;
    bool hasRecordedData = false;
    
    void initializeBuffers();
    void copyToRecordingBuffer(const juce::AudioBuffer<float>& source);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioCapture)
};