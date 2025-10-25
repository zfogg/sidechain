#include "ProfileSetupComponent.h"

ProfileSetupComponent::ProfileSetupComponent()
{
    setSize(1000, 800);
}

ProfileSetupComponent::~ProfileSetupComponent()
{
}

void ProfileSetupComponent::setUserInfo(const juce::String& user, const juce::String& userEmail, const juce::String& picUrl)
{
    username = user;
    email = userEmail;
    profilePicUrl = picUrl;
    DBG("ProfileSetup - setUserInfo: " + user + ", " + userEmail + ", profilePicUrl: " + profilePicUrl);
    repaint();
}

void ProfileSetupComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour::fromRGB(25, 25, 25));

    // Logout button at top-right
    auto logoutBtnBounds = juce::Rectangle<int>(getWidth() - 150, 10, 140, 40);
    g.setColour(juce::Colour::fromRGB(180, 50, 50));
    g.fillRoundedRectangle(logoutBtnBounds.toFloat(), 6.0f);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Logout", logoutBtnBounds, juce::Justification::centred);

    // Header
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("Complete Your Profile", getLocalBounds().withY(60).withHeight(40), juce::Justification::centred);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(16.0f);
    g.drawText("Welcome " + username + "! Let's set up your profile.", getLocalBounds().withY(110).withHeight(30), juce::Justification::centred);
    
    // Profile picture area (circular placeholder)
    auto picBounds = juce::Rectangle<int>(200, 140, 150, 150);
    drawCircularProfilePic(g, picBounds);
    
    // Buttons positioned to the right of the profile picture
    // Upload button
    auto uploadBtn = juce::Rectangle<int>(400, 150, 150, 36);
    g.setColour(juce::Colour::fromRGB(0, 212, 255));
    g.fillRoundedRectangle(uploadBtn.toFloat(), 6.0f);
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("📸 Upload Photo", uploadBtn, juce::Justification::centred);

    // Skip button
    auto skipBtn = juce::Rectangle<int>(400, 196, 70, 32);
    g.setColour(juce::Colour::fromRGB(108, 117, 125));
    g.fillRoundedRectangle(skipBtn.toFloat(), 4.0f);
    g.setColour(juce::Colours::white);
    g.drawText("Skip", skipBtn, juce::Justification::centred);

    // Continue button
    auto continueBtn = juce::Rectangle<int>(480, 196, 70, 32);
    g.setColour(juce::Colour::fromRGB(40, 167, 69));
    g.fillRoundedRectangle(continueBtn.toFloat(), 4.0f);
    g.setColour(juce::Colours::white);
    g.drawText("Continue", continueBtn, juce::Justification::centred);
}

void ProfileSetupComponent::drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Save graphics state before clipping
    juce::Graphics::ScopedSaveState saveState(g);
    
    // Create circular clipping path
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());
    g.reduceClipRegion(circlePath);
    
    if (profilePicUrl.isEmpty())
    {
        // Default profile picture placeholder
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.fillEllipse(bounds.toFloat());
        
        // Add a person icon or initials
        g.setColour(juce::Colour::fromRGB(120, 120, 120));
        g.setFont(36.0f);
        juce::String initials = username.substring(0, 2).toUpperCase();
        g.drawText(initials, bounds, juce::Justification::centred);
    }
    else
    {
        // Load and draw actual profile picture
        juce::File imageFile(profilePicUrl);
        if (imageFile.existsAsFile())
        {
            juce::Image profileImage = juce::ImageFileFormat::loadFrom(imageFile);
            if (profileImage.isValid())
            {
                // Scale image to fit circle
                auto scaledImage = profileImage.rescaled(bounds.getWidth(), bounds.getHeight(), juce::Graphics::highResamplingQuality);
                g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
            }
            else
            {
                // Fallback if image failed to load
                g.setColour(juce::Colour::fromRGB(150, 150, 150));
                g.fillEllipse(bounds.toFloat());
                g.setColour(juce::Colours::white);
                g.setFont(28.0f);
                g.drawText("?", bounds, juce::Justification::centred);
            }
        }
        else
        {
            // Fallback if file doesn't exist
            g.setColour(juce::Colour::fromRGB(100, 100, 100));
            g.fillEllipse(bounds.toFloat());
            g.setColour(juce::Colours::white);
            g.setFont(28.0f);
            g.drawText("!", bounds, juce::Justification::centred);
        }
    }
    
    // Border
    g.setColour(juce::Colour::fromRGB(200, 200, 200));
    g.drawEllipse(bounds.toFloat(), 2.0f);
}

void ProfileSetupComponent::resized()
{
    // Component layout will be handled in paint method
}

void ProfileSetupComponent::mouseUp(const juce::MouseEvent& event)
{
    // Use same coordinates as paint method
    auto uploadBtn = juce::Rectangle<int>(400, 150, 150, 36);
    auto skipBtn = juce::Rectangle<int>(400, 196, 70, 32);
    auto continueBtn = juce::Rectangle<int>(480, 196, 70, 32);
    auto picBounds = juce::Rectangle<int>(200, 140, 150, 150);
    auto logoutBtn = juce::Rectangle<int>(getWidth() - 150, 10, 140, 40);
    
    if (uploadBtn.contains(event.getPosition()) || picBounds.contains(event.getPosition()))
    {
        // Open file picker for profile picture
        auto chooser = std::make_shared<juce::FileChooser>("Select Profile Picture", juce::File(), "*.jpg;*.jpeg;*.png;*.gif");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                           [this, chooser](const juce::FileChooser&)
        {
            auto selectedFile = chooser->getResult();
            if (selectedFile.existsAsFile())
            {
                // Update local profilePicUrl and notify parent
                profilePicUrl = selectedFile.getFullPathName(); // Temporary - will be S3 URL
                DBG("Profile picture selected: " + profilePicUrl);
                
                // Notify parent component
                if (onProfilePicSelected)
                    onProfilePicSelected(profilePicUrl);
                    
                repaint();
            }
        });
    }
    else if (skipBtn.contains(event.getPosition()) || continueBtn.contains(event.getPosition()))
    {
        // Both skip and continue go to the feed
        if (skipBtn.contains(event.getPosition()) && onSkipSetup)
            onSkipSetup();
        else if (continueBtn.contains(event.getPosition()) && onCompleteSetup)
            onCompleteSetup();
    }
    else if (logoutBtn.contains(event.getPosition()))
    {
        DBG("Logout button clicked");
        if (onLogout)
            onLogout();
    }
}

juce::Rectangle<int> ProfileSetupComponent::getButtonArea(int index, int totalButtons)
{
    const int buttonWidth = 200;
    const int buttonHeight = 40;
    const int spacing = 10;
    
    int totalWidth = totalButtons * buttonWidth + (totalButtons - 1) * spacing;
    int startX = (getWidth() - totalWidth) / 2;
    
    return juce::Rectangle<int>(startX + index * (buttonWidth + spacing), 0, buttonWidth, buttonHeight);
}