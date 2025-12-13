#include "SkeletonLoader.h"
#include "../../util/Colors.h"

//==============================================================================
// SkeletonLoader Base Class
//==============================================================================

SkeletonLoader::SkeletonLoader()
    : baseColor(SidechainColors::backgroundLight().darker(0.1f))
    , shimmerColor(SidechainColors::backgroundLight().brighter(0.15f))
{
}

SkeletonLoader::~SkeletonLoader()
{
    stopTimer();
}

void SkeletonLoader::paint(juce::Graphics& g)
{
    drawSkeletonShapes(g);
}

void SkeletonLoader::visibilityChanged()
{
    if (isVisible() && shimmerEnabled)
    {
        animationStartTime = juce::Time::currentTimeMillis();
        startTimerHz(60);  // 60 FPS for smooth animation
    }
    else
    {
        stopTimer();
    }
}

void SkeletonLoader::setShimmerEnabled(bool enabled)
{
    shimmerEnabled = enabled;
    if (enabled && isVisible())
    {
        animationStartTime = juce::Time::currentTimeMillis();
        startTimerHz(60);
    }
    else
    {
        stopTimer();
    }
    repaint();
}

void SkeletonLoader::timerCallback()
{
    auto elapsed = juce::Time::currentTimeMillis() - animationStartTime;
    shimmerProgress = static_cast<float>(elapsed % shimmerDurationMs) / shimmerDurationMs;
    repaint();
}

juce::Colour SkeletonLoader::getColorWithShimmer(juce::Rectangle<int> bounds) const
{
    if (!shimmerEnabled)
        return baseColor;

    // Calculate shimmer position (-0.3 to 1.3 for smooth entry/exit)
    float shimmerX = shimmerProgress * 1.6f - 0.3f;

    // Calculate normalized position of this element
    float elementCenterX = static_cast<float>(bounds.getCentreX()) / static_cast<float>(getWidth());

    // Distance from shimmer line (creates a gradient effect)
    float distance = std::abs(elementCenterX - shimmerX);

    // Shimmer width (how wide the highlight spreads)
    constexpr float shimmerWidth = 0.25f;

    if (distance < shimmerWidth)
    {
        // Blend between base and shimmer based on distance
        float blend = 1.0f - (distance / shimmerWidth);
        // Use smooth curve for nicer effect
        blend = blend * blend * (3.0f - 2.0f * blend);
        return baseColor.interpolatedWith(shimmerColor, blend * 0.7f);
    }

    return baseColor;
}

void SkeletonLoader::drawRect(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(getColorWithShimmer(bounds));
    g.fillRect(bounds);
}

void SkeletonLoader::drawRoundedRect(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(getColorWithShimmer(bounds));
    g.fillRoundedRectangle(bounds.toFloat(), cornerRadius);
}

void SkeletonLoader::drawCircle(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(getColorWithShimmer(bounds));
    g.fillEllipse(bounds.toFloat());
}

void SkeletonLoader::drawLine(juce::Graphics& g, juce::Rectangle<int> bounds, float widthPercent)
{
    auto lineBounds = bounds.withWidth(static_cast<int>(bounds.getWidth() * widthPercent));
    g.setColour(getColorWithShimmer(lineBounds));
    g.fillRoundedRectangle(lineBounds.toFloat(), 3.0f);
}

void SkeletonLoader::drawLines(juce::Graphics& g, juce::Rectangle<int> bounds, int lineCount, int lineHeight, int lineSpacing)
{
    int y = bounds.getY();
    for (int i = 0; i < lineCount; ++i)
    {
        // Vary line widths for natural look
        float widthPercent = (i == lineCount - 1) ? 0.6f : (0.8f + (i % 2) * 0.2f);
        auto lineBounds = juce::Rectangle<int>(bounds.getX(), y, bounds.getWidth(), lineHeight);
        drawLine(g, lineBounds, widthPercent);
        y += lineHeight + lineSpacing;
    }
}

//==============================================================================
// PostCardSkeleton
//==============================================================================

void PostCardSkeleton::resized()
{
    // Nothing to do - shapes are drawn relative to bounds
}

void PostCardSkeleton::drawSkeletonShapes(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(12);

    // Background card
    g.setColour(SidechainColors::backgroundLight());
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    // Header section (avatar + username + time)
    auto headerBounds = bounds.removeFromTop(48);

    // Avatar (circle)
    auto avatarBounds = headerBounds.removeFromLeft(40);
    avatarBounds = avatarBounds.withSizeKeepingCentre(40, 40);
    drawCircle(g, avatarBounds);

    headerBounds.removeFromLeft(12);

    // Username line
    auto usernameBounds = headerBounds.removeFromTop(16);
    usernameBounds = usernameBounds.withWidth(120);
    drawLine(g, usernameBounds, 1.0f);

    // Time line
    headerBounds.removeFromTop(6);
    auto timeBounds = headerBounds.removeFromTop(12);
    timeBounds = timeBounds.withWidth(60);
    drawLine(g, timeBounds, 1.0f);

    bounds.removeFromTop(12);

    // Waveform area
    auto waveformBounds = bounds.removeFromTop(80);
    drawRoundedRect(g, waveformBounds);

    bounds.removeFromTop(12);

    // Audio metadata (BPM, Key, Duration)
    auto metaBounds = bounds.removeFromTop(24);
    for (int i = 0; i < 3; ++i)
    {
        auto tagBounds = metaBounds.removeFromLeft(60);
        drawRoundedRect(g, tagBounds.withTrimmedRight(8));
    }

    bounds.removeFromTop(12);

    // Action buttons row (like, comment, share, save)
    auto actionBounds = bounds.removeFromTop(32);
    for (int i = 0; i < 4; ++i)
    {
        auto buttonBounds = actionBounds.removeFromLeft(40);
        buttonBounds = buttonBounds.withSizeKeepingCentre(24, 24);
        drawCircle(g, buttonBounds);
        actionBounds.removeFromLeft(16);
    }
}

