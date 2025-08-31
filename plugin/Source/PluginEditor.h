#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * Sidechain Audio Plugin Editor - Simplified for initial build
 */
class SidechainAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Button::Listener
{
public:
    SidechainAudioProcessorEditor (SidechainAudioProcessor&);
    ~SidechainAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;
    
    // Authentication UI methods
    void showAuthenticationDialog();

private:
    // Basic UI components
    std::unique_ptr<juce::TextButton> connectButton;
    std::unique_ptr<juce::Label> statusLabel;
    
    SidechainAudioProcessor& audioProcessor;
    
    static constexpr int PLUGIN_WIDTH = 400;
    static constexpr int PLUGIN_HEIGHT = 300;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainAudioProcessorEditor)
};