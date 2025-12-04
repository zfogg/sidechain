#include "UserCardComponent.h"

//==============================================================================
UserCardComponent::UserCardComponent()
{
    setInterceptsMouseClicks(true, true);
}

UserCardComponent::~UserCardComponent()
{
}

//==============================================================================
void UserCardComponent::setUser(const DiscoveredUser& newUser)
{
    user = newUser;
    avatarImage = juce::Image();
    avatarLoadRequested = false;
    repaint();
}

void UserCardComponent::setIsFollowing(bool following)
{
    if (user.isFollowing != following)
    {
        user.isFollowing = following;
        repaint();
    }
}

//==============================================================================
void UserCardComponent::paint(juce::Graphics& g)
{
    drawBackground(g);

    auto bounds = getLocalBounds().reduced(8, 4);

    // Layout: Avatar | User Info | Follow Button
    auto avatarBounds = bounds.removeFromLeft(AVATAR_SIZE);
    bounds.removeFromLeft(12);  // spacing

    auto followBounds = bounds.removeFromRight(80);
    auto infoBounds = bounds;

    drawAvatar(g, avatarBounds);
    drawUserInfo(g, infoBounds);
    drawFollowButton(g, followBounds);
}

void UserCardComponent::resized()
{
}

//==============================================================================
void UserCardComponent::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(isHovered ? Colors::backgroundHover : Colors::background);
    g.fillRoundedRectangle(bounds.reduced(4, 2), 8.0f);
}

void UserCardComponent::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto avatarArea = bounds.withSizeKeepingCentre(AVATAR_SIZE, AVATAR_SIZE);
    float cornerRadius = AVATAR_SIZE / 2.0f;

    if (avatarImage.isValid())
    {
        // Draw the loaded avatar image
        juce::Path clipPath;
        clipPath.addEllipse(avatarArea.toFloat());
        g.saveState();
        g.reduceClipRegion(clipPath);
        g.drawImage(avatarImage, avatarArea.toFloat());
        g.restoreState();
    }
    else
    {
        // Draw placeholder with initials
        g.setColour(Colors::badge);
        g.fillEllipse(avatarArea.toFloat());

        // Draw initials
        juce::String initials;
        auto name = user.getDisplayNameOrUsername();
        if (name.isNotEmpty())
        {
            auto words = juce::StringArray::fromTokens(name, " ", "");
            for (auto& word : words)
            {
                if (word.isNotEmpty() && initials.length() < 2)
                    initials += word.substring(0, 1).toUpperCase();
            }
        }
        if (initials.isEmpty())
            initials = "?";

        g.setColour(Colors::textPrimary);
        g.setFont(juce::Font(16.0f).boldened());
        g.drawText(initials, avatarArea, juce::Justification::centred);

        // Request avatar load if we have a URL
        if (!avatarLoadRequested && user.avatarUrl.isNotEmpty())
        {
            avatarLoadRequested = true;
            // Note: In a real implementation, use an image cache/loader
            // For now, avatars will show initials
        }
    }
}

void UserCardComponent::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Display name (bold)
    auto displayName = user.getDisplayNameOrUsername();
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(15.0f).boldened());

    auto nameBounds = bounds.removeFromTop(20);
    g.drawText(displayName, nameBounds, juce::Justification::centredLeft, true);

    // Username (if different from display name) and follower count
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(12.0f));

    juce::String subtitle;
    if (!user.displayName.isEmpty() && user.displayName != user.username)
    {
        subtitle = "@" + user.username;
    }

    // Add follower count
    if (user.followerCount > 0)
    {
        if (subtitle.isNotEmpty())
            subtitle += " · ";

        if (user.followerCount >= 1000)
            subtitle += juce::String(user.followerCount / 1000.0f, 1) + "K followers";
        else
            subtitle += juce::String(user.followerCount) + " followers";
    }

    auto subtitleBounds = bounds.removeFromTop(16);
    g.drawText(subtitle, subtitleBounds, juce::Justification::centredLeft, true);

    // Genre badge (if available)
    if (user.genre.isNotEmpty())
    {
        auto genreBounds = bounds.removeFromTop(20);
        drawGenreBadge(g, genreBounds);
    }
}

void UserCardComponent::drawGenreBadge(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (user.genre.isEmpty())
        return;

    g.setFont(juce::Font(10.0f));
    auto textWidth = g.getCurrentFont().getStringWidth(user.genre);

    auto badgeBounds = bounds.removeFromLeft(textWidth + 12).withHeight(18);
    badgeBounds.setY(bounds.getY() + 1);

    g.setColour(Colors::badge);
    g.fillRoundedRectangle(badgeBounds.toFloat(), 9.0f);

    g.setColour(Colors::textSecondary);
    g.drawText(user.genre, badgeBounds, juce::Justification::centred);
}

void UserCardComponent::drawFollowButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto buttonBounds = bounds.withSizeKeepingCentre(72, 28);

    if (user.isFollowing)
    {
        // "Following" button (muted)
        g.setColour(Colors::followingButton);
        g.fillRoundedRectangle(buttonBounds.toFloat(), 14.0f);

        g.setColour(Colors::textSecondary);
        g.setFont(juce::Font(11.0f));
        g.drawText("Following", buttonBounds, juce::Justification::centred);
    }
    else
    {
        // "Follow" button (accent)
        g.setColour(Colors::followButton);
        g.fillRoundedRectangle(buttonBounds.toFloat(), 14.0f);

        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(11.0f).boldened());
        g.drawText("Follow", buttonBounds, juce::Justification::centred);
    }
}

//==============================================================================
juce::Rectangle<int> UserCardComponent::getAvatarBounds() const
{
    return getLocalBounds().reduced(8, 4).removeFromLeft(AVATAR_SIZE);
}

juce::Rectangle<int> UserCardComponent::getUserInfoBounds() const
{
    auto bounds = getLocalBounds().reduced(8, 4);
    bounds.removeFromLeft(AVATAR_SIZE + 12);
    bounds.removeFromRight(80);
    return bounds;
}

juce::Rectangle<int> UserCardComponent::getFollowButtonBounds() const
{
    auto bounds = getLocalBounds().reduced(8, 4);
    return bounds.removeFromRight(80).withSizeKeepingCentre(72, 28);
}

//==============================================================================
void UserCardComponent::mouseUp(const juce::MouseEvent& event)
{
    auto point = event.getPosition();

    // Check follow button first
    if (getFollowButtonBounds().contains(point))
    {
        if (onFollowToggled)
            onFollowToggled(user, !user.isFollowing);
        return;
    }

    // Click anywhere else goes to profile
    if (onUserClicked)
        onUserClicked(user);
}

void UserCardComponent::mouseEnter(const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void UserCardComponent::mouseExit(const juce::MouseEvent&)
{
    isHovered = false;
    repaint();
}
