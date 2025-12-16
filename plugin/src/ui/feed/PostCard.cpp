#include "PostCard.h"

#include "../../ui/animations/Easing.h"
#include "../../ui/animations/TransitionAnimation.h"
#include "../../util/Colors.h"
#include "../../util/HoverState.h"
#include "../../util/Log.h"
#include "../../util/LongPressDetector.h"
#include "../../util/StringFormatter.h"
#include "../../util/UIHelpers.h"
#include "../../util/profiling/PerformanceMonitor.h"
#include <algorithm>

using namespace Sidechain::UI::Animations;

//==============================================================================
PostCard::PostCard(Sidechain::Stores::AppStore *store) : AppStoreComponent(nullptr) {
  setSize(600, CARD_HEIGHT);

  // Set up hover state
  hoverState.onHoverChanged = [this]([[maybe_unused]] bool hovered) { repaint(); };

  // Set up long-press detector for emoji reactions
  longPressDetector.onLongPress = [this]() { showEmojiReactionsPanel(); };

  // Fade-in animation will be created when setPost() is called
  // No setup needed here anymore

  // Add waveform image view as child component
  addAndMakeVisible(waveformView);
  waveformView.setBackgroundColour(SidechainColors::waveformBackground());

  // Store reference for lazy subscription
  appStore = store;
}

PostCard::~PostCard() {
  // Base class AppStoreComponent handles store unsubscription in destructor
}

//==============================================================================
void PostCard::setNetworkClient(NetworkClient *client) {
  waveformView.setNetworkClient(client);
}

void PostCard::setPost(const FeedPost &newPost) {
  post = newPost;
  Log::debug("PostCard: Setting post - ID: " + post.id + ", user: " + post.username +
             ", isFollowing: " + juce::String(post.isFollowing ? "true" : "false") +
             ", isOwnPost: " + juce::String(post.isOwnPost ? "true" : "false"));

  if (appStore && !post.id.isEmpty()) {
    bindToStore(appStore);
  }

  // Immediately repaint to reflect updated post data (especially follow state)
  repaint();

  // Create and start fade-in animation
  currentOpacity = 0.0f;
  // Use SafePointer for animation callback in case PostCard is destroyed
  juce::Component::SafePointer<PostCard> safeThis(this);
  fadeInAnimation = TransitionAnimation<float>::create(0.0f, 1.0f, 300)
                        ->withEasing(Easing::easeOutCubic)
                        ->onProgress([safeThis](float opacity) {
                          if (safeThis == nullptr)
                            return;
                          safeThis->currentOpacity = opacity;
                          safeThis->repaint();
                        })
                        ->start();

  // Fetch avatar image via AppStore reactive observable (with caching)
  if (post.userAvatarUrl.isNotEmpty() && appStore) {
    juce::Component::SafePointer<PostCard> safeThis(this);
    appStore->loadImageObservable(post.userAvatarUrl)
        .subscribe(
            [safeThis](const juce::Image &image) {
              if (safeThis == nullptr)
                return;
              if (image.isValid()) {
                safeThis->repaint();
              }
            },
            [safeThis](std::exception_ptr) {
              if (safeThis == nullptr)
                return;
              Log::warn("PostCard: Failed to load avatar image");
            });
  }

  // Load waveform image from CDN
  if (post.waveformUrl.isNotEmpty()) {
    Log::debug("PostCard: Loading waveform from " + post.waveformUrl);
    waveformView.loadFromUrl(post.waveformUrl);
  } else {
    waveformView.clear();
  }

  repaint();
}

//==============================================================================
// UI State Updates (not persisted to PostsStore)
// Post data updates now come automatically via PostsStore subscription
// (Task 2.5)

void PostCard::setPlaybackProgress(float progress) {
  playbackProgress = juce::jlimit(0.0f, 1.0f, progress);
  repaint();
}

void PostCard::setIsPlaying(bool playing) {
  isPlaying = playing;
  Log::debug("PostCard: Playback state changed - post: " + post.id +
             ", playing: " + juce::String(playing ? "true" : "false"));
  repaint();
}

void PostCard::setLoading(bool loading) {
  isLoading = loading;
  repaint();
}

void PostCard::setDownloadProgress(float progress) {
  downloadProgress = juce::jlimit(0.0f, 1.0f, progress);
  isDownloading = (progress > 0.0f && progress < 1.0f);
  repaint();
}

//==============================================================================
void PostCard::paint(juce::Graphics &g) {
  SCOPED_TIMER_THRESHOLD("ui::render_post", 16.0);
  // Apply fade-in opacity
  g.setOpacity(currentOpacity);

  drawBackground(g);

  // Draw repost attribution header if this is a repost
  if (post.isARepost)
    drawRepostAttribution(g);

  drawAvatar(g, getAvatarBounds());
  drawUserInfo(g, getUserInfoBounds());
  drawFollowButton(g, getFollowButtonBounds());
  drawWaveform(g, getWaveformBounds());
  drawPlayButton(g, getPlayButtonBounds());
  drawSoundBadge(g); // Sound indicator below waveform
  drawMetadataBadges(
      g, juce::Rectangle<int>(getWidth() - RIGHT_PANEL_WIDTH - 10, 10, RIGHT_PANEL_WIDTH, CARD_HEIGHT - 50));
  drawSocialButtons(g,
                    juce::Rectangle<int>(getWidth() - RIGHT_PANEL_WIDTH - 10, CARD_HEIGHT - 45, RIGHT_PANEL_WIDTH, 35));

  // Reset opacity for like animation (should be fully visible)
  g.setOpacity(1.0f);
  // Draw like animation on top of everything
  drawLikeAnimation(g);
}

void PostCard::drawBackground(juce::Graphics &g) {
  UIHelpers::drawCardWithHover(g, getLocalBounds(), SidechainColors::backgroundLight(),
                               SidechainColors::backgroundLighter(), SidechainColors::border(), hoverState.isHovered());
}

void PostCard::drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(SidechainColors::surface());
  g.fillEllipse(bounds.toFloat());

  // Avatar border
  g.setColour(SidechainColors::border());
  g.drawEllipse(bounds.toFloat(), 1.0f);

  // Draw online indicator (green/cyan dot in bottom-right corner)
  if (post.isOnline || post.isInStudio) {
    const int indicatorSize = 14;
    const int borderWidth = 2;

    // Position at bottom-right of avatar
    auto indicatorBounds = juce::Rectangle<int>(bounds.getRight() - indicatorSize + 2,
                                                bounds.getBottom() - indicatorSize + 2, indicatorSize, indicatorSize)
                               .toFloat();

    // Draw dark border (matches card background)
    g.setColour(SidechainColors::background());
    g.fillEllipse(indicatorBounds);

    // Draw indicator (cyan for in_studio, green for just online)
    auto innerBounds = indicatorBounds.reduced(borderWidth);
    g.setColour(post.isInStudio ? SidechainColors::inStudioIndicator() : SidechainColors::onlineIndicator());
    g.fillEllipse(innerBounds);
  }
}

