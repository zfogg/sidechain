#include "PostCardComponent.h"

//==============================================================================
PostCardComponent::PostCardComponent()
{
    setSize(600, CARD_HEIGHT);
}

PostCardComponent::~PostCardComponent()
{
}

//==============================================================================
void PostCardComponent::setPost(const FeedPost& newPost)
{
    post = newPost;
    avatarLoadRequested = false;
    avatarImage = juce::Image();
    repaint();
}

void PostCardComponent::updateLikeCount(int count, bool liked)
{
    post.likeCount = count;
    post.isLiked = liked;
    repaint();
}

void PostCardComponent::updatePlayCount(int count)
{
    post.playCount = count;
    repaint();
}

void PostCardComponent::updateFollowState(bool following)
{
    post.isFollowing = following;
    repaint();
}

void PostCardComponent::updateReaction(const juce::String& emoji)
{
    post.userReaction = emoji;
    if (emoji.isNotEmpty())
    {
        post.isLiked = true;  // Reacting also counts as a like
    }
    repaint();
}

void PostCardComponent::setPlaybackProgress(float progress)
{
    playbackProgress = juce::jlimit(0.0f, 1.0f, progress);
    repaint();
}

void PostCardComponent::setIsPlaying(bool playing)
{
    isPlaying = playing;
    repaint();
}

void PostCardComponent::setLoading(bool loading)
{
    isLoading = loading;
    repaint();
}

//==============================================================================
void PostCardComponent::paint(juce::Graphics& g)
{
    drawBackground(g);
    drawAvatar(g, getAvatarBounds());
    drawUserInfo(g, getUserInfoBounds());
    drawFollowButton(g, getFollowButtonBounds());
    drawWaveform(g, getWaveformBounds());
    drawPlayButton(g, getPlayButtonBounds());
    drawMetadataBadges(g, juce::Rectangle<int>(getWidth() - 120, 15, 110, CARD_HEIGHT - 30));
    drawSocialButtons(g, juce::Rectangle<int>(getWidth() - 120, CARD_HEIGHT - 40, 110, 30));

    // Draw like animation on top of everything
    drawLikeAnimation(g);
}

void PostCardComponent::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Card background
    auto bgColor = isHovered ? juce::Colour::fromRGB(50, 50, 50) : juce::Colour::fromRGB(40, 40, 40);
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    g.setColour(juce::Colour::fromRGB(60, 60, 60));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
}

void PostCardComponent::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Create circular clipping
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());

    g.saveState();
    g.reduceClipRegion(circlePath);

    if (avatarImage.isValid())
    {
        // Draw loaded avatar image
        auto scaledImage = avatarImage.rescaled(bounds.getWidth(), bounds.getHeight(), juce::Graphics::highResamplingQuality);
        g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
    }
    else
    {
        // Draw placeholder with initial
        g.setColour(juce::Colour::fromRGB(70, 70, 70));
        g.fillEllipse(bounds.toFloat());

        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        juce::String initial = post.username.isEmpty() ? "?" : post.username.substring(0, 1).toUpperCase();
        g.drawText(initial, bounds, juce::Justification::centred);
    }

    g.restoreState();

    // Avatar border
    g.setColour(juce::Colour::fromRGB(100, 100, 100));
    g.drawEllipse(bounds.toFloat(), 1.0f);
}

void PostCardComponent::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Username
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(post.username.isEmpty() ? "Unknown" : post.username,
               bounds.getX(), bounds.getY(), bounds.getWidth(), 20,
               juce::Justification::centredLeft);

    // Timestamp
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText(post.timeAgo,
               bounds.getX(), bounds.getY() + 20, bounds.getWidth(), 18,
               juce::Justification::centredLeft);

    // DAW badge if present
    if (post.daw.isNotEmpty())
    {
        g.setColour(juce::Colour::fromRGB(80, 80, 80));
        g.setFont(10.0f);
        g.drawText(post.daw,
                   bounds.getX(), bounds.getY() + 40, bounds.getWidth(), 15,
                   juce::Justification::centredLeft);
    }
}

