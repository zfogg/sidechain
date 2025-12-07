#include "PostCard.h"
#include "../../util/Colors.h"
#include "../../stores/ImageCache.h"
#include "../../util/UIHelpers.h"
#include "../../util/StringFormatter.h"
#include "../../util/HoverState.h"
#include "../../util/LongPressDetector.h"
#include "../../util/Log.h"
#include "../../util/Animation.h"
#include <algorithm>

//==============================================================================
PostCard::PostCard()
{
    setSize(600, CARD_HEIGHT);

    // Set up hover state
    hoverState.onHoverChanged = [this](bool hovered) {
        repaint();
    };

    // Set up long-press detector for emoji reactions
    longPressDetector.onLongPress = [this]() {
        showEmojiReactionsPanel();
    };

    // Set up fade-in animation
    fadeInOpacity.onValueChanged = [this](float opacity) {
        repaint();
    };
    fadeInOpacity.onAnimationComplete = []() {
        // Animation complete, opacity is now 1.0
    };
}

PostCard::~PostCard()
{
}

//==============================================================================
void PostCard::setPost(const FeedPost& newPost)
{
    post = newPost;
    avatarImage = juce::Image();
    Log::debug("PostCard: Setting post - ID: " + post.id + ", user: " + post.username);

    // Start fade-in animation
    fadeInOpacity.setImmediate(0.0f);
    fadeInOpacity.animateTo(1.0f);

    // Load avatar via ImageCache
    if (post.userAvatarUrl.isNotEmpty())
    {
        ImageLoader::load(post.userAvatarUrl, [this](const juce::Image& img) {
            avatarImage = img;
            repaint();
        });
    }

    repaint();
}

void PostCard::updateLikeCount(int count, bool liked)
{
    post.likeCount = count;
    post.isLiked = liked;
    Log::debug("PostCard: Like count updated - post: " + post.id + ", count: " + juce::String(count) + ", liked: " + juce::String(liked ? "true" : "false"));
    repaint();
}

void PostCard::updatePlayCount(int count)
{
    post.playCount = count;
    Log::debug("PostCard: Play count updated - post: " + post.id + ", count: " + juce::String(count));
    repaint();
}

void PostCard::updateFollowState(bool following)
{
    post.isFollowing = following;
    repaint();
}

void PostCard::updateReaction(const juce::String& emoji)
{
    post.userReaction = emoji;
    if (emoji.isNotEmpty())
    {
        post.isLiked = true;  // Reacting also counts as a like
    }
    repaint();
}

void PostCard::setPlaybackProgress(float progress)
{
    playbackProgress = juce::jlimit(0.0f, 1.0f, progress);
    repaint();
}

void PostCard::setIsPlaying(bool playing)
{
    isPlaying = playing;
    Log::debug("PostCard: Playback state changed - post: " + post.id + ", playing: " + juce::String(playing ? "true" : "false"));
    repaint();
}

void PostCard::setLoading(bool loading)
{
    isLoading = loading;
    repaint();
}

void PostCard::setDownloadProgress(float progress)
{
    downloadProgress = juce::jlimit(0.0f, 1.0f, progress);
    isDownloading = (progress > 0.0f && progress < 1.0f);
    repaint();
}

//==============================================================================
void PostCard::paint(juce::Graphics& g)
{
    // Apply fade-in opacity
    g.setOpacity(fadeInOpacity.getValue());

    drawBackground(g);
    drawAvatar(g, getAvatarBounds());
    drawUserInfo(g, getUserInfoBounds());
    drawFollowButton(g, getFollowButtonBounds());
    drawWaveform(g, getWaveformBounds());
    drawPlayButton(g, getPlayButtonBounds());
    drawMetadataBadges(g, juce::Rectangle<int>(getWidth() - 120, 15, 110, CARD_HEIGHT - 30));
    drawSocialButtons(g, juce::Rectangle<int>(getWidth() - 120, CARD_HEIGHT - 40, 110, 30));

    // Reset opacity for like animation (should be fully visible)
    g.setOpacity(1.0f);
    // Draw like animation on top of everything
    drawLikeAnimation(g);
}

