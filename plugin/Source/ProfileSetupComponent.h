#pragma once

#include <JuceHeader.h>

class ProfileSetupComponent : public juce::Component
{
public:
    ProfileSetupComponent();
    ~ProfileSetupComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

    // Callback for when user wants to skip profile setup
    std::function<void()> onSkipSetup;

    // Callback for when user completes profile setup
    std::function<void()> onCompleteSetup;

    // Callback for when profile picture is selected
    std::function<void(const juce::String& picUrl)> onProfilePicSelected;

    // Callback for logout
    std::function<void()> onLogout;
    
    // Set user info
    void setUserInfo(const juce::String& username, const juce::String& email, const juce::String& picUrl = "");

private:
    juce::String username;
    juce::String email;
    juce::String profilePicUrl;
    
    // UI helper methods
    void drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds);
    juce::Rectangle<int> getButtonArea(int index, int totalButtons);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfileSetupComponent)
};