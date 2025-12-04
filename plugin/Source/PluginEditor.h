#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ProfileSetupComponent.h"
#include "PostsFeedComponent.h"
#include "RecordingComponent.h"
#include "NetworkClient.h"
#include "ConnectionIndicator.h"

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
    void drawFocusedButton(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, 
                          juce::Colour bgColor, bool isFocused);

private:
    // Basic UI components
    std::unique_ptr<juce::TextButton> connectButton;
    std::unique_ptr<juce::Label> statusLabel;

    SidechainAudioProcessor& audioProcessor;

    // UI state
    enum class AuthState { Disconnected, ChoosingMethod, EmailLogin, EmailSignup, Connecting, Connected, Error };
    enum class AppView { Authentication, ProfileSetup, PostsFeed, Recording };
    
    AuthState authState = AuthState::Disconnected;
    AppView currentView = AppView::Authentication;
    juce::String username = "";
    juce::String email = "";
    juce::String password = "";
    juce::String confirmPassword = "";
    juce::String displayName = "";
    juce::String errorMessage = "";
    juce::String profilePicUrl = "";
    juce::String authToken = "";  // Store auth token for persistence
    bool hasCompletedProfileSetup = false;  // Track if user has seen profile setup
    
    // Components for different views
    std::unique_ptr<ProfileSetupComponent> profileSetupComponent;
    std::unique_ptr<PostsFeedComponent> postsFeedComponent;
    std::unique_ptr<RecordingComponent> recordingComponent;

    // Network client for API calls
    std::unique_ptr<NetworkClient> networkClient;

    // Connection status indicator
    std::unique_ptr<ConnectionIndicator> connectionIndicator;

    // Form input state
    int activeField = -1; // -1 = none, 0 = email, 1 = username, 2 = display, 3 = password, 4 = confirm
    
    // View management
    void showView(AppView view);
    void onLoginSuccess();
    void logout();

    // Persistent state
    void saveLoginState();
    void loadLoginState();
    
    // Keyboard navigation
    int currentFocusIndex = -1;  // -1 = none, 0+ = field/button index
    int maxFocusIndex = 0;       // Updated based on current auth state
    void updateFocusIndicators();
    void handleTabNavigation(bool reverse = false);
    bool handleEnterKey();

    static constexpr int PLUGIN_WIDTH = 1000;
    static constexpr int PLUGIN_HEIGHT = 800;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainAudioProcessorEditor)
};