void PostCard::drawBackground(juce::Graphics& g)
{
    UIHelpers::drawCardWithHover(g, getLocalBounds(),
        SidechainColors::backgroundLight(),
        SidechainColors::backgroundLighter(),
        SidechainColors::border(),
        hoverState.isHovered());
}

void PostCard::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    ImageLoader::drawCircularAvatar(g, bounds, avatarImage,
        ImageLoader::getInitials(post.username),
        SidechainColors::surface(),
        SidechainColors::textPrimary());

    // Avatar border
    g.setColour(SidechainColors::border());
    g.drawEllipse(bounds.toFloat(), 1.0f);

    // Draw online indicator (green/cyan dot in bottom-right corner)
    if (post.isOnline || post.isInStudio)
    {
        const int indicatorSize = 14;
        const int borderWidth = 2;

        // Position at bottom-right of avatar
        auto indicatorBounds = juce::Rectangle<int>(
            bounds.getRight() - indicatorSize + 2,
            bounds.getBottom() - indicatorSize + 2,
            indicatorSize,
            indicatorSize
        ).toFloat();

        // Draw dark border (matches card background)
        g.setColour(SidechainColors::background());
        g.fillEllipse(indicatorBounds);

        // Draw indicator (cyan for in_studio, green for just online)
        auto innerBounds = indicatorBounds.reduced(borderWidth);
        g.setColour(post.isInStudio ? SidechainColors::inStudioIndicator() : SidechainColors::onlineIndicator());
        g.fillEllipse(innerBounds);
    }
}

void PostCard::drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Username
    g.setColour(SidechainColors::textPrimary());
    g.setFont(14.0f);
    g.drawText(post.username.isEmpty() ? "Unknown" : post.username,
               bounds.getX(), bounds.getY(), bounds.getWidth(), 20,
               juce::Justification::centredLeft);

    // Timestamp
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText(post.timeAgo,
               bounds.getX(), bounds.getY() + 20, bounds.getWidth(), 18,
               juce::Justification::centredLeft);

    // DAW badge if present
    if (post.daw.isNotEmpty())
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(10.0f);
        g.drawText(post.daw,
                   bounds.getX(), bounds.getY() + 40, bounds.getWidth(), 15,
                   juce::Justification::centredLeft);
    }
}

void PostCard::drawFollowButton(juce::Graphics& g, juce::Rectangle<int> bounds)
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
        textColor = SidechainColors::textSecondary();
        borderColor = SidechainColors::border();
    }
    else
    {
        // Not following: prominent filled button
        bgColor = SidechainColors::follow();
        textColor = SidechainColors::textPrimary();
        borderColor = SidechainColors::follow();
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

void PostCard::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Waveform background
    g.setColour(SidechainColors::waveformBackground());
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
            g.setColour(SidechainColors::waveformPlayed()); // Played portion
        }
        else
        {
            g.setColour(SidechainColors::waveform()); // Unplayed portion
        }

        g.fillRect(barX, barY, barWidth, barHeight);
    }

    // Duration overlay at bottom-right of waveform - use UIHelpers::drawBadge
    if (post.durationSeconds > 0)
    {
        juce::String duration = StringFormatter::formatDuration(post.durationSeconds);
        auto durationBounds = juce::Rectangle<int>(bounds.getRight() - 45, bounds.getBottom() - 18, 40, 16);

        UIHelpers::drawBadge(g, durationBounds, duration,
            SidechainColors::background().withAlpha(0.85f),
            SidechainColors::textPrimary(), 10.0f, 3.0f);
    }
}

void PostCard::drawPlayButton(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Semi-transparent circle background
    g.setColour(SidechainColors::background().withAlpha(0.75f));
    g.fillEllipse(bounds.toFloat());

    // Play/pause icon
    g.setColour(SidechainColors::textPrimary());

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
    g.setColour(SidechainColors::textPrimary().withAlpha(0.4f));
    g.drawEllipse(bounds.toFloat(), 1.0f);
}