//==============================================================================
// ProfileSkeleton
//==============================================================================

void ProfileSkeleton::drawSkeletonShapes(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(16);

    // Avatar (large circle in center)
    auto avatarSize = 80;
    auto avatarBounds = juce::Rectangle<int>(
        bounds.getCentreX() - avatarSize / 2,
        bounds.getY(),
        avatarSize, avatarSize
    );
    drawCircle(g, avatarBounds);

    bounds.removeFromTop(avatarSize + 12);

    // Display name
    auto nameBounds = bounds.removeFromTop(24).withSizeKeepingCentre(150, 20);
    drawLine(g, nameBounds, 1.0f);

    bounds.removeFromTop(8);

    // Username
    auto usernameBounds = bounds.removeFromTop(18).withSizeKeepingCentre(100, 14);
    drawLine(g, usernameBounds, 1.0f);

    bounds.removeFromTop(16);

    // Stats row (posts, followers, following)
    auto statsBounds = bounds.removeFromTop(50);
    int statWidth = statsBounds.getWidth() / 3;

    for (int i = 0; i < 3; ++i)
    {
        auto statBounds = statsBounds.removeFromLeft(statWidth);

        // Number
        auto numBounds = statBounds.removeFromTop(24).withSizeKeepingCentre(40, 20);
        drawLine(g, numBounds, 1.0f);

        statBounds.removeFromTop(4);

        // Label
        auto labelBounds = statBounds.removeFromTop(16).withSizeKeepingCentre(60, 14);
        drawLine(g, labelBounds, 1.0f);
    }

    bounds.removeFromTop(16);

    // Bio lines
    auto bioBounds = bounds.removeFromTop(60);
    drawLines(g, bioBounds, 3, 14, 8);

    bounds.removeFromTop(16);

    // Action buttons (Follow, Message)
    auto buttonBounds = bounds.removeFromTop(40);
    auto followBounds = buttonBounds.removeFromLeft(buttonBounds.getWidth() / 2 - 6).withHeight(36);
    drawRoundedRect(g, followBounds);

    buttonBounds.removeFromLeft(12);
    auto messageBounds = buttonBounds.withHeight(36);
    drawRoundedRect(g, messageBounds);
}

//==============================================================================
// StoryCircleSkeleton
//==============================================================================

void StoryCircleSkeleton::drawSkeletonShapes(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Outer ring
    auto ringBounds = bounds.reduced(2);
    g.setColour(getColorWithShimmer(ringBounds).withAlpha(0.3f));
    g.drawEllipse(ringBounds.toFloat(), 2.5f);

    // Inner avatar circle
    auto avatarBounds = bounds.reduced(6);
    drawCircle(g, avatarBounds);

    // Username label below
    if (bounds.getHeight() > 70)
    {
        auto labelBounds = juce::Rectangle<int>(
            bounds.getX() - 10,
            bounds.getBottom() + 4,
            bounds.getWidth() + 20,
            12
        );
        drawLine(g, labelBounds, 0.8f);
    }
}

//==============================================================================
// CommentSkeleton
//==============================================================================

void CommentSkeleton::drawSkeletonShapes(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(8);

    // Avatar
    auto avatarBounds = bounds.removeFromLeft(32);
    avatarBounds = avatarBounds.withSizeKeepingCentre(32, 32);
    drawCircle(g, avatarBounds);

    bounds.removeFromLeft(10);

    // Username
    auto userBounds = bounds.removeFromTop(14).withWidth(80);
    drawLine(g, userBounds, 1.0f);

    bounds.removeFromTop(6);

    // Comment text (2 lines)
    drawLines(g, bounds.withHeight(40), 2, 12, 6);
}

//==============================================================================
// FeedSkeleton
//==============================================================================

FeedSkeleton::FeedSkeleton(int cardCount)
{
    setCardCount(cardCount);
}

void FeedSkeleton::paint(juce::Graphics& g)
{
    g.fillAll(SidechainColors::background());
}

void FeedSkeleton::resized()
{
    auto bounds = getLocalBounds();

    for (auto* skeleton : skeletons)
    {
        auto cardBounds = bounds.removeFromTop(cardHeight);
        skeleton->setBounds(cardBounds);
        bounds.removeFromTop(cardSpacing);
    }
}

void FeedSkeleton::setCardCount(int count)
{
    skeletons.clear();

    for (int i = 0; i < count; ++i)
    {
        auto* skeleton = skeletons.add(new PostCardSkeleton());
        addAndMakeVisible(skeleton);
    }

    resized();
}