void PostCard::drawUserInfo(juce::Graphics &g, juce::Rectangle<int> bounds) {
  int yOffset = bounds.getY();

  // For reposts, show original post info; otherwise show current post info
  juce::String displayFilename = post.isARepost ? post.originalFilename : post.filename;
  juce::String displayUsername = post.isARepost ? post.originalUsername : post.username;

  // Primary header: filename if available, otherwise username
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);

  if (displayFilename.isNotEmpty()) {
    // Show filename as main header
    g.drawText(displayFilename, bounds.getX(), yOffset, bounds.getWidth(), 22, juce::Justification::centredLeft);
    yOffset += 22;

    // Show "by username" below
    g.setColour(SidechainColors::textSecondary());
    g.setFont(14.0f);
    g.drawText("by " + (displayUsername.isEmpty() ? "Unknown" : displayUsername), bounds.getX(), yOffset,
               bounds.getWidth(), 20, juce::Justification::centredLeft);
    yOffset += 20;
  } else {
    // Fallback: show username as main header
    g.drawText(displayUsername.isEmpty() ? "Unknown" : displayUsername, bounds.getX(), yOffset, bounds.getWidth(), 22,
               juce::Justification::centredLeft);
    yOffset += 24;
  }

  // Timestamp
  g.setColour(SidechainColors::textMuted());
  g.setFont(13.0f);
  g.drawText(post.timeAgo, bounds.getX(), yOffset, bounds.getWidth(), 20, juce::Justification::centredLeft);

  // DAW badge if present
  if (post.daw.isNotEmpty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText(post.daw, bounds.getX(), yOffset + 20, bounds.getWidth(), 18, juce::Justification::centredLeft);
  }
}

void PostCard::drawFollowButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Don't show follow button for own posts
  if (post.isOwnPost)
    return;

  // Button text based on follow state
  juce::String buttonText = post.isFollowing ? "Following" : "Follow";

  // Colors based on state
  juce::Colour bgColor, textColor, borderColor;
  if (post.isFollowing) {
    // Following state: subtle outline button
    bgColor = juce::Colour::fromRGBA(0, 0, 0, 0);
    textColor = SidechainColors::textSecondary();
    borderColor = SidechainColors::border();
  } else {
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

void PostCard::drawWaveform(juce::Graphics &g, juce::Rectangle<int> bounds) {
  SCOPED_TIMER("ui::draw_waveform");
  // If we have a waveform URL, the WaveformImageView component will handle
  // rendering
  if (post.waveformUrl.isNotEmpty()) {
    // Duration overlay at bottom-right of waveform - use UIHelpers::drawBadge
    if (post.durationSeconds > 0) {
      juce::String duration = StringFormatter::formatDuration(post.durationSeconds);
      auto durationBounds = bounds.removeFromBottom(18).removeFromRight(50).reduced(2);
      UIHelpers::drawBadge(g, durationBounds, duration, SidechainColors::background().withAlpha(0.85f),
                           SidechainColors::textPrimary(), 10.0f, 3.0f);
    }
    return;
  }

  // Fallback: draw fake waveform if no waveformUrl (legacy posts)
  // Waveform background
  g.setColour(SidechainColors::waveformBackground());
  g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

  // Generate deterministic waveform based on post ID
  int barWidth = 3;
  int barSpacing = 2;
  int numBars = bounds.getWidth() / (barWidth + barSpacing);

  // Draw waveform bars
  for (int i = 0; i < numBars; ++i) {
    float barProgress = static_cast<float>(i) / static_cast<float>(numBars);
    int barHeight = 5 + (std::hash<int>{}(post.id.hashCode() + i) % 25);
    int barX = bounds.getX() + i * (barWidth + barSpacing);
    int barY = bounds.getCentreY() - barHeight / 2;

    // Color based on playback progress
    if (barProgress <= playbackProgress) {
      g.setColour(SidechainColors::waveformPlayed()); // Played portion
    } else {
      g.setColour(SidechainColors::waveform()); // Unplayed portion
    }

    g.fillRect(barX, barY, barWidth, barHeight);
  }

  // Duration overlay at bottom-right of waveform - use UIHelpers::drawBadge
  if (post.durationSeconds > 0) {
    juce::String duration = StringFormatter::formatDuration(post.durationSeconds);
    auto durationBounds = juce::Rectangle<int>(bounds.getRight() - 45, bounds.getBottom() - 18, 40, 16);

    UIHelpers::drawBadge(g, durationBounds, duration, SidechainColors::background().withAlpha(0.85f),
                         SidechainColors::textPrimary(), 10.0f, 3.0f);
  }
}

void PostCard::drawPlayButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Semi-transparent circle background
  g.setColour(SidechainColors::background().withAlpha(0.75f));
  g.fillEllipse(bounds.toFloat());

  // Play/pause icon
  g.setColour(SidechainColors::textPrimary());

  if (isPlaying) {
    // Pause icon (two vertical bars)
    int barWidth = 4;
    int barHeight = 14;
    int gap = 4;
    int startX = bounds.getCentreX() - (barWidth + gap / 2);
    int startY = bounds.getCentreY() - barHeight / 2;

    g.fillRect(startX, startY, barWidth, barHeight);
    g.fillRect(startX + barWidth + gap, startY, barWidth, barHeight);
  } else {
    // Play icon (triangle)
    juce::Path triangle;
    float cx = static_cast<float>(bounds.getCentreX());
    float cy = static_cast<float>(bounds.getCentreY());
    float size = 10.0f;

    // Slightly offset to right for visual centering
    triangle.addTriangle(cx - size * 0.4f, cy - size, cx - size * 0.4f, cy + size, cx + size * 0.8f, cy);
    g.fillPath(triangle);
  }

  // Border
  g.setColour(SidechainColors::textPrimary().withAlpha(0.4f));
  g.drawEllipse(bounds.toFloat(), 1.0f);
}

void PostCard::drawMetadataBadges(juce::Graphics &g, juce::Rectangle<int> bounds) {
  int badgeY = bounds.getY();
  int badgeX = bounds.getX();
  int colWidth = bounds.getWidth() / 2 - 4; // Two columns with spacing

  // Row 1: BPM and Key badges side-by-side
  bool hasBpm = post.bpm > 0;
  bool hasKey = post.key.isNotEmpty();

  if (hasBpm || hasKey) {
    if (hasBpm) {
      auto bpmBounds = juce::Rectangle<int>(badgeX, badgeY, colWidth, BADGE_HEIGHT);
      UIHelpers::drawBadge(g, bpmBounds, StringFormatter::formatBPM(post.bpm), SidechainColors::surface(),
                           SidechainColors::textPrimary(), 13.0f, 4.0f);
    }

    if (hasKey) {
      int keyX = hasBpm ? badgeX + colWidth + 8 : badgeX;
      auto keyBounds = juce::Rectangle<int>(keyX, badgeY, colWidth, BADGE_HEIGHT);
      UIHelpers::drawBadge(g, keyBounds, post.key, SidechainColors::surface(), SidechainColors::textPrimary(), 13.0f,
                           4.0f);
    }
    badgeY += BADGE_HEIGHT + 6;
  }

  // Row 2: Play count
  if (post.playCount > 0) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(13.0f);
    g.drawText(StringFormatter::formatPlays(post.playCount), badgeX, badgeY, bounds.getWidth(), 18,
               juce::Justification::centredLeft);
    badgeY += 22;
  }

  // Row 3: Genre badges (up to two, side by side)
  if (post.genres.size() > 0) {
    for (int i = 0; i < juce::jmin(2, post.genres.size()); ++i) {
      juce::String genre = post.genres[i];
      // Truncate long genre names
      if (genre.length() > 10)
        genre = genre.substring(0, 8) + "..";

      int genreX = badgeX + i * (colWidth + 8);
      auto genreBounds = juce::Rectangle<int>(genreX, badgeY, colWidth, BADGE_HEIGHT - 2);
      UIHelpers::drawBadge(g, genreBounds, genre, SidechainColors::backgroundLighter(),
                           SidechainColors::textSecondary(), 12.0f, 4.0f);
    }
    badgeY += BADGE_HEIGHT + 4;
  }

  // Row 4: MIDI badge (always visible when post has MIDI)
  if (post.hasMidi) {
    auto midiBadgeBounds = juce::Rectangle<int>(badgeX, badgeY, 65, BADGE_HEIGHT);
    UIHelpers::drawBadge(g, midiBadgeBounds, "MIDI", SidechainColors::primary().withAlpha(0.2f),
                         SidechainColors::primary(), 13.0f, 4.0f);
    badgeY += BADGE_HEIGHT + 6;
  }

  // Row 5: Recommendation reason badge (for "For You" feed)
  if (post.recommendationReason.isNotEmpty()) {
    auto reasonBounds = juce::Rectangle<int>(badgeX, badgeY, bounds.getWidth(), BADGE_HEIGHT);
    UIHelpers::drawBadge(g, reasonBounds, post.recommendationReason, SidechainColors::primary().withAlpha(0.2f),
                         SidechainColors::primary(), 11.0f, 4.0f);
  }
}