void PostCard::drawMetadataBadges(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int badgeY = bounds.getY();

    // BPM badge
    if (post.bpm > 0)
    {
        auto bpmBounds = juce::Rectangle<int>(bounds.getX(), badgeY, 55, BADGE_HEIGHT);
        UIHelpers::drawBadge(g, bpmBounds, StringFormatter::formatBPM(post.bpm),
            SidechainColors::surface(), SidechainColors::textPrimary(), 11.0f, 4.0f);
        badgeY += BADGE_HEIGHT + 5;
    }

    // Key badge
    if (post.key.isNotEmpty())
    {
        auto keyBounds = juce::Rectangle<int>(bounds.getX(), badgeY, 55, BADGE_HEIGHT);
        UIHelpers::drawBadge(g, keyBounds, post.key,
            SidechainColors::surface(), SidechainColors::textPrimary(), 11.0f, 4.0f);
        badgeY += BADGE_HEIGHT + 5;
    }

    // Genre badges (first two)
    for (int i = 0; i < juce::jmin(2, post.genres.size()); ++i)
    {
        auto genreBounds = juce::Rectangle<int>(bounds.getX(), badgeY, bounds.getWidth(), BADGE_HEIGHT - 4);
        UIHelpers::drawBadge(g, genreBounds, post.genres[i],
            SidechainColors::backgroundLighter(), SidechainColors::textSecondary(), 10.0f, 3.0f);
        badgeY += BADGE_HEIGHT;
    }

    // MIDI badge (always visible when post has MIDI)
    if (post.hasMidi)
    {
        auto midiBadgeBounds = juce::Rectangle<int>(bounds.getX(), badgeY, 55, BADGE_HEIGHT);
        UIHelpers::drawBadge(g, midiBadgeBounds, "MIDI",
            SidechainColors::primary().withAlpha(0.2f), SidechainColors::primary(), 11.0f, 4.0f);
        badgeY += BADGE_HEIGHT + 5;
    }

    // Recommendation reason badge (for "For You" feed)
    if (post.recommendationReason.isNotEmpty())
    {
        auto reasonBounds = juce::Rectangle<int>(bounds.getX(), badgeY, bounds.getWidth(), BADGE_HEIGHT - 4);
        // Use a subtle accent color to indicate it's a recommendation
        UIHelpers::drawBadge(g, reasonBounds, post.recommendationReason,
            SidechainColors::primary().withAlpha(0.2f), SidechainColors::primary(), 9.0f, 3.0f);
    }
}

