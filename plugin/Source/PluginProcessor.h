#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Sidechain Audio Plugin Processor
 * 
 * Main plugin class - simplified for initial build
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
    // Sidechain-specific functionality (simplified)
    bool isAuthenticated() const { return authenticated; }
    bool isRecording() const { return recording; }

private:
    //==============================================================================
    // Basic state for now
    bool authenticated = false;
    bool recording = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainAudioProcessor)
};