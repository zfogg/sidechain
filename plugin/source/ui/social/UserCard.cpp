#include "UserCard.h"
#include "../../stores/ImageCache.h"
#include "../../util/UIHelpers.h"
#include "../../util/StringFormatter.h"
#include "../../util/HoverState.h"
#include "../../util/Log.h"

//==============================================================================
UserCard::UserCard()
{
    setInterceptsMouseClicks(true, true);

    // Set up hover state
    hoverState.onHoverChanged = [this](bool hovered) {
        repaint();
    };
}

UserCard::~UserCard()
{
}

//==============================================================================
void UserCard::setUser(const DiscoveredUser& newUser)
{
    user = newUser;
    avatarImage = juce::Image();
    Log::debug("UserCard: Setting user - ID: " + user.id + ", username: " + user.username);

    // Load avatar via ImageCache
    if (user.avatarUrl.isNotEmpty())
    {
        juce::Component::SafePointer<UserCard> safeThis(this);
        ::ImageLoader::load(user.avatarUrl, [safeThis](const juce::Image& img) {
            if (safeThis == nullptr) return;  // Component was deleted
            safeThis->avatarImage = img;
            safeThis->repaint();
        });
    }

    repaint();
}

void UserCard::setIsFollowing(bool following)
{
    if (user.isFollowing != following)
    {
        user.isFollowing = following;
        Log::debug("UserCard: Follow state changed - user: " + user.id + ", following: " + juce::String(following ? "true" : "false"));
        repaint();
    }
}

//==============================================================================
void UserCard::paint(juce::Graphics& g)
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

void UserCard::resized()
{
}

//==============================================================================
void UserCard::drawBackground(juce::Graphics& g)
{
    UIHelpers::drawCardWithHover(g, getLocalBounds().reduced(4, 2),
        Colors::background,
        Colors::backgroundHover,
        juce::Colours::transparentBlack,
        hoverState.isHovered());
}

void UserCard::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto avatarArea = bounds.withSizeKeepingCentre(AVATAR_SIZE, AVATAR_SIZE);

    // Use ImageUtils helper for avatar drawing with initials fallback
    ImageLoader::drawCircularAvatar(g, avatarArea, avatarImage,
        ImageLoader::getInitials(user.getDisplayNameOrUsername()),
        Colors::badge,
        Colors::textPrimary,
        16.0f);

    // Draw online indicator (green/cyan dot in bottom-right corner)
    if (user.isOnline || user.isInStudio)
    {
        const int indicatorSize = 14;
        const int borderWidth = 2;

        // Position at bottom-right of avatar
        auto indicatorBounds = juce::Rectangle<int>(
            avatarArea.getRight() - indicatorSize + 2,
            avatarArea.getBottom() - indicatorSize + 2,
            indicatorSize,
            indicatorSize
        ).toFloat();

        // Draw dark border (matches card background)
        g.setColour(Colors::background);
        g.fillEllipse(indicatorBounds);

        // Draw indicator (cyan for in_studio, green for just online)
        auto innerBounds = indicatorBounds.reduced(borderWidth);
        g.setColour(user.isInStudio ? Colors::inStudioIndicator : Colors::onlineIndicator);
        g.fillEllipse(innerBounds);
    }
}

void UserCard::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
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

        subtitle += StringFormatter::formatFollowers(user.followerCount);
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

void UserCard::drawGenreBadge(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (user.genre.isEmpty())
        return;

    UIHelpers::drawPillBadge(g, bounds.getX(), bounds.getY() + 1,
        user.genre, Colors::badge, Colors::textSecondary, 10.0f, 6, 4);
}

void UserCard::drawFollowButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto buttonBounds = bounds.withSizeKeepingCentre(72, 28);

    if (user.isFollowing)
    {
        // "Following" button (muted)
        UIHelpers::drawButton(g, buttonBounds, "Following",
            Colors::followingButton, Colors::textSecondary, false, 14.0f);
    }
    else
    {
        // "Follow" button (accent)
        g.setFont(juce::Font(11.0f).boldened());
        UIHelpers::drawButton(g, buttonBounds, "Follow",
            Colors::followButton, juce::Colours::black, false, 14.0f);
    }
}

//==============================================================================
juce::Rectangle<int> UserCard::getAvatarBounds() const
{
    return getLocalBounds().reduced(8, 4).removeFromLeft(AVATAR_SIZE);
}

juce::Rectangle<int> UserCard::getUserInfoBounds() const
{
    auto bounds = getLocalBounds().reduced(8, 4);
    bounds.removeFromLeft(AVATAR_SIZE + 12);
    bounds.removeFromRight(80);
    return bounds;
}

juce::Rectangle<int> UserCard::getFollowButtonBounds() const
{
    auto bounds = getLocalBounds().reduced(8, 4);
    return bounds.removeFromRight(80).withSizeKeepingCentre(72, 28);
}

//==============================================================================
void UserCard::mouseUp(const juce::MouseEvent& event)
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

void UserCard::mouseEnter(const juce::MouseEvent&)
{
    hoverState.setHovered(true);
}

void UserCard::mouseExit(const juce::MouseEvent&)
{
    hoverState.setHovered(false);
}