void PostCard::drawSocialButtons(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Like/Reaction button
    auto likeBounds = getLikeButtonBounds();

    // Show user's reaction emoji if they've reacted, otherwise show heart
    if (post.userReaction.isNotEmpty())
    {
        // Show the emoji the user reacted with
        g.setFont(16.0f);
        g.setColour(SidechainColors::textPrimary());
        g.drawText(post.userReaction, likeBounds.withWidth(22), juce::Justification::centred);
    }
    else
    {
        // Show heart icon
        juce::Colour likeColor = post.isLiked ? SidechainColors::like() : SidechainColors::textMuted();
        g.setColour(likeColor);
        g.setFont(14.0f);
        juce::String heartIcon = post.isLiked ? juce::String(juce::CharPointer_UTF8("\xE2\x99\xA5")) : juce::String(juce::CharPointer_UTF8("\xE2\x99\xA1"));
        g.drawText(heartIcon, likeBounds.withWidth(20), juce::Justification::centred);
    }

    // Calculate total reaction count (sum of all emoji reactions)
    int totalReactions = post.likeCount;
    for (const auto& [emoji, count] : post.reactionCounts)
    {
        if (emoji != "like")  // Don't double-count "like" reactions
            totalReactions += count;
    }

    // Show total reaction count if we have reactions
    if (totalReactions > 0)
    {
        g.setColour(post.isLiked || post.userReaction.isNotEmpty() ? SidechainColors::like() : SidechainColors::textMuted());
        g.setFont(11.0f);
        g.drawText(StringFormatter::formatCount(totalReactions),
                   likeBounds.withX(likeBounds.getX() + 20).withWidth(30),
                   juce::Justification::centredLeft);
    }

    // Draw individual emoji reaction counts (top 3 most popular)
    drawReactionCounts(g, likeBounds);

    // Comment count
    auto commentBounds = getCommentButtonBounds();
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    // Draw comment bubble icon (avoid emoji for Linux font compatibility)
    auto iconBounds = commentBounds.withWidth(16).withHeight(14).withY(commentBounds.getCentreY() - 7);
    g.drawRoundedRectangle(iconBounds.toFloat(), 3.0f, 1.5f);
    // Small tail for speech bubble
    juce::Path tail;
    tail.addTriangle(iconBounds.getX() + 3, iconBounds.getBottom(),
                     iconBounds.getX() + 8, iconBounds.getBottom(),
                     iconBounds.getX() + 2, iconBounds.getBottom() + 4);
    g.fillPath(tail);

    g.setFont(11.0f);
    g.drawText(StringFormatter::formatCount(post.commentCount),
               commentBounds.withX(commentBounds.getX() + 18).withWidth(25),
               juce::Justification::centredLeft);

    // Play count (views)
    g.setColour(SidechainColors::textMuted());
    g.setFont(10.0f);
    g.drawText(StringFormatter::formatPlays(post.playCount),
               bounds.getX(), bounds.getY() - 15, 60, 12,
               juce::Justification::centredLeft);

    // Add to DAW button
    auto addToDAWBounds = getAddToDAWButtonBounds();
    if (hoverState.isHovered() && addToDAWBounds.contains(getMouseXYRelative()))
    {
        g.setColour(SidechainColors::surfaceHover());
        g.fillRoundedRectangle(addToDAWBounds.toFloat(), 4.0f);
    }

    g.setColour(SidechainColors::textSecondary());
    g.setFont(10.0f);
    g.drawText("Add to DAW", addToDAWBounds, juce::Justification::centred);

    // Drop to Track button (shown on hover or when downloading)
    if (hoverState.isHovered() || isDownloading)
    {
        auto dropToTrackBounds = getDropToTrackButtonBounds();

        if (isDownloading)
        {
            // Show progress bar
            g.setColour(SidechainColors::backgroundLighter());
            g.fillRoundedRectangle(dropToTrackBounds.toFloat(), 4.0f);

            auto progressBounds = dropToTrackBounds.withWidth(static_cast<int>(dropToTrackBounds.getWidth() * downloadProgress));
            g.setColour(SidechainColors::follow());
            g.fillRoundedRectangle(progressBounds.toFloat(), 4.0f);

            g.setColour(SidechainColors::textPrimary());
            g.setFont(9.0f);
            juce::String progressText = juce::String(static_cast<int>(downloadProgress * 100)) + "%";
            g.drawText(progressText, dropToTrackBounds, juce::Justification::centred);
        }
        else
        {
            // Normal button state
            if (dropToTrackBounds.contains(getMouseXYRelative()))
            {
                g.setColour(SidechainColors::surfaceHover());
                g.fillRoundedRectangle(dropToTrackBounds.toFloat(), 4.0f);
            }

            g.setColour(SidechainColors::textPrimary());
            g.setFont(10.0f);
            g.drawText("Drop to Track", dropToTrackBounds, juce::Justification::centred);
        }
    }

    // Download MIDI button (only shown when post has MIDI and on hover)
    if (post.hasMidi && hoverState.isHovered())
    {
        auto midiBounds = getDownloadMIDIButtonBounds();

        if (midiBounds.contains(getMouseXYRelative()))
        {
            g.setColour(SidechainColors::surfaceHover());
            g.fillRoundedRectangle(midiBounds.toFloat(), 4.0f);
        }

        // Use text to indicate MIDI (avoid emoji for Linux font compatibility)
        g.setColour(SidechainColors::primary());
        g.setFont(9.0f);
        g.drawText("[MIDI]", midiBounds, juce::Justification::centred);
    }

    // Add to Playlist button (shown on hover) - R.3.1.3.3
    if (hoverState.isHovered())
    {
        auto playlistBounds = getAddToPlaylistButtonBounds();

        if (playlistBounds.contains(getMouseXYRelative()))
        {
            g.setColour(SidechainColors::surfaceHover());
            g.fillRoundedRectangle(playlistBounds.toFloat(), 4.0f);
        }

        g.setColour(SidechainColors::textSecondary());
        g.setFont(9.0f);
        g.drawText("[+Playlist]", playlistBounds, juce::Justification::centred);
    }

    // Download Project File button (only shown when post has project file and on hover)
    if (post.hasProjectFile && hoverState.isHovered())
    {
        auto projectBounds = getDownloadProjectButtonBounds();

        if (projectBounds.contains(getMouseXYRelative()))
        {
            g.setColour(SidechainColors::surfaceHover());
            g.fillRoundedRectangle(projectBounds.toFloat(), 4.0f);
        }

        // Use DAW type to indicate project file (avoid emoji for Linux font compatibility)
        juce::String dawLabel = post.projectFileDaw.isNotEmpty() ? post.projectFileDaw.toUpperCase().substring(0, 3) : "PRJ";
        g.setColour(SidechainColors::primary());
        g.setFont(9.0f);
        g.drawText("[" + dawLabel + "]", projectBounds, juce::Justification::centred);
    }

    // Remix button (R.3.2 Remix Chains) - always shown on hover
    if (hoverState.isHovered())
    {
        auto remixBounds = getRemixButtonBounds();

        if (remixBounds.contains(getMouseXYRelative()))
        {
            g.setColour(SidechainColors::surfaceHover());
            g.fillRoundedRectangle(remixBounds.toFloat(), 4.0f);
        }

        // Show different label based on what's remixable
        juce::String remixLabel;
        if (post.hasMidi && post.audioUrl.isNotEmpty())
            remixLabel = "Remix";  // Can remix both
        else if (post.hasMidi)
            remixLabel = "Remix MIDI";
        else
            remixLabel = "Remix Audio";

        g.setColour(SidechainColors::primary());
        g.setFont(9.0f);
        g.drawText(remixLabel, remixBounds, juce::Justification::centred);
    }

    // Remix chain badge (shows remix count or "Remix of..." indicator)
    if (post.isRemix || post.remixCount > 0)
    {
        auto chainBounds = getRemixChainBadgeBounds();

        // Background
        g.setColour(SidechainColors::primary().withAlpha(0.15f));
        g.fillRoundedRectangle(chainBounds.toFloat(), 3.0f);

        // Border
        g.setColour(SidechainColors::primary().withAlpha(0.4f));
        g.drawRoundedRectangle(chainBounds.toFloat(), 3.0f, 1.0f);

        // Text
        g.setColour(SidechainColors::primary());
        g.setFont(9.0f);

        juce::String badgeText;
        if (post.isRemix && post.remixCount > 0)
        {
            // Both a remix and has remixes
            badgeText = "Remix +" + juce::String(post.remixCount);
        }
        else if (post.isRemix)
        {
            // Just a remix (depth indicator)
            if (post.remixChainDepth > 1)
                badgeText = "Remix (x" + juce::String(post.remixChainDepth) + ")";
            else
                badgeText = "Remix";
        }
        else
        {
            // Original with remixes
            badgeText = juce::String(post.remixCount) + " Remixes";
        }

        g.drawText(badgeText, chainBounds, juce::Justification::centred);
    }
}