void PostCard::drawSocialButtons(juce::Graphics &g, [[maybe_unused]] juce::Rectangle<int> bounds) {
  // Like/Reaction button
  auto likeBounds = getLikeButtonBounds();

  // Show user's reaction emoji if they've reacted, otherwise show heart
  if (post.userReaction.isNotEmpty()) {
    // Show the emoji the user reacted with
    g.setFont(18.0f);
    g.setColour(SidechainColors::textPrimary());
    g.drawText(post.userReaction, likeBounds.withWidth(24), juce::Justification::centred);
  } else {
    // Show heart icon
    juce::Colour likeColor = post.isLiked ? SidechainColors::like() : SidechainColors::textMuted();
    g.setColour(likeColor);
    g.setFont(16.0f);
    juce::String heartIcon = post.isLiked ? juce::String(juce::CharPointer_UTF8("\xE2\x99\xA5"))
                                          : juce::String(juce::CharPointer_UTF8("\xE2\x99\xA1"));
    g.drawText(heartIcon, likeBounds.withWidth(22), juce::Justification::centred);
  }

  // Calculate total reaction count (sum of all emoji reactions)
  int totalReactions = post.likeCount;
  for (const auto &[emoji, count] : post.reactionCounts) {
    if (emoji != "like") // Don't double-count "like" reactions
      totalReactions += count;
  }

  // Show total reaction count if we have reactions
  if (totalReactions > 0) {
    g.setColour(post.isLiked || post.userReaction.isNotEmpty() ? SidechainColors::like()
                                                               : SidechainColors::textMuted());
    g.setFont(13.0f);
    g.drawText(StringFormatter::formatCount(totalReactions), likeBounds.withX(likeBounds.getX() + 24).withWidth(30),
               juce::Justification::centredLeft);
  }

  // Draw individual emoji reaction counts (top 3 most popular)
  drawReactionCounts(g, likeBounds);

  // Save/Bookmark button
  drawSaveButton(g, getSaveButtonBounds());

  // Repost button (don't show for own posts)
  if (!post.isOwnPost)
    drawRepostButton(g, getRepostButtonBounds());

  // Pin button (only show for own posts)
  if (post.isOwnPost)
    drawPinButton(g, getPinButtonBounds());

  // Pinned badge (show if post is pinned)
  if (post.isPinned)
    drawPinnedBadge(g);

  // Comment count/status
  auto commentBounds = getCommentButtonBounds();
  bool commentsOff = post.commentsDisabled();
  g.setColour(commentsOff ? SidechainColors::textMuted().withAlpha(0.4f) : SidechainColors::textMuted());
  g.setFont(16.0f);
  // Draw comment bubble icon (avoid emoji for Linux font compatibility)
  auto iconBounds = commentBounds.withWidth(18).withHeight(16).withY(commentBounds.getCentreY() - 8);
  g.drawRoundedRectangle(iconBounds.toFloat(), 3.0f, 1.5f);
  // Small tail for speech bubble
  juce::Path tail;
  tail.addTriangle(static_cast<float>(iconBounds.getX() + 3), static_cast<float>(iconBounds.getBottom()),
                   static_cast<float>(iconBounds.getX() + 9), static_cast<float>(iconBounds.getBottom()),
                   static_cast<float>(iconBounds.getX() + 2), static_cast<float>(iconBounds.getBottom() + 5));
  g.fillPath(tail);

  // Draw strike-through line if comments disabled
  if (commentsOff) {
    g.setColour(SidechainColors::textMuted().withAlpha(0.6f));
    g.drawLine(static_cast<float>(iconBounds.getX() - 1), static_cast<float>(iconBounds.getBottom() + 2),
               static_cast<float>(iconBounds.getRight() + 1), static_cast<float>(iconBounds.getY() - 2), 1.5f);
  }

  g.setFont(13.0f);
  if (commentsOff) {
    g.setColour(SidechainColors::textMuted().withAlpha(0.4f));
    g.drawText("Off", commentBounds.withX(commentBounds.getX() + 22).withWidth(28), juce::Justification::centredLeft);
  } else {
    g.drawText(StringFormatter::formatCount(post.commentCount),
               commentBounds.withX(commentBounds.getX() + 22).withWidth(28), juce::Justification::centredLeft);
  }

  // Add to DAW button - always visible with background
  auto addToDAWBounds = getAddToDAWButtonBounds();

  // Draw button background
  if (hoverState.isHovered() && addToDAWBounds.contains(getMouseXYRelative()))
    g.setColour(SidechainColors::surfaceHover());
  else
    g.setColour(SidechainColors::backgroundLighter());
  g.fillRoundedRectangle(addToDAWBounds.toFloat(), 4.0f);

  // Draw border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(addToDAWBounds.toFloat(), 4.0f, 1.0f);

  g.setColour(SidechainColors::textPrimary());
  g.setFont(12.0f);
  g.drawText("Add to DAW", addToDAWBounds, juce::Justification::centred);

  // Drop to Track button (shown on hover or when downloading)
  if (hoverState.isHovered() || isDownloading) {
    auto dropToTrackBounds = getDropToTrackButtonBounds();

    if (isDownloading) {
      // Show progress bar
      g.setColour(SidechainColors::backgroundLighter());
      g.fillRoundedRectangle(dropToTrackBounds.toFloat(), 4.0f);

      auto progressBounds = dropToTrackBounds.withWidth(
          static_cast<int>(static_cast<float>(dropToTrackBounds.getWidth()) * downloadProgress));
      g.setColour(SidechainColors::follow());
      g.fillRoundedRectangle(progressBounds.toFloat(), 4.0f);

      g.setColour(SidechainColors::textPrimary());
      g.setFont(11.0f);
      juce::String progressText = juce::String(static_cast<int>(downloadProgress * 100)) + "%";
      g.drawText(progressText, dropToTrackBounds, juce::Justification::centred);
    } else {
      // Normal button state
      if (dropToTrackBounds.contains(getMouseXYRelative())) {
        g.setColour(SidechainColors::surfaceHover());
        g.fillRoundedRectangle(dropToTrackBounds.toFloat(), 4.0f);
      }

      g.setColour(SidechainColors::textPrimary());
      g.setFont(11.0f);
      g.drawText("Drop to Track", dropToTrackBounds, juce::Justification::centred);
    }
  }

  // Download MIDI button (only shown when post has MIDI and on hover)
  if (post.hasMidi && hoverState.isHovered()) {
    auto midiBounds = getDownloadMIDIButtonBounds();

    if (midiBounds.contains(getMouseXYRelative())) {
      g.setColour(SidechainColors::surfaceHover());
      g.fillRoundedRectangle(midiBounds.toFloat(), 4.0f);
    }

    // Use text to indicate MIDI (avoid emoji for Linux font compatibility)
    g.setColour(SidechainColors::primary());
    g.setFont(11.0f);
    g.drawText("MIDI", midiBounds, juce::Justification::centred);
  }

  // Add to Playlist button (shown on hover) - R.3.1.3.3
  if (hoverState.isHovered()) {
    auto playlistBounds = getAddToPlaylistButtonBounds();

    if (playlistBounds.contains(getMouseXYRelative())) {
      g.setColour(SidechainColors::surfaceHover());
      g.fillRoundedRectangle(playlistBounds.toFloat(), 4.0f);
    }

    g.setColour(SidechainColors::textSecondary());
    g.setFont(11.0f);
    g.drawText("+Playlist", playlistBounds, juce::Justification::centred);
  }

  // Download Project File button (only shown when post has project file and on
  // hover)
  if (post.hasProjectFile && hoverState.isHovered()) {
    auto projectBounds = getDownloadProjectButtonBounds();

    if (projectBounds.contains(getMouseXYRelative())) {
      g.setColour(SidechainColors::surfaceHover());
      g.fillRoundedRectangle(projectBounds.toFloat(), 4.0f);
    }

    // Use DAW type to indicate project file (avoid emoji for Linux font
    // compatibility)
    juce::String dawLabel =
        post.projectFileDaw.isNotEmpty() ? post.projectFileDaw.toUpperCase().substring(0, 3) : "PRJ";
    g.setColour(SidechainColors::primary());
    g.setFont(11.0f);
    g.drawText(dawLabel, projectBounds, juce::Justification::centred);
  }

  // Remix button (R.3.2 Remix Chains) - always visible
  {
    auto remixBounds = getRemixButtonBounds();

    // Draw button background
    if (hoverState.isHovered() && remixBounds.contains(getMouseXYRelative()))
      g.setColour(SidechainColors::primary().withAlpha(0.3f));
    else
      g.setColour(SidechainColors::primary().withAlpha(0.15f));
    g.fillRoundedRectangle(remixBounds.toFloat(), 4.0f);

    // Draw border
    g.setColour(SidechainColors::primary().withAlpha(0.5f));
    g.drawRoundedRectangle(remixBounds.toFloat(), 4.0f, 1.0f);

    // Show different label based on what's remixable
    juce::String remixLabel;
    if (post.hasMidi && post.audioUrl.isNotEmpty())
      remixLabel = "Remix"; // Can remix both
    else if (post.hasMidi)
      remixLabel = "Remix MIDI";
    else
      remixLabel = "Remix";

    g.setColour(SidechainColors::primary());
    g.setFont(12.0f);
    g.drawText(remixLabel, remixBounds, juce::Justification::centred);
  }

  // Remix chain badge (shows remix count or "Remix of..." indicator)
  if (post.isRemix || post.remixCount > 0) {
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
    if (post.isRemix && post.remixCount > 0) {
      // Both a remix and has remixes
      badgeText = "Remix +" + juce::String(post.remixCount);
    } else if (post.isRemix) {
      // Just a remix (depth indicator)
      if (post.remixChainDepth > 1)
        badgeText = "Remix (x" + juce::String(post.remixChainDepth) + ")";
      else
        badgeText = "Remix";
    } else {
      // Original with remixes
      badgeText = juce::String(post.remixCount) + " Remixes";
    }

    g.drawText(badgeText, chainBounds, juce::Justification::centred);
  }
}

