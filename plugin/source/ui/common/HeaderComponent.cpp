#include "HeaderComponent.h"
#include "../../util/Colors.h"
#include "../../util/ImageCache.h"
#include "../../util/Async.h"
#include "../../util/Validate.h"

//==============================================================================
HeaderComponent::HeaderComponent()
{
    setSize(1000, HEADER_HEIGHT);
}

//==============================================================================
void HeaderComponent::setUserInfo(const juce::String& user, const juce::String& picUrl)
{
    username = user;

    // Only reload image if URL changed and we don't already have a cached image
    if (profilePicUrl != picUrl)
    {
        profilePicUrl = picUrl;

        // Only download if we don't have an image and URL is valid
        if (!cachedProfileImage.isValid() && Validate::isUrl(profilePicUrl))
        {
            loadProfileImage(profilePicUrl);
        }
    }

    repaint();
}

void HeaderComponent::setProfileImage(const juce::Image& image)
{
    cachedProfileImage = image;
    repaint();
}

void HeaderComponent::loadProfileImage(const juce::String& url)
{
    auto urlObj = juce::URL(url);

    // Use Async::run to download image on background thread
    Async::run<juce::Image>(
        // Background work: download image
        [urlObj]() -> juce::Image {
            auto inputStream = urlObj.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(5000)
                    .withNumRedirectsToFollow(5));

            if (inputStream != nullptr)
            {
                juce::MemoryBlock imageData;
                inputStream->readIntoMemoryBlock(imageData);
                return juce::ImageFileFormat::loadFrom(imageData.getData(), imageData.getSize());
            }

            return {};
        },
        // Callback on message thread: update UI
        [this](const juce::Image& image) {
            if (image.isValid())
            {
                cachedProfileImage = image;
                DBG("HeaderComponent - loaded profile image");
                repaint();
            }
        }
    );
}

//==============================================================================
void HeaderComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(SidechainColors::backgroundLight());
    g.fillRect(bounds);

    // Draw sections
    drawLogo(g, getLogoBounds());
    drawSearchButton(g, getSearchButtonBounds());
    drawProfileSection(g, getProfileBounds());

    // Bottom border
    g.setColour(SidechainColors::border());
    g.drawLine(0.0f, static_cast<float>(bounds.getBottom() - 1),
               static_cast<float>(bounds.getWidth()), static_cast<float>(bounds.getBottom() - 1), 1.0f);
}

void HeaderComponent::resized()
{
    // Layout is handled in getBounds methods
}

void HeaderComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    if (getLogoBounds().contains(pos))
    {
        if (onLogoClicked)
            onLogoClicked();
    }
    else if (getSearchButtonBounds().contains(pos))
    {
        if (onSearchClicked)
            onSearchClicked();
    }
    else if (getProfileBounds().contains(pos))
    {
        if (onProfileClicked)
            onProfileClicked();
    }
}

//==============================================================================
void HeaderComponent::drawLogo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(20.0f).boldened());
    g.drawText("Sidechain", bounds, juce::Justification::centredLeft);
}

void HeaderComponent::drawSearchButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Search button background
    g.setColour(SidechainColors::surface());
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

    // Search icon and placeholder text
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    g.drawText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x8D")) + " Search users...",
               bounds, juce::Justification::centred);
}

void HeaderComponent::drawProfileSection(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Profile picture
    auto picBounds = juce::Rectangle<int>(bounds.getX(), bounds.getCentreY() - 18, 36, 36);
    drawCircularProfilePic(g, picBounds);

    // Username
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    auto textBounds = bounds.withX(picBounds.getRight() + 8).withWidth(bounds.getWidth() - 44);
    g.drawText(username, textBounds, juce::Justification::centredLeft);
}

void HeaderComponent::drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    ImageLoader::drawCircularAvatar(g, bounds, cachedProfileImage,
        ImageLoader::getInitials(username),
        SidechainColors::primary(),
        SidechainColors::textPrimary(),
        14.0f);

    // Border
    g.setColour(SidechainColors::border());
    g.drawEllipse(bounds.toFloat().reduced(0.5f), 1.0f);
}

//==============================================================================
juce::Rectangle<int> HeaderComponent::getLogoBounds() const
{
    return juce::Rectangle<int>(20, 0, 120, getHeight());
}

juce::Rectangle<int> HeaderComponent::getSearchButtonBounds() const
{
    int buttonWidth = 220;
    int buttonHeight = 36;
    int x = (getWidth() - buttonWidth) / 2;
    int y = (getHeight() - buttonHeight) / 2;
    return juce::Rectangle<int>(x, y, buttonWidth, buttonHeight);
}

juce::Rectangle<int> HeaderComponent::getProfileBounds() const
{
    return juce::Rectangle<int>(getWidth() - 160, 0, 140, getHeight());
}