void PostCard::drawReactionCounts(juce::Graphics& g, juce::Rectangle<int> likeBounds)
{
    if (post.reactionCounts.empty())
        return;

    // Collect all reactions and sort by count (descending)
    struct ReactionItem
    {
        juce::String emoji;
        int count;
    };

    juce::Array<ReactionItem> reactions;
    for (const auto& [emoji, count] : post.reactionCounts)
    {
        // Skip "like" reactions as they're already shown in the main count
        if (emoji == "like" || count == 0)
            continue;
        reactions.add({emoji, count});
    }

    // Sort by count (descending) using std::sort with iterators
    std::sort(reactions.begin(), reactions.end(),
        [](const ReactionItem& a, const ReactionItem& b) {
            return a.count > b.count;
        });

    // Show top 3 reactions below the like button
    int maxReactions = juce::jmin(3, reactions.size());
    if (maxReactions == 0)
        return;

    int reactionY = likeBounds.getBottom() + 2;
    int reactionX = likeBounds.getX();
    int emojiSize = 14;
    int spacing = 4;

    for (int i = 0; i < maxReactions; ++i)
    {
        const auto& reaction = reactions[i];

        // Draw emoji
        g.setFont(static_cast<float>(emojiSize));
        g.setColour(SidechainColors::textPrimary());
        auto emojiBounds = juce::Rectangle<int>(reactionX, reactionY, emojiSize, emojiSize);
        g.drawText(reaction.emoji, emojiBounds, juce::Justification::centred);

        // Draw count next to emoji
        g.setFont(9.0f);
        g.setColour(SidechainColors::textMuted());
        auto countBounds = juce::Rectangle<int>(reactionX + emojiSize + 2, reactionY, 20, emojiSize);
        g.drawText(StringFormatter::formatCount(reaction.count), countBounds, juce::Justification::centredLeft);

        // Move to next position
        reactionX += emojiSize + spacing + 22;  // emoji + spacing + count width
    }
}

