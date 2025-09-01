#include "PostsFeedComponent.h"

PostsFeedComponent::PostsFeedComponent()
{
    setSize(1000, 800);
}

PostsFeedComponent::~PostsFeedComponent()
{
}

void PostsFeedComponent::setUserInfo(const juce::String& user, const juce::String& userEmail, const juce::String& picUrl)
{
    username = user;
    email = userEmail;
    profilePicUrl = picUrl;
    repaint();
}

void PostsFeedComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour::fromRGB(25, 25, 25));
    
    // Top navigation bar
    drawTopBar(g);
    
    // Main feed area
    drawEmptyState(g);
}

void PostsFeedComponent::drawTopBar(juce::Graphics& g)
{
    auto topBarBounds = getLocalBounds().withHeight(70);
    
    // Top bar background
    g.setColour(juce::Colour::fromRGB(35, 35, 35));
    g.fillRect(topBarBounds);
    
    // App title
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("🎵 Sidechain", topBarBounds.withX(20).withWidth(200), juce::Justification::centredLeft);
    
    // Profile section (right side)
    auto profileBounds = topBarBounds.withX(getWidth() - 200).withWidth(180);
    
    // Small profile pic
    auto smallPicBounds = juce::Rectangle<int>(profileBounds.getX() + 10, profileBounds.getCentreY() - 20, 40, 40);
    drawCircularProfilePic(g, smallPicBounds, true);
    
    // Username
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(username, smallPicBounds.withX(smallPicBounds.getRight() + 10).withWidth(100), juce::Justification::centredLeft);
    
    // Border
    g.setColour(juce::Colour::fromRGB(60, 60, 60));
    g.drawLine(0, topBarBounds.getBottom(), getWidth(), topBarBounds.getBottom(), 1.0f);
}

void PostsFeedComponent::drawEmptyState(juce::Graphics& g)
{
    auto feedBounds = getLocalBounds().withTrimmedTop(70);
    
    // Empty state illustration
    auto centerBounds = feedBounds.withSizeKeepingCentre(400, 300);
    
    // Icon
    g.setColour(juce::Colour::fromRGB(100, 100, 100));
    g.setFont(48.0f);
    g.drawText("🎤", centerBounds.withHeight(80), juce::Justification::centred);
    
    // Main message
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("Welcome to Your Feed!", centerBounds.withY(centerBounds.getY() + 100).withHeight(40), juce::Justification::centred);
    
    // Subtitle
    g.setColour(juce::Colours::lightgrey);
    g.setFont(16.0f);
    g.drawText("Start creating music loops to share with the community.", 
               centerBounds.withY(centerBounds.getY() + 150).withHeight(30), juce::Justification::centred);
    
    g.drawText("Record from your DAW to get started!", 
               centerBounds.withY(centerBounds.getY() + 180).withHeight(30), juce::Justification::centred);
    
    // Action button
    auto actionBtn = juce::Rectangle<int>(centerBounds.getCentreX() - 100, centerBounds.getY() + 230, 200, 50);
    g.setColour(juce::Colour::fromRGB(0, 212, 255));
    g.fillRoundedRectangle(actionBtn.toFloat(), 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("🎵 Start Recording", actionBtn, juce::Justification::centred);
}

void PostsFeedComponent::drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds, bool small)
{
    // Create circular clipping path
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());
    
    juce::Graphics::ScopedSaveState saveState(g);
    g.reduceClipRegion(circlePath);
    
    if (profilePicUrl.isEmpty())
    {
        // Default profile picture placeholder
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.fillEllipse(bounds.toFloat());
        
        // Add initials
        g.setColour(juce::Colour::fromRGB(120, 120, 120));
        g.setFont(small ? 14.0f : 28.0f);
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
                // Fallback: show first letter
                g.setColour(juce::Colour::fromRGB(0, 150, 255));
                g.fillEllipse(bounds.toFloat());
                g.setColour(juce::Colours::white);
                g.setFont(small ? 14.0f : 28.0f);
                g.drawText(username.substring(0, 1).toUpperCase(), bounds, juce::Justification::centred);
            }
        }
        else
        {
            // Fallback: show first letter
            g.setColour(juce::Colour::fromRGB(0, 150, 255));
            g.fillEllipse(bounds.toFloat());
            g.setColour(juce::Colours::white);
            g.setFont(small ? 14.0f : 28.0f);
            g.drawText(username.substring(0, 1).toUpperCase(), bounds, juce::Justification::centred);
        }
    }
    
    // Border
    g.setColour(juce::Colour::fromRGB(200, 200, 200));
    g.drawEllipse(bounds.toFloat(), small ? 1.0f : 2.0f);
}

void PostsFeedComponent::resized()
{
    // Component layout handled in paint method
}

void PostsFeedComponent::mouseUp(const juce::MouseEvent& event)
{
    // Check if clicked on profile area in top bar
    auto topBarBounds = getLocalBounds().withHeight(70);
    auto profileBounds = topBarBounds.withX(getWidth() - 200).withWidth(180);
    
    if (profileBounds.contains(event.getPosition()))
    {
        if (onGoToProfile)
            onGoToProfile();
    }
    
    // Check if clicked on "Start Recording" button
    auto feedBounds = getLocalBounds().withTrimmedTop(70);
    auto centerBounds = feedBounds.withSizeKeepingCentre(400, 300);
    auto actionBtn = juce::Rectangle<int>(centerBounds.getCentreX() - 100, centerBounds.getY() + 230, 200, 50);
    
    if (actionBtn.contains(event.getPosition()))
    {
        // TODO: Start recording functionality
        DBG("Start recording clicked!");
    }
}