void PostCardComponent::drawFollowButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Don't show follow button for own posts
    if (post.isOwnPost)
        return;

    // Button text based on follow state
    juce::String buttonText = post.isFollowing ? "Following" : "Follow";

    // Colors based on state
    juce::Colour bgColor, textColor, borderColor;
    if (post.isFollowing)
    {
        // Following state: subtle outline button
        bgColor = juce::Colour::fromRGBA(0, 0, 0, 0);
        textColor = juce::Colour::fromRGB(150, 150, 150);
        borderColor = juce::Colour::fromRGB(80, 80, 80);
    }
    else
    {
        // Not following: prominent filled button
        bgColor = juce::Colour::fromRGB(0, 150, 255);
        textColor = juce::Colours::white;
        borderColor = juce::Colour::fromRGB(0, 150, 255);
    }

    // Draw button background
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Draw border
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);

    // Draw text
    g.setColour(textColor);
    g.setFont(11.0f);
    g.drawText(buttonText, bounds, juce::Justification::centred);
}

void PostCardComponent::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Waveform background
    g.setColour(juce::Colour::fromRGB(50, 50, 50));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Generate deterministic waveform based on post ID
    int barWidth = 3;
    int barSpacing = 2;
    int numBars = bounds.getWidth() / (barWidth + barSpacing);

    // Draw waveform bars
    for (int i = 0; i < numBars; ++i)
    {
        float barProgress = static_cast<float>(i) / static_cast<float>(numBars);
        int barHeight = 5 + (std::hash<int>{}(post.id.hashCode() + i) % 25);
        int barX = bounds.getX() + i * (barWidth + barSpacing);
        int barY = bounds.getCentreY() - barHeight / 2;

        // Color based on playback progress
        if (barProgress <= playbackProgress)
        {
            g.setColour(juce::Colour::fromRGB(0, 212, 255)); // Played portion
        }
        else
        {
            g.setColour(juce::Colour::fromRGB(0, 140, 180)); // Unplayed portion
        }

        g.fillRect(barX, barY, barWidth, barHeight);
    }

    // Duration overlay at bottom-right of waveform
    if (post.durationSeconds > 0)
    {
        int seconds = static_cast<int>(post.durationSeconds);
        int mins = seconds / 60;
        int secs = seconds % 60;
        juce::String duration = juce::String::formatted("%d:%02d", mins, secs);

        auto durationBounds = juce::Rectangle<int>(bounds.getRight() - 45, bounds.getBottom() - 18, 40, 16);
        g.setColour(juce::Colour::fromRGBA(0, 0, 0, 180));
        g.fillRoundedRectangle(durationBounds.toFloat(), 3.0f);

        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText(duration, durationBounds, juce::Justification::centred);
    }
}

void PostCardComponent::drawPlayButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Semi-transparent circle background
    g.setColour(juce::Colour::fromRGBA(0, 0, 0, 150));
    g.fillEllipse(bounds.toFloat());

    // Play/pause icon
    g.setColour(juce::Colours::white);

    if (isPlaying)
    {
        // Pause icon (two vertical bars)
        int barWidth = 4;
        int barHeight = 14;
        int gap = 4;
        int startX = bounds.getCentreX() - (barWidth + gap / 2);
        int startY = bounds.getCentreY() - barHeight / 2;

        g.fillRect(startX, startY, barWidth, barHeight);
        g.fillRect(startX + barWidth + gap, startY, barWidth, barHeight);
    }
    else
    {
        // Play icon (triangle)
        juce::Path triangle;
        float cx = static_cast<float>(bounds.getCentreX());
        float cy = static_cast<float>(bounds.getCentreY());
        float size = 10.0f;

        // Slightly offset to right for visual centering
        triangle.addTriangle(cx - size * 0.4f, cy - size,
                            cx - size * 0.4f, cy + size,
                            cx + size * 0.8f, cy);
        g.fillPath(triangle);
    }

    // Border
    g.setColour(juce::Colour::fromRGBA(255, 255, 255, 100));
    g.drawEllipse(bounds.toFloat(), 1.0f);
}

void PostCardComponent::drawMetadataBadges(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int badgeY = bounds.getY();

    // BPM badge
    if (post.bpm > 0)
    {
        auto bpmBounds = juce::Rectangle<int>(bounds.getX(), badgeY, 55, BADGE_HEIGHT);
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.fillRoundedRectangle(bpmBounds.toFloat(), 4.0f);

        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        g.drawText(juce::String(post.bpm) + " BPM", bpmBounds, juce::Justification::centred);

        badgeY += BADGE_HEIGHT + 5;
    }

    // Key badge
    if (post.key.isNotEmpty())
    {
        auto keyBounds = juce::Rectangle<int>(bounds.getX(), badgeY, 55, BADGE_HEIGHT);
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.fillRoundedRectangle(keyBounds.toFloat(), 4.0f);

        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        g.drawText(post.key, keyBounds, juce::Justification::centred);

        badgeY += BADGE_HEIGHT + 5;
    }

    // Genre badges (first two)
    for (int i = 0; i < juce::jmin(2, post.genres.size()); ++i)
    {
        auto genreBounds = juce::Rectangle<int>(bounds.getX(), badgeY, bounds.getWidth(), BADGE_HEIGHT - 4);
        g.setColour(juce::Colour::fromRGB(50, 50, 50));
        g.fillRoundedRectangle(genreBounds.toFloat(), 3.0f);

        g.setColour(juce::Colour::fromRGB(150, 150, 150));
        g.setFont(10.0f);
        g.drawText(post.genres[i], genreBounds, juce::Justification::centred);

        badgeY += BADGE_HEIGHT;
    }
}

void PostCardComponent::drawSocialButtons(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Like/Reaction button
    auto likeBounds = getLikeButtonBounds();

    // Show user's reaction emoji if they've reacted, otherwise show heart
    if (post.userReaction.isNotEmpty())
    {
        // Show the emoji the user reacted with
        g.setFont(16.0f);
        g.setColour(juce::Colours::white);
        g.drawText(post.userReaction, likeBounds.withWidth(22), juce::Justification::centred);
    }
    else
    {
        // Show heart icon
        juce::Colour likeColor = post.isLiked ? juce::Colour::fromRGB(255, 80, 80) : juce::Colours::grey;
        g.setColour(likeColor);
        g.setFont(14.0f);
        juce::String heartIcon = post.isLiked ? juce::String(juce::CharPointer_UTF8("\xE2\x99\xA5")) : juce::String(juce::CharPointer_UTF8("\xE2\x99\xA1"));
        g.drawText(heartIcon, likeBounds.withWidth(20), juce::Justification::centred);
    }

    // Like/reaction count
    g.setColour(post.isLiked || post.userReaction.isNotEmpty() ? juce::Colour::fromRGB(255, 80, 80) : juce::Colours::grey);
    g.setFont(11.0f);
    g.drawText(juce::String(post.likeCount),
               likeBounds.withX(likeBounds.getX() + 20).withWidth(30),
               juce::Justification::centredLeft);

    // Comment count
    auto commentBounds = getCommentButtonBounds();
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("💬", commentBounds.withWidth(20), juce::Justification::centred);

    g.setFont(11.0f);
    g.drawText(juce::String(post.commentCount),
               commentBounds.withX(commentBounds.getX() + 18).withWidth(25),
               juce::Justification::centredLeft);

    // Play count (views)
    g.setColour(juce::Colour::fromRGB(100, 100, 100));
    g.setFont(10.0f);
    g.drawText(juce::String(post.playCount) + " plays",
               bounds.getX(), bounds.getY() - 15, 60, 12,
               juce::Justification::centredLeft);
}

//==============================================================================
void PostCardComponent::resized()
{
    // Layout is handled in paint() using bounds calculations
}

void PostCardComponent::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check if pressing on the like button area - start long-press timer
    if (getLikeButtonBounds().contains(pos))
    {
        longPressActive = true;
        longPressPosition = pos;
        longPressStartTime = juce::Time::getMillisecondCounter();
        startTimerHz(30);  // Check every ~33ms
    }
}

void PostCardComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Cancel long-press timer
    bool wasLongPress = longPressActive &&
        (juce::Time::getMillisecondCounter() - longPressStartTime >= LONG_PRESS_DURATION_MS);
    longPressActive = false;

    // Check play button
    if (getPlayButtonBounds().contains(pos))
    {
        if (isPlaying)
        {
            if (onPauseClicked)
                onPauseClicked(post);
        }
        else
        {
            if (onPlayClicked)
                onPlayClicked(post);
        }
        return;
    }

    // Check like button - only handle as click if not a long-press
    if (getLikeButtonBounds().contains(pos) && !wasLongPress)
    {
        bool willBeLiked = !post.isLiked;

        // Trigger animation when liking (not when unliking)
        if (willBeLiked)
            startLikeAnimation();

        if (onLikeToggled)
            onLikeToggled(post, willBeLiked);
        return;
    }

    // Check comment button
    if (getCommentButtonBounds().contains(pos))
    {
        if (onCommentClicked)
            onCommentClicked(post);
        return;
    }

    // Check share button
    if (getShareButtonBounds().contains(pos))
    {
        if (onShareClicked)
            onShareClicked(post);
        return;
    }

    // Check more button
    if (getMoreButtonBounds().contains(pos))
    {
        if (onMoreClicked)
            onMoreClicked(post);
        return;
    }

    // Check follow button (only if not own post)
    if (!post.isOwnPost && getFollowButtonBounds().contains(pos))
    {
        bool willFollow = !post.isFollowing;
        if (onFollowToggled)
            onFollowToggled(post, willFollow);
        return;
    }

    // Check avatar/user area
    if (getAvatarBounds().contains(pos) || getUserInfoBounds().contains(pos))
    {
        if (onUserClicked)
            onUserClicked(post);
        return;
    }

    // Check waveform area (for seeking)
    auto waveformBounds = getWaveformBounds();
    if (waveformBounds.contains(pos))
    {
        float seekPosition = static_cast<float>(pos.x - waveformBounds.getX()) / static_cast<float>(waveformBounds.getWidth());
        if (onWaveformClicked)
            onWaveformClicked(post, seekPosition);
        return;
    }
}

void PostCardComponent::mouseEnter(const juce::MouseEvent& /*event*/)
{
    isHovered = true;
    repaint();
}

void PostCardComponent::mouseExit(const juce::MouseEvent& /*event*/)
{
    isHovered = false;
    repaint();
}

//==============================================================================
juce::Rectangle<int> PostCardComponent::getAvatarBounds() const
{
    return juce::Rectangle<int>(15, (CARD_HEIGHT - AVATAR_SIZE) / 2, AVATAR_SIZE, AVATAR_SIZE);
}

juce::Rectangle<int> PostCardComponent::getUserInfoBounds() const
{
    auto avatar = getAvatarBounds();
    return juce::Rectangle<int>(avatar.getRight() + 15, 15, 140, CARD_HEIGHT - 30);
}

juce::Rectangle<int> PostCardComponent::getWaveformBounds() const
{
    auto userInfo = getUserInfoBounds();
    int waveformX = userInfo.getRight() + 15;
    int waveformWidth = getWidth() - waveformX - 130;
    return juce::Rectangle<int>(waveformX, 20, waveformWidth, CARD_HEIGHT - 40);
}

juce::Rectangle<int> PostCardComponent::getPlayButtonBounds() const
{
    auto waveform = getWaveformBounds();
    return juce::Rectangle<int>(waveform.getCentreX() - BUTTON_SIZE / 2,
                                waveform.getCentreY() - BUTTON_SIZE / 2,
                                BUTTON_SIZE, BUTTON_SIZE);
}

juce::Rectangle<int> PostCardComponent::getLikeButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 115, CARD_HEIGHT - 35, 50, 25);
}

juce::Rectangle<int> PostCardComponent::getCommentButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 60, CARD_HEIGHT - 35, 45, 25);
}

juce::Rectangle<int> PostCardComponent::getShareButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 35, 15, 25, 25);
}

juce::Rectangle<int> PostCardComponent::getMoreButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 35, 45, 25, 25);
}

juce::Rectangle<int> PostCardComponent::getFollowButtonBounds() const
{
    // Position follow button below the timestamp, to the right of user info
    auto userInfo = getUserInfoBounds();
    return juce::Rectangle<int>(userInfo.getX(), userInfo.getY() + 58, 65, 22);
}

//==============================================================================
// Like Animation

void PostCardComponent::startLikeAnimation()
{
    likeAnimationActive = true;
    likeAnimationProgress = 0.0f;
    startTimer(1000 / LIKE_ANIMATION_FPS);
}

void PostCardComponent::timerCallback()
{
    // Check for long-press on like button
    if (longPressActive)
    {
        juce::uint32 elapsed = juce::Time::getMillisecondCounter() - longPressStartTime;
        if (elapsed >= LONG_PRESS_DURATION_MS)
        {
            longPressActive = false;
            showEmojiReactionsPanel();

            // Stop timer if no animation is running
            if (!likeAnimationActive)
                stopTimer();

            return;
        }
    }

    // Handle like animation
    if (likeAnimationActive)
    {
        // Advance animation
        float step = (1000.0f / LIKE_ANIMATION_FPS) / LIKE_ANIMATION_DURATION_MS;
        likeAnimationProgress += step;

        if (likeAnimationProgress >= 1.0f)
        {
            likeAnimationProgress = 1.0f;
            likeAnimationActive = false;
        }

        repaint();
    }

    // Stop timer if nothing is active
    if (!likeAnimationActive && !longPressActive)
    {
        stopTimer();
    }
}

void PostCardComponent::drawLikeAnimation(juce::Graphics& g)
{
    if (!likeAnimationActive)
        return;

    auto likeBounds = getLikeButtonBounds();
    float cx = static_cast<float>(likeBounds.getCentreX()) - 5.0f;
    float cy = static_cast<float>(likeBounds.getCentreY());

    // Easing function for smooth animation (ease out cubic)
    float t = likeAnimationProgress;
    float easedT = 1.0f - std::pow(1.0f - t, 3.0f);

    // Scale animation (pop in then settle)
    float scalePhase = easedT < 0.5f ? easedT * 2.0f : 1.0f;
    float scale = 1.0f + std::sin(scalePhase * juce::MathConstants<float>::pi) * 0.5f;

    // Draw expanding hearts that burst outward
    int numHearts = 6;
    for (int i = 0; i < numHearts; ++i)
    {
        float angle = (static_cast<float>(i) / static_cast<float>(numHearts)) * juce::MathConstants<float>::twoPi;
        float distance = easedT * 25.0f;
        float alpha = 1.0f - easedT;

        float hx = cx + std::cos(angle) * distance;
        float hy = cy + std::sin(angle) * distance;

        // Smaller hearts that burst out
        float heartSize = (1.0f - easedT * 0.5f) * 8.0f;

        g.setColour(juce::Colour::fromRGB(255, 80, 80).withAlpha(alpha * 0.8f));
        g.setFont(heartSize);
        g.drawText("♥", static_cast<int>(hx - heartSize / 2), static_cast<int>(hy - heartSize / 2),
                   static_cast<int>(heartSize), static_cast<int>(heartSize), juce::Justification::centred);
    }

    // Draw central heart with scale
    float centralSize = 14.0f * scale;
    float alpha = juce::jmin(1.0f, 2.0f - easedT * 1.5f);
    g.setColour(juce::Colour::fromRGB(255, 80, 80).withAlpha(alpha));
    g.setFont(centralSize);
    g.drawText("♥", static_cast<int>(cx - centralSize / 2), static_cast<int>(cy - centralSize / 2),
               static_cast<int>(centralSize), static_cast<int>(centralSize), juce::Justification::centred);

    // Draw a ring that expands
    float ringRadius = easedT * 30.0f;
    float ringAlpha = (1.0f - easedT) * 0.3f;
    g.setColour(juce::Colour::fromRGB(255, 80, 80).withAlpha(ringAlpha));
    g.drawEllipse(cx - ringRadius, cy - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, 2.0f);
}

//==============================================================================
// Emoji Reactions

void PostCardComponent::showEmojiReactionsPanel()
{
    // Create a temporary component to use as anchor for the bubble
    // We need to find the like button's screen position

    auto* bubble = new EmojiReactionsBubble(this);

    // Set the currently selected emoji if user has already reacted
    if (post.userReaction.isNotEmpty())
        bubble->setSelectedEmoji(post.userReaction);

    // Handle emoji selection
    bubble->onEmojiSelected = [this](const juce::String& emoji) {
        handleEmojiSelected(emoji);
    };

    // Position and show the bubble
    // The bubble will position itself relative to this component
    bubble->show();
}

void PostCardComponent::handleEmojiSelected(const juce::String& emoji)
{
    // Update local state
    post.userReaction = emoji;
    post.isLiked = true;

    // Trigger animation
    startLikeAnimation();

    // Notify callback
    if (onEmojiReaction)
        onEmojiReaction(post, emoji);

    repaint();
}