//==============================================================================
void PostCard::resized()
{
    // Layout is handled in paint() using bounds calculations
}

void PostCard::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check if pressing on the like button area - start long-press detection
    if (getLikeButtonBounds().contains(pos))
    {
        longPressDetector.start();
    }
}

void PostCard::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check if long-press was triggered (before canceling)
    bool wasLongPress = longPressDetector.wasTriggered();
    longPressDetector.cancel();

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

    // Check like button
    if (getLikeButtonBounds().contains(pos))
    {
        if (!wasLongPress)
        {
            if (onLikeToggled)
                onLikeToggled(post, !post.isLiked);
        }
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

    // Check follow button
    if (getFollowButtonBounds().contains(pos))
    {
        if (onFollowToggled)
            onFollowToggled(post, !post.isFollowing);
        return;
    }

    // Check more button
    if (getMoreButtonBounds().contains(pos))
    {
        if (onMoreClicked)
            onMoreClicked(post);
        return;
    }

    // Check Add to DAW button
    if (getAddToDAWButtonBounds().contains(pos))
    {
        if (onAddToDAWClicked)
            onAddToDAWClicked(post);
        return;
    }

    // Check Drop to Track button
    if (hoverState.isHovered() && getDropToTrackButtonBounds().contains(pos))
    {
        if (onDropToTrackClicked)
            onDropToTrackClicked(post);
        return;
    }

    // Check Download MIDI button (only when post has MIDI)
    if (post.hasMidi && hoverState.isHovered() && getDownloadMIDIButtonBounds().contains(pos))
    {
        if (onDownloadMIDIClicked)
            onDownloadMIDIClicked(post);
        return;
    }

    // Check Download Project File button (only when post has project file)
    if (post.hasProjectFile && hoverState.isHovered() && getDownloadProjectButtonBounds().contains(pos))
    {
        if (onDownloadProjectClicked)
            onDownloadProjectClicked(post);
        return;
    }

    // Check Add to Playlist button (R.3.1.3.3)
    if (hoverState.isHovered() && getAddToPlaylistButtonBounds().contains(pos))
    {
        if (onAddToPlaylistClicked)
            onAddToPlaylistClicked(post);
        return;
    }

    // Check Remix button (R.3.2 Remix Chains)
    if (hoverState.isHovered() && getRemixButtonBounds().contains(pos))
    {
        if (onRemixClicked)
        {
            // Determine default remix type based on what's available
            juce::String defaultRemixType = "audio";
            if (post.hasMidi && post.audioUrl.isNotEmpty())
                defaultRemixType = "both";  // Default to remixing both when available
            else if (post.hasMidi)
                defaultRemixType = "midi";

            onRemixClicked(post, defaultRemixType);
        }
        return;
    }

    // Check Remix chain badge (view remix lineage)
    if ((post.isRemix || post.remixCount > 0) && getRemixChainBadgeBounds().contains(pos))
    {
        if (onRemixChainClicked)
            onRemixChainClicked(post);
        return;
    }

    // Check avatar (navigate to profile)
    if (getAvatarBounds().contains(pos))
    {
        if (onUserClicked)
            onUserClicked(post);
        return;
    }

    // Check waveform (seek)
    if (getWaveformBounds().contains(pos))
    {
        auto waveformBounds = getWaveformBounds();
        float normalizedPos = static_cast<float>(pos.x - waveformBounds.getX()) / static_cast<float>(waveformBounds.getWidth());
        normalizedPos = juce::jlimit(0.0f, 1.0f, normalizedPos);
        if (onWaveformClicked)
            onWaveformClicked(post, normalizedPos);
        return;
    }

    // If this was a simple click on the card (not on any interactive element), trigger card tap
    if (event.mouseWasClicked() && !event.mods.isAnyModifierKeyDown())
    {
        if (onCardTapped)
        {
            onCardTapped(post);
        }
    }
}

void PostCard::mouseEnter(const juce::MouseEvent& /*event*/)
{
    hoverState.setHovered(true);
}

void PostCard::mouseExit(const juce::MouseEvent& /*event*/)
{
    hoverState.setHovered(false);
    // Cancel any active long-press when mouse exits
    longPressDetector.cancel();
}

//==============================================================================
juce::Rectangle<int> PostCard::getAvatarBounds() const
{
    return juce::Rectangle<int>(15, (CARD_HEIGHT - AVATAR_SIZE) / 2, AVATAR_SIZE, AVATAR_SIZE);
}

juce::Rectangle<int> PostCard::getUserInfoBounds() const
{
    auto avatar = getAvatarBounds();
    return juce::Rectangle<int>(avatar.getRight() + 15, 15, 140, CARD_HEIGHT - 30);
}

juce::Rectangle<int> PostCard::getWaveformBounds() const
{
    auto userInfo = getUserInfoBounds();
    int waveformX = userInfo.getRight() + 15;
    int waveformWidth = getWidth() - waveformX - 130;
    return juce::Rectangle<int>(waveformX, 20, waveformWidth, CARD_HEIGHT - 40);
}

juce::Rectangle<int> PostCard::getPlayButtonBounds() const
{
    auto waveform = getWaveformBounds();
    return juce::Rectangle<int>(waveform.getCentreX() - BUTTON_SIZE / 2,
                                waveform.getCentreY() - BUTTON_SIZE / 2,
                                BUTTON_SIZE, BUTTON_SIZE);
}

juce::Rectangle<int> PostCard::getLikeButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 115, CARD_HEIGHT - 35, 50, 25);
}

juce::Rectangle<int> PostCard::getCommentButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 60, CARD_HEIGHT - 35, 45, 25);
}

juce::Rectangle<int> PostCard::getShareButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 35, 15, 25, 25);
}

juce::Rectangle<int> PostCard::getMoreButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 35, 45, 25, 25);
}

juce::Rectangle<int> PostCard::getFollowButtonBounds() const
{
    // Position follow button below the timestamp, to the right of user info
    auto userInfo = getUserInfoBounds();
    return juce::Rectangle<int>(userInfo.getX(), userInfo.getY() + 58, 65, 22);
}

juce::Rectangle<int> PostCard::getAddToDAWButtonBounds() const
{
    // Position below the play count, on the left side
    return juce::Rectangle<int>(getWidth() - 115, CARD_HEIGHT - 20, 70, 18);
}

juce::Rectangle<int> PostCard::getDropToTrackButtonBounds() const
{
    // Position next to Add to DAW button, slightly above
    return juce::Rectangle<int>(getWidth() - 115, CARD_HEIGHT - 40, 70, 18);
}

juce::Rectangle<int> PostCard::getDownloadMIDIButtonBounds() const
{
    // Position above Drop to Track button (only shown when post has MIDI)
    return juce::Rectangle<int>(getWidth() - 115, CARD_HEIGHT - 58, 70, 16);
}

juce::Rectangle<int> PostCard::getDownloadProjectButtonBounds() const
{
    // Position above MIDI button (or above Drop to Track if no MIDI)
    int yOffset = post.hasMidi ? CARD_HEIGHT - 76 : CARD_HEIGHT - 58;
    return juce::Rectangle<int>(getWidth() - 115, yOffset, 70, 16);
}

juce::Rectangle<int> PostCard::getAddToPlaylistButtonBounds() const
{
    // Position to the left of Drop to Track button
    return juce::Rectangle<int>(getWidth() - 190, CARD_HEIGHT - 40, 70, 18);
}

juce::Rectangle<int> PostCard::getRemixButtonBounds() const
{
    // Position to the left of Add to Playlist button
    return juce::Rectangle<int>(getWidth() - 265, CARD_HEIGHT - 40, 70, 18);
}

juce::Rectangle<int> PostCard::getRemixChainBadgeBounds() const
{
    // Position at top-right of waveform area to show remix lineage
    auto waveform = getWaveformBounds();
    return juce::Rectangle<int>(waveform.getRight() - 80, waveform.getY() - 2, 78, 16);
}

//==============================================================================
// Like Animation

void PostCard::startLikeAnimation()
{
    likeAnimation.start();
}

void PostCard::timerCallback()
{
    // Long-press detection is handled by LongPressDetector
    // Only need to manage animation timer here

    // Stop timer if animation is not running
    if (!likeAnimation.isRunning())
    {
        stopTimer();
    }
}

void PostCard::drawLikeAnimation(juce::Graphics& g)
{
    if (!likeAnimation.isRunning())
        return;

    auto likeBounds = getLikeButtonBounds();
    float cx = static_cast<float>(likeBounds.getCentreX()) - 5.0f;
    float cy = static_cast<float>(likeBounds.getCentreY());

    // Get eased progress from animation
    float easedT = likeAnimation.getProgress();

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

        g.setColour(SidechainColors::like().withAlpha(alpha * 0.8f));
        g.setFont(heartSize);
        g.drawText("♥", static_cast<int>(hx - heartSize / 2), static_cast<int>(hy - heartSize / 2),
                   static_cast<int>(heartSize), static_cast<int>(heartSize), juce::Justification::centred);
    }

    // Draw central heart with scale
    float centralSize = 14.0f * scale;
    float alpha = juce::jmin(1.0f, 2.0f - easedT * 1.5f);
    g.setColour(SidechainColors::like().withAlpha(alpha));
    g.setFont(centralSize);
    g.drawText("♥", static_cast<int>(cx - centralSize / 2), static_cast<int>(cy - centralSize / 2),
               static_cast<int>(centralSize), static_cast<int>(centralSize), juce::Justification::centred);

    // Draw a ring that expands
    float ringRadius = easedT * 30.0f;
    float ringAlpha = (1.0f - easedT) * 0.3f;
    g.setColour(SidechainColors::like().withAlpha(ringAlpha));
    g.drawEllipse(cx - ringRadius, cy - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, 2.0f);
}

//==============================================================================
// Emoji Reactions

void PostCard::showEmojiReactionsPanel()
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

void PostCard::handleEmojiSelected(const juce::String& emoji)
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