void PostCard::drawReactionCounts(juce::Graphics &g, juce::Rectangle<int> likeBounds) {
  if (post.reactionCounts.empty())
    return;

  // Collect all reactions and sort by count (descending)
  struct ReactionItem {
    juce::String emoji;
    int count;
  };

  juce::Array<ReactionItem> reactions;
  for (const auto &[emoji, count] : post.reactionCounts) {
    // Skip "like" reactions as they're already shown in the main count
    if (emoji == "like" || count == 0)
      continue;
    reactions.add({emoji, count});
  }

  // Sort by count (descending) using std::sort with iterators
  std::sort(reactions.begin(), reactions.end(),
            [](const ReactionItem &a, const ReactionItem &b) { return a.count > b.count; });

  // Show top 3 reactions below the like button
  int maxReactions = juce::jmin(3, reactions.size());
  if (maxReactions == 0)
    return;

  int reactionY = likeBounds.getBottom() + 2;
  int reactionX = likeBounds.getX();
  int emojiSize = 14;
  int spacing = 4;

  for (int i = 0; i < maxReactions; ++i) {
    const auto &reaction = reactions[i];

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
    reactionX += emojiSize + spacing + 22; // emoji + spacing + count width
  }
}

void PostCard::drawSaveButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Bookmark/Save button
  juce::Colour saveColor = post.isSaved ? SidechainColors::primary() : SidechainColors::textMuted();
  g.setColour(saveColor);

  // Draw bookmark icon
  auto iconBounds = bounds.withWidth(16).withHeight(18).withY(bounds.getCentreY() - 9);
  juce::Path bookmark;

  if (post.isSaved) {
    // Filled bookmark
    bookmark.addRectangle(static_cast<float>(iconBounds.getX()), static_cast<float>(iconBounds.getY()),
                          static_cast<float>(iconBounds.getWidth()), static_cast<float>(iconBounds.getHeight() - 4));
    // Triangular notch at bottom
    bookmark.addTriangle(static_cast<float>(iconBounds.getX()), static_cast<float>(iconBounds.getBottom() - 4),
                         static_cast<float>(iconBounds.getX() + iconBounds.getWidth()),
                         static_cast<float>(iconBounds.getBottom() - 4), static_cast<float>(iconBounds.getCentreX()),
                         static_cast<float>(iconBounds.getBottom() - 8));
    g.fillPath(bookmark);
  } else {
    // Outline bookmark
    bookmark.startNewSubPath(static_cast<float>(iconBounds.getX()), static_cast<float>(iconBounds.getY()));
    bookmark.lineTo(static_cast<float>(iconBounds.getX()), static_cast<float>(iconBounds.getBottom() - 4));
    bookmark.lineTo(static_cast<float>(iconBounds.getCentreX()), static_cast<float>(iconBounds.getBottom() - 8));
    bookmark.lineTo(static_cast<float>(iconBounds.getRight()), static_cast<float>(iconBounds.getBottom() - 4));
    bookmark.lineTo(static_cast<float>(iconBounds.getRight()), static_cast<float>(iconBounds.getY()));
    bookmark.closeSubPath();
    g.strokePath(bookmark, juce::PathStrokeType(1.5f));
  }

  // Draw save count if > 0
  if (post.saveCount > 0) {
    g.setFont(11.0f);
    g.drawText(StringFormatter::formatCount(post.saveCount), bounds.withX(bounds.getX() + 18).withWidth(25),
               juce::Justification::centredLeft);
  }
}

