#pragma once

#include <JuceHeader.h>
#include "audio/AudioCapture.h"
#include "audio/AudioPlayer.h"

//==============================================================================
/**
 * Sidechain Audio Plugin Processor
 *
 * Main plugin class that handles audio processing and recording.
 * The processor captures audio from the DAW for sharing on the social feed.
 */
class SidechainAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    SidechainAudioProcessor();
    ~SidechainAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Authentication state
    bool isAuthenticated() const { return authenticated; }
    void setAuthenticated(bool auth) { authenticated = auth; }

    //==============================================================================
    // Audio Recording API (called from Editor/UI thread)
    void startRecording();
    void stopRecording();
    bool isRecording() const { return audioCapture.isRecording(); }

    // Get recorded audio buffer (call after stopRecording)
    juce::AudioBuffer<float> getRecordedAudio();

    // Recording info
    double getRecordingLengthSeconds() const { return audioCapture.getRecordingLengthSeconds(); }
    double getMaxRecordingLengthSeconds() const { return audioCapture.getMaxRecordingLengthSeconds(); }
    float getRecordingProgress() const { return audioCapture.getRecordingProgress(); }
    bool isRecordingBufferFull() const { return audioCapture.isBufferFull(); }

    // Level metering (for UI display)
    float getPeakLevel(int channel) const { return audioCapture.getPeakLevel(channel); }
    float getRMSLevel(int channel) const { return audioCapture.getRMSLevel(channel); }

    // Sample rate (for UI calculations)
    double getCurrentSampleRate() const { return currentSampleRate; }

    //==============================================================================
    // DAW Transport Info (BPM detection via AudioPlayHead)
    // Returns 0.0 if BPM is not available from the host
    double getCurrentBPM() const { return currentBPM.load(); }
    bool isBPMAvailable() const { return bpmAvailable.load(); }

    //==============================================================================
    // Audio Playback (for feed audio)
    AudioPlayer& getAudioPlayer() { return audioPlayer; }
    const AudioPlayer& getAudioPlayer() const { return audioPlayer; }

private:
    //==============================================================================
    // Audio capture system
    AudioCapture audioCapture;
    juce::AudioBuffer<float> lastRecordedAudio;

    // Audio playback for feed
    AudioPlayer audioPlayer;

    // Audio settings (cached from prepareToPlay)
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    // State
    bool authenticated = false;

    // DAW transport info (updated on audio thread, read from UI thread)
    std::atomic<double> currentBPM { 0.0 };
    std::atomic<bool> bpmAvailable { false };
    std::atomic<bool> dawTransportPlaying { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainAudioProcessor)
};