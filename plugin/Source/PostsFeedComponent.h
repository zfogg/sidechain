#pragma once

#include <JuceHeader.h>

class PostsFeedComponent : public juce::Component
{
public:
    PostsFeedComponent();
    ~PostsFeedComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

    // Callback for when user wants to go to profile
    std::function<void()> onGoToProfile;

    // Callback for logout
    std::function<void()> onLogout;

    // Callback for starting recording
    std::function<void()> onStartRecording;

    // Set user info
    void setUserInfo(const juce::String& username, const juce::String& email, const juce::String& profilePicUrl);

private:
    juce::String username;
    juce::String email;
    juce::String profilePicUrl;
    
    // UI helper methods
    void drawTopBar(juce::Graphics& g);
    void drawEmptyState(juce::Graphics& g);
    void drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds, bool small = false);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostsFeedComponent)
};