void PostCard::drawRepostButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Don't show repost button for own posts
  if (post.isOwnPost)
    return;

  juce::Colour repostColor = post.isReposted ? SidechainColors::success() : SidechainColors::textMuted();
  g.setColour(repostColor);

  // Draw repost icon (two arrows in circular motion, similar to Twitter
  // retweet)
  auto iconBounds = bounds.withWidth(18).withHeight(14).withY(bounds.getCentreY() - 7);
  float cx = static_cast<float>(iconBounds.getCentreX());
  float cy = static_cast<float>(iconBounds.getCentreY());
  float size = 6.0f;

  juce::Path repostIcon;

  // Create arc paths with arrow heads
  // Top-right arrow
  juce::Path topArc;
  topArc.addArc(cx - size, cy - size, size * 2.0f, size * 2.0f, -juce::MathConstants<float>::pi * 0.5f,
                juce::MathConstants<float>::pi * 0.5f, true);
  // Arrow head at end of top arc
  float arrowTipX = cx + size;
  float arrowTipY = cy;
  repostIcon.addTriangle(arrowTipX, arrowTipY - 4, arrowTipX, arrowTipY + 4, arrowTipX + 5, arrowTipY);

  // Bottom-left arrow
  juce::Path bottomArc;
  bottomArc.addArc(cx - size, cy - size, size * 2.0f, size * 2.0f, juce::MathConstants<float>::pi * 0.5f,
                   juce::MathConstants<float>::pi * 1.5f, true);
  // Arrow head at end of bottom arc
  arrowTipX = cx - size;
  arrowTipY = cy;
  repostIcon.addTriangle(arrowTipX, arrowTipY - 4, arrowTipX, arrowTipY + 4, arrowTipX - 5, arrowTipY);

  g.fillPath(repostIcon);

  // Stroke the arcs
  g.strokePath(topArc, juce::PathStrokeType(1.5f));
  g.strokePath(bottomArc, juce::PathStrokeType(1.5f));

  // Draw repost count if > 0
  if (post.repostCount > 0) {
    g.setFont(11.0f);
    g.drawText(StringFormatter::formatCount(post.repostCount), bounds.withX(bounds.getX() + 20).withWidth(20),
               juce::Justification::centredLeft);
  }
}

void PostCard::drawPinButton(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Only show for own posts
  if (!post.isOwnPost)
    return;

  juce::Colour pinColor = post.isPinned ? SidechainColors::primary() : SidechainColors::textMuted();
  g.setColour(pinColor);

  // Draw pin icon (pushpin shape)
  auto iconBounds = bounds.withWidth(16).withHeight(18).withY(bounds.getCentreY() - 9);
  float x = static_cast<float>(iconBounds.getX());
  float y = static_cast<float>(iconBounds.getY());
  float w = static_cast<float>(iconBounds.getWidth());
  float h = static_cast<float>(iconBounds.getHeight());

  juce::Path pin;

  if (post.isPinned) {
    // Filled pushpin for pinned state
    // Pin head (rounded rectangle at top)
    pin.addRoundedRectangle(x + 2, y, w - 4, h * 0.35f, 2.0f);
    // Pin body (tapered rectangle)
    pin.addRectangle(x + 4, y + h * 0.35f, w - 8, h * 0.3f);
    // Pin point (triangle)
    pin.addTriangle(x + w * 0.5f, y + h,       // bottom point
                    x + 4, y + h * 0.65f,      // top left
                    x + w - 4, y + h * 0.65f); // top right
    g.fillPath(pin);
  } else {
    // Outlined pushpin for unpinned state
    // Pin head outline
    g.drawRoundedRectangle(x + 2, y, w - 4, h * 0.35f, 2.0f, 1.5f);
    // Pin body outline
    g.drawRect(x + 4, y + h * 0.35f, w - 8, h * 0.3f, 1.5f);
    // Pin point (triangle outline)
    pin.addTriangle(x + w * 0.5f, y + h, x + 4, y + h * 0.65f, x + w - 4, y + h * 0.65f);
    g.strokePath(pin, juce::PathStrokeType(1.5f));
  }
}

void PostCard::drawPinnedBadge(juce::Graphics &g) {
  if (!post.isPinned)
    return;

  // Draw small pin badge in top-right corner of the card
  auto badgeBounds = juce::Rectangle<int>(getWidth() - 55, 8, 48, 16);

  // Background
  g.setColour(SidechainColors::primary().withAlpha(0.2f));
  g.fillRoundedRectangle(badgeBounds.toFloat(), 4.0f);

  // Border
  g.setColour(SidechainColors::primary().withAlpha(0.5f));
  g.drawRoundedRectangle(badgeBounds.toFloat(), 4.0f, 1.0f);

  // Text
  g.setColour(SidechainColors::primary());
  g.setFont(10.0f);
  g.drawText("PINNED", badgeBounds, juce::Justification::centred);
}

void PostCard::drawSoundBadge(juce::Graphics &g) {
  // Only show if post has a detected sound with multiple usages
  if (post.soundId.isEmpty() || post.soundUsageCount < 2)
    return;

  auto badgeBounds = getSoundBadgeBounds();

  // Hover state - highlight on hover
  bool isHovered = hoverState.isHovered() && badgeBounds.contains(getMouseXYRelative());

  // Background with subtle gradient
  if (isHovered) {
    g.setColour(SidechainColors::primary().withAlpha(0.25f));
  } else {
    g.setColour(SidechainColors::backgroundLighter().withAlpha(0.9f));
  }
  g.fillRoundedRectangle(badgeBounds.toFloat(), 4.0f);

  // Border
  g.setColour(isHovered ? SidechainColors::primary().withAlpha(0.6f) : SidechainColors::border());
  g.drawRoundedRectangle(badgeBounds.toFloat(), 4.0f, 1.0f);

  // Music note icon (using text since we can't use emojis reliably on Linux)
  g.setColour(isHovered ? SidechainColors::primary() : SidechainColors::textSecondary());
  g.setFont(10.0f);

  // Format text: use sound name if available, otherwise generic text
  juce::String badgeText;
  if (post.soundName.isNotEmpty()) {
    // Truncate long sound names
    juce::String name = post.soundName.length() > 10 ? post.soundName.substring(0, 8) + ".." : post.soundName;
    badgeText = name + " (" + juce::String(post.soundUsageCount) + ")";
  } else {
    badgeText = juce::String(post.soundUsageCount) + " posts";
  }

  g.drawText(badgeText, badgeBounds.reduced(4, 0), juce::Justification::centred);
}

void PostCard::drawRepostAttribution(juce::Graphics &g) {
  if (!post.isARepost || post.originalUsername.isEmpty())
    return;

  // Draw "Username reposted" header above the card content
  g.setColour(SidechainColors::textMuted());
  g.setFont(11.0f);

  // Repost icon (small arrows)
  juce::String repostText = post.username + " reposted";

  // Draw at the very top of the card
  auto headerBounds = juce::Rectangle<int>(15, 2, getWidth() - 30, 14);
  g.drawText(repostText, headerBounds, juce::Justification::centredLeft);
}

//==============================================================================
void PostCard::resized() {
  // Position waveform image view
  waveformView.setBounds(getWaveformBounds());
}

void PostCard::mouseDown(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Check if pressing on the like button area - start long-press detection
  if (getLikeButtonBounds().contains(pos)) {
    longPressDetector.start();
  }
}

void PostCard::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Check if long-press was triggered (before canceling)
  bool wasLongPress = longPressDetector.wasTriggered();
  longPressDetector.cancel();

  // Check play button
  if (getPlayButtonBounds().contains(pos)) {
    Log::info("PostCard: Play button clicked for post: " + post.id + ", audioUrl: " + post.audioUrl);
    if (isPlaying) {
      if (onPauseClicked) {
        Log::debug("PostCard: Calling onPauseClicked callback");
        onPauseClicked(post);
      } else {
        Log::warn("PostCard: onPauseClicked callback not set!");
      }
    } else {
      if (onPlayClicked) {
        Log::debug("PostCard: Calling onPlayClicked callback");
        onPlayClicked(post);
      } else {
        Log::warn("PostCard: onPlayClicked callback not set!");
      }
    }
    return;
  }

  // Check like button
  if (getLikeButtonBounds().contains(pos)) {
    if (!wasLongPress) {
      if (appStore) {
        juce::Component::SafePointer<PostCard> safeThis(this);
        appStore->likePostObservable(post.id).subscribe(
            [safeThis](int) {
              if (safeThis == nullptr)
                return;
              Log::debug("PostCard: Like toggled successfully");
            },
            [safeThis](std::exception_ptr error) {
              if (safeThis == nullptr)
                return;
              Log::error("PostCard: Failed to toggle like");
            });
      } else if (onLikeToggled) {
        // Fallback for when AppStore is not set
        onLikeToggled(post, !post.isLiked);
      }
    }
    return;
  }

  // Check comment button (only if comments are enabled)
  if (getCommentButtonBounds().contains(pos)) {
    if (!post.commentsDisabled() && onCommentClicked)
      onCommentClicked(post);
    return;
  }

  // Check share button
  if (getShareButtonBounds().contains(pos)) {
    if (onShareClicked)
      onShareClicked(post);
    return;
  }

  // Check save/bookmark button
  if (getSaveButtonBounds().contains(pos)) {
    if (appStore) {
      juce::Component::SafePointer<PostCard> safeThis(this);
      appStore->toggleSaveObservable(post.id).subscribe(
          [safeThis](int) {
            if (safeThis == nullptr)
              return;
            Log::debug("PostCard: Post save toggled successfully");
          },
          [safeThis](std::exception_ptr error) {
            if (safeThis == nullptr)
              return;
            Log::error("PostCard: Failed to toggle save");
          });
    } else if (onSaveToggled) {
      // Fallback for when AppStore is not set
      onSaveToggled(post, !post.isSaved);
    }
    return;
  }

  // Check repost button (not for own posts)
  if (!post.isOwnPost && getRepostButtonBounds().contains(pos)) {
    if (appStore) {
      juce::Component::SafePointer<PostCard> safeThis(this);
      appStore->toggleRepostObservable(post.id).subscribe(
          [safeThis](int) {
            if (safeThis == nullptr)
              return;
            Log::debug("PostCard: Post repost toggled successfully");
          },
          [safeThis](std::exception_ptr error) {
            if (safeThis == nullptr)
              return;
            Log::error("PostCard: Failed to toggle repost");
          });
    } else if (onRepostClicked) {
      // Fallback for when AppStore is not set
      onRepostClicked(post);
    }
    return;
  }

  // Check pin button (only for own posts)
  if (post.isOwnPost && getPinButtonBounds().contains(pos)) {
    if (appStore) {
      juce::Component::SafePointer<PostCard> safeThis(this);
      appStore->togglePinObservable(post.id, !post.isPinned)
          .subscribe(
              [safeThis](int) {
                if (safeThis == nullptr)
                  return;
                Log::debug("PostCard: Post pin toggled successfully");
              },
              [safeThis](std::exception_ptr error) {
                if (safeThis == nullptr)
                  return;
                Log::error("PostCard: Failed to toggle pin");
              });
    } else if (onPinToggled) {
      // Fallback for when AppStore is not set
      onPinToggled(post, !post.isPinned);
    }
    return;
  }

  // Check follow button
  if (getFollowButtonBounds().contains(pos)) {
    if (appStore) {
      juce::Component::SafePointer<PostCard> safeThis(this);
      appStore->toggleFollowObservable(post.id, !post.isFollowing)
          .subscribe(
              [safeThis](int) {
                if (safeThis == nullptr)
                  return;
                Log::debug("PostCard: Follow toggled successfully");
              },
              [safeThis](std::exception_ptr error) {
                if (safeThis == nullptr)
                  return;
                Log::error("PostCard: Failed to toggle follow");
              });
    } else if (onFollowToggled) {
      // Fallback for when AppStore is not set
      onFollowToggled(post, !post.isFollowing);
    }
    return;
  }

  // Check more button
  if (getMoreButtonBounds().contains(pos)) {
    if (onMoreClicked)
      onMoreClicked(post);
    return;
  }

  // Check Add to DAW button
  if (getAddToDAWButtonBounds().contains(pos)) {
    if (onAddToDAWClicked)
      onAddToDAWClicked(post);
    return;
  }

  // Check Drop to Track button
  if (hoverState.isHovered() && getDropToTrackButtonBounds().contains(pos)) {
    if (onDropToTrackClicked)
      onDropToTrackClicked(post);
    return;
  }

  // Check Download MIDI button (only when post has MIDI)
  if (post.hasMidi && hoverState.isHovered() && getDownloadMIDIButtonBounds().contains(pos)) {
    if (onDownloadMIDIClicked)
      onDownloadMIDIClicked(post);
    return;
  }

  // Check Download Project File button (only when post has project file)
  if (post.hasProjectFile && hoverState.isHovered() && getDownloadProjectButtonBounds().contains(pos)) {
    if (onDownloadProjectClicked)
      onDownloadProjectClicked(post);
    return;
  }

  // Check Add to Playlist button (R.3.1.3.3)
  if (hoverState.isHovered() && getAddToPlaylistButtonBounds().contains(pos)) {
    if (onAddToPlaylistClicked)
      onAddToPlaylistClicked(post);
    return;
  }

  // Check Remix button (R.3.2 Remix Chains) - always clickable
  if (getRemixButtonBounds().contains(pos)) {
    if (onRemixClicked) {
      // Determine default remix type based on what's available
      juce::String defaultRemixType = "audio";
      if (post.hasMidi && post.audioUrl.isNotEmpty())
        defaultRemixType = "both"; // Default to remixing both when available
      else if (post.hasMidi)
        defaultRemixType = "midi";

      onRemixClicked(post, defaultRemixType);
    }
    return;
  }

  // Check Remix chain badge (view remix lineage)
  if ((post.isRemix || post.remixCount > 0) && getRemixChainBadgeBounds().contains(pos)) {
    if (onRemixChainClicked)
      onRemixChainClicked(post);
    return;
  }

  // Check sound badge (navigate to sound page)
  if (post.soundId.isNotEmpty() && post.soundUsageCount >= 2 && getSoundBadgeBounds().contains(pos)) {
    if (onSoundClicked)
      onSoundClicked(post.soundId);
    return;
  }

  // Check avatar (navigate to profile)
  if (getAvatarBounds().contains(pos)) {
    if (onUserClicked)
      onUserClicked(post);
    return;
  }

  // Check username area (navigate to profile)
  if (getUserInfoBounds().contains(pos)) {
    if (onUserClicked)
      onUserClicked(post);
    return;
  }

  // Check waveform (seek)
  if (getWaveformBounds().contains(pos)) {
    auto waveformBounds = getWaveformBounds();
    float normalizedPos =
        static_cast<float>(pos.x - waveformBounds.getX()) / static_cast<float>(waveformBounds.getWidth());
    normalizedPos = juce::jlimit(0.0f, 1.0f, normalizedPos);
    if (onWaveformClicked)
      onWaveformClicked(post, normalizedPos);
    return;
  }

  // If this was a simple click on the card (not on any interactive element),
  // trigger card tap
  if (event.mouseWasClicked() && !event.mods.isAnyModifierKeyDown()) {
    if (onCardTapped) {
      onCardTapped(post);
    }
  }
}

void PostCard::mouseEnter(const juce::MouseEvent & /*event*/) {
  hoverState.setHovered(true);
}

void PostCard::mouseExit(const juce::MouseEvent & /*event*/) {
  hoverState.setHovered(false);
  // Cancel any active long-press when mouse exits
  longPressDetector.cancel();
}

//==============================================================================
juce::Rectangle<int> PostCard::getAvatarBounds() const {
  return juce::Rectangle<int>(15, (CARD_HEIGHT - AVATAR_SIZE) / 2, AVATAR_SIZE, AVATAR_SIZE);
}

juce::Rectangle<int> PostCard::getUserInfoBounds() const {
  auto avatar = getAvatarBounds();
  return juce::Rectangle<int>(avatar.getRight() + 15, 15, 160, CARD_HEIGHT - 30);
}

juce::Rectangle<int> PostCard::getWaveformBounds() const {
  auto userInfo = getUserInfoBounds();
  int waveformX = userInfo.getRight() + 15;
  int waveformWidth = getWidth() - waveformX - RIGHT_PANEL_WIDTH - 20;
  return juce::Rectangle<int>(waveformX, 25, waveformWidth, CARD_HEIGHT - 55);
}

juce::Rectangle<int> PostCard::getPlayButtonBounds() const {
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getCentreX() - BUTTON_SIZE / 2, waveform.getCentreY() - BUTTON_SIZE / 2,
                              BUTTON_SIZE, BUTTON_SIZE);
}

juce::Rectangle<int> PostCard::getLikeButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - RIGHT_PANEL_WIDTH, CARD_HEIGHT - 40, 55, 28);
}

juce::Rectangle<int> PostCard::getCommentButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - RIGHT_PANEL_WIDTH + 60, CARD_HEIGHT - 40, 50, 28);
}

juce::Rectangle<int> PostCard::getShareButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 40, 15, 30, 30);
}

juce::Rectangle<int> PostCard::getMoreButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 40, 50, 30, 30);
}

juce::Rectangle<int> PostCard::getFollowButtonBounds() const {
  // Position follow button below the timestamp, to the right of user info
  auto userInfo = getUserInfoBounds();
  return juce::Rectangle<int>(userInfo.getX(), userInfo.getY() + 70, 75, 26);
}

juce::Rectangle<int> PostCard::getAddToDAWButtonBounds() const {
  // Position below the metadata badges
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getX(), CARD_HEIGHT - 25, 85, 20);
}

juce::Rectangle<int> PostCard::getDropToTrackButtonBounds() const {
  // Position next to Add to DAW button
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getX() + 90, CARD_HEIGHT - 25, 90, 20);
}

juce::Rectangle<int> PostCard::getDownloadMIDIButtonBounds() const {
  // Position next to Drop to Track
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getX() + 185, CARD_HEIGHT - 25, 60, 20);
}

juce::Rectangle<int> PostCard::getDownloadProjectButtonBounds() const {
  // Position next to MIDI button
  auto waveform = getWaveformBounds();
  int xOffset = post.hasMidi ? 250 : 185;
  return juce::Rectangle<int>(waveform.getX() + xOffset, CARD_HEIGHT - 25, 60, 20);
}

juce::Rectangle<int> PostCard::getAddToPlaylistButtonBounds() const {
  // Position below waveform on the right
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getRight() - 80, CARD_HEIGHT - 25, 80, 20);
}

juce::Rectangle<int> PostCard::getRemixButtonBounds() const {
  // Position left of Add to Playlist
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getRight() - 165, CARD_HEIGHT - 25, 80, 20);
}

juce::Rectangle<int> PostCard::getRemixChainBadgeBounds() const {
  // Position at top-right of waveform area to show remix lineage
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getRight() - 80, waveform.getY() - 2, 78, 16);
}

juce::Rectangle<int> PostCard::getSaveButtonBounds() const {
  // Position next to comment button in social buttons row
  return juce::Rectangle<int>(getWidth() - RIGHT_PANEL_WIDTH + 115, CARD_HEIGHT - 40, 45, 28);
}

juce::Rectangle<int> PostCard::getRepostButtonBounds() const {
  // Position next to save button in social buttons row
  return juce::Rectangle<int>(getWidth() - RIGHT_PANEL_WIDTH + 165, CARD_HEIGHT - 40, 40, 28);
}

juce::Rectangle<int> PostCard::getPinButtonBounds() const {
  // Position next to repost/save button area, only shown for own posts
  return juce::Rectangle<int>(getWidth() - RIGHT_PANEL_WIDTH + 165, CARD_HEIGHT - 40, 40, 28);
}

juce::Rectangle<int> PostCard::getSoundBadgeBounds() const {
  // Position below the waveform, shows "X posts with this sound"
  auto waveform = getWaveformBounds();
  return juce::Rectangle<int>(waveform.getX(), waveform.getBottom() + 2, 120, 16);
}

//==============================================================================
// Like Animation

void PostCard::startLikeAnimation() {
  // Create and start the like animation
  likeAnimationProgress = 0.0f;
  likeAnimation = TransitionAnimation<float>::create(0.0f, 1.0f, 400)
                      ->withEasing(Easing::easeOutCubic)
                      ->onProgress([this](float progress) {
                        likeAnimationProgress = progress;
                        repaint();
                      })
                      ->start();
}

void PostCard::timerCallback() {
  // Long-press detection is handled by LongPressDetector
  // TransitionAnimation handles its own timer, so we don't need to do anything
  // here
}

void PostCard::drawLikeAnimation(juce::Graphics &g) {
  // Only draw if animation is active (not null and progress > 0)
  if (!likeAnimation || likeAnimationProgress <= 0.0f)
    return;

  // Only draw while animation is running (progress < 1.0)
  if (likeAnimationProgress >= 1.0f)
    return;

  auto likeBounds = getLikeButtonBounds();
  float cx = static_cast<float>(likeBounds.getCentreX()) - 5.0f;
  float cy = static_cast<float>(likeBounds.getCentreY());

  // Get eased progress from animation
  float easedT = likeAnimationProgress;

  // Scale animation (pop in then settle)
  float scalePhase = easedT < 0.5f ? easedT * 2.0f : 1.0f;
  float scale = 1.0f + std::sin(scalePhase * juce::MathConstants<float>::pi) * 0.5f;

  // Draw expanding hearts that burst outward
  int numHearts = 6;
  for (int i = 0; i < numHearts; ++i) {
    float angle = (static_cast<float>(i) / static_cast<float>(numHearts)) * juce::MathConstants<float>::twoPi;
    float distance = easedT * 25.0f;
    float alpha = 1.0f - easedT;

    float hx = cx + std::cos(angle) * distance;
    float hy = cy + std::sin(angle) * distance;

    // Smaller hearts that burst out
    float heartSize = (1.0f - easedT * 0.5f) * 8.0f;

    g.setColour(SidechainColors::like().withAlpha(alpha * 0.8f));
    g.setFont(heartSize);
    g.drawText("<3", static_cast<int>(hx - heartSize / 2), static_cast<int>(hy - heartSize / 2),
               static_cast<int>(heartSize), static_cast<int>(heartSize), juce::Justification::centred);
  }

  // Draw central heart with scale
  float centralSize = 14.0f * scale;
  float alpha = juce::jmin(1.0f, 2.0f - easedT * 1.5f);
  g.setColour(SidechainColors::like().withAlpha(alpha));
  g.setFont(centralSize);
  g.drawText("<3", static_cast<int>(cx - centralSize / 2), static_cast<int>(cy - centralSize / 2),
             static_cast<int>(centralSize), static_cast<int>(centralSize), juce::Justification::centred);

  // Draw a ring that expands
  float ringRadius = easedT * 30.0f;
  float ringAlpha = (1.0f - easedT) * 0.3f;
  g.setColour(SidechainColors::like().withAlpha(ringAlpha));
  g.drawEllipse(cx - ringRadius, cy - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, 2.0f);
}

//==============================================================================
// Emoji Reactions

void PostCard::showEmojiReactionsPanel() {
  // Create a temporary component to use as anchor for the bubble
  // We need to find the like button's screen position

  auto *bubble = new EmojiReactionsBubble(this);

  // Set the currently selected emoji if user has already reacted
  if (post.userReaction.isNotEmpty())
    bubble->setSelectedEmoji(post.userReaction);

  // Handle emoji selection
  bubble->onEmojiSelected = [this](const juce::String &emoji) { handleEmojiSelected(emoji); };

  // Position and show the bubble
  // The bubble will position itself relative to this component
  bubble->show();
}

void PostCard::handleEmojiSelected(const juce::String &emoji) {
  // Trigger animation (happens immediately for optimistic UI)
  startLikeAnimation();

  // Use AppStore for reactive update if available
  if (appStore) {
    juce::Component::SafePointer<PostCard> safeThis(this);
    appStore->addReactionObservable(post.id, emoji)
        .subscribe(
            [safeThis](int) {
              if (safeThis == nullptr)
                return;
              Log::debug("PostCard: Reaction added successfully");
            },
            [safeThis](std::exception_ptr error) {
              if (safeThis == nullptr)
                return;
              Log::error("PostCard: Failed to add reaction");
            });
  } else if (onEmojiReaction) {
    // Fallback for when AppStore is not set
    post.userReaction = emoji;
    post.isLiked = true;
    onEmojiReaction(post, emoji);
  }

  repaint();
}

//==============================================================================
// AppStore Subscription (Type-Safe Lazy Pattern)

void PostCard::subscribeToAppStore() {
  if (!appStore)
    return;

  using namespace Sidechain::Stores;

  // Capture post ID by value to avoid accessing potentially invalid member
  juce::String postId = post.id;
  juce::Component::SafePointer<PostCard> safeThis(this);

  storeUnsubscriber = appStore->subscribeToFeed([safeThis, postId](const PostsState &postsState) {
    if (safeThis == nullptr || postId.isEmpty())
      return;

    juce::MessageManager::callAsync([safeThis, postId, postsState]() {
      if (safeThis == nullptr)
        return;

      const auto &currentFeed = postsState.getCurrentFeed();
      for (const auto &feedPost : currentFeed->posts) {
        if (feedPost.id == postId) {
          if (safeThis == nullptr)
            return;
          if (!feedPost.id.isEmpty()) {
            safeThis->post = feedPost;
            safeThis->repaint();
          }
          return;
        }
      }
    });
  });
}

void PostCard::onAppStateChanged(const Sidechain::Stores::PostsState & /*state*/) {
  repaint();
}

//==============================================================================
// Tooltips

juce::String PostCard::getTooltip() {
  auto mousePos = getMouseXYRelative();

  // Play/pause button
  if (getPlayButtonBounds().contains(mousePos))
    return isPlaying ? "Pause (Space)" : "Play loop (Space)";

  // Like button
  if (getLikeButtonBounds().contains(mousePos))
    return post.isLiked ? "Unlike" : "Like (hold for reactions)";

  // Comment button
  if (getCommentButtonBounds().contains(mousePos)) {
    if (post.commentsDisabled())
      return "Comments are disabled";
    else if (post.commentsFollowersOnly())
      return "Comments: Followers only";
    else
      return "View comments";
  }

  // Share button
  if (getShareButtonBounds().contains(mousePos))
    return "Copy link to clipboard";

  // Save/bookmark button
  if (getSaveButtonBounds().contains(mousePos))
    return post.isSaved ? "Remove from saved" : "Save to collection";

  // Repost button
  if (!post.isOwnPost && getRepostButtonBounds().contains(mousePos))
    return post.isReposted ? "Undo repost" : "Repost to your feed";

  // Pin button (only for own posts)
  if (post.isOwnPost && getPinButtonBounds().contains(mousePos))
    return post.isPinned ? "Unpin from profile" : "Pin to profile";

  // More button (context menu)
  if (getMoreButtonBounds().contains(mousePos))
    return "More options";

  // Follow button
  if (!post.isOwnPost && getFollowButtonBounds().contains(mousePos))
    return post.isFollowing ? "Unfollow" : "Follow";

  // User avatar/info - navigate to profile
  if (getAvatarBounds().contains(mousePos) || getUserInfoBounds().contains(mousePos))
    return "View " + post.username + "'s profile";

  // Waveform - seek
  if (getWaveformBounds().contains(mousePos))
    return "Click to seek";

  // Download MIDI button
  if (post.hasMidi && getDownloadMIDIButtonBounds().contains(mousePos))
    return "Download MIDI file";

  // Download project file button
  if (post.hasProjectFile && getDownloadProjectButtonBounds().contains(mousePos))
    return "Download DAW project file";

  // Add to DAW button
  if (getAddToDAWButtonBounds().contains(mousePos))
    return "Save audio to disk";

  // Drop to Track button
  if (getDropToTrackButtonBounds().contains(mousePos))
    return "Add to your project";

  // Add to Playlist button
  if (getAddToPlaylistButtonBounds().contains(mousePos))
    return "Add to a playlist";

  // Remix button
  if (getRemixButtonBounds().contains(mousePos))
    return "Create a remix";

  // Remix chain badge
  if ((post.isRemix || post.remixCount > 0) && getRemixChainBadgeBounds().contains(mousePos))
    return "View remix chain";

  // Sound badge
  if (post.soundId.isNotEmpty() && post.soundUsageCount >= 2 && getSoundBadgeBounds().contains(mousePos)) {
    juce::String tooltip = "View " + juce::String(post.soundUsageCount) + " posts with this sound";
    if (post.soundName.isNotEmpty())
      tooltip = post.soundName + " - " + tooltip;
    return tooltip;
  }

  return {}; // No tooltip for this area
}
