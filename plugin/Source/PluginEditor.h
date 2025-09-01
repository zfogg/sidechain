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
    void mouseUp (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Authentication UI methods
    void showAuthenticationDialog();
    void showSignupDialog();
    void handleOAuthLogin(const juce::String& provider);
    void handleEmailLogin();
    void handleEmailSignup();
    juce::Rectangle<int> getButtonArea(int index, int totalButtons) const;
    void drawFormField(juce::Graphics& g, const juce::String& label, const juce::String& value,
                      juce::Rectangle<int> area, bool isPassword = false, bool isActive = false);

private:
    // Basic UI components
    std::unique_ptr<juce::TextButton> connectButton;
    std::unique_ptr<juce::Label> statusLabel;

    SidechainAudioProcessor& audioProcessor;

    // UI state
    enum class AuthState { Disconnected, ChoosingMethod, EmailLogin, EmailSignup, Connecting, Connected, Error };
    AuthState authState = AuthState::Disconnected;
    juce::String username = "";
    juce::String email = "";
    juce::String password = "";
    juce::String confirmPassword = "";
    juce::String displayName = "";
    juce::String errorMessage = "";
    
    // Form input state
    int activeField = -1; // -1 = none, 0 = email, 1 = username, 2 = display, 3 = password, 4 = confirm

    static constexpr int PLUGIN_WIDTH = 1000;
    static constexpr int PLUGIN_HEIGHT = 800;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainAudioProcessorEditor)
};