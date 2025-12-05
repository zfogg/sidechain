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

    // Set local preview path while uploading
    void setLocalPreviewPath(const juce::String& localPath);

    // Set the profile picture URL (S3 URL) after successful upload
    void setProfilePictureUrl(const juce::String& s3Url);

    // Set the profile image directly (from UserDataStore's cached image)
    void setProfileImage(const juce::Image& image);

private:
    juce::String username;
    juce::String email;
    juce::String profilePicUrl;       // The S3 URL for the profile picture
    juce::String localPreviewPath;    // Local file path for preview while uploading
    juce::Image previewImage;         // Cached image for display
    
    // UI helper methods
    void drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds);
    juce::Rectangle<int> getButtonArea(int index, int totalButtons);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfileSetupComponent)
};