#pragma once

#include <JuceHeader.h>
#include "../../models/FeedPost.h"
#include "../../util/Animation.h"
#include "EmojiReactionsPanel.h"

//==============================================================================
/**
 * PostCardComponent displays a single post in the feed
 *
 * Features:
 * - User avatar with circular clip and fallback to initials
 * - Username and relative timestamp
 * - Waveform visualization with play progress overlay
 * - Play/pause button
 * - BPM and key badges
 * - Like button with count
 * - Comment count indicator
 * - Share button
 *
 * The component uses a callback-based API for actions to keep it decoupled
 * from network/audio code.
 */
class PostCardComponent : public juce::Component,
                          public juce::Timer
{
public:
    PostCardComponent();
    ~PostCardComponent() override;

    //==============================================================================
    // Data binding
    void setPost(const FeedPost& post);
    const FeedPost& getPost() const { return post; }
    juce::String getPostId() const { return post.id; }

    // Update specific fields without full refresh
    void updateLikeCount(int count, bool isLiked);
    void updatePlayCount(int count);
    void updateFollowState(bool isFollowing);
    void updateReaction(const juce::String& emoji);  // Update the user's reaction
    void setPlaybackProgress(float progress); // 0.0 - 1.0
    void setIsPlaying(bool playing);
    void setPlaying(bool playing) { setIsPlaying(playing); }
    void setLoading(bool loading);

    //==============================================================================
    // Callbacks for user actions
    std::function<void(const FeedPost&)> onPlayClicked;
    std::function<void(const FeedPost&)> onPauseClicked;
    std::function<void(const FeedPost&, bool liked)> onLikeToggled;
    std::function<void(const FeedPost&, const juce::String& emoji)> onEmojiReaction;  // Emoji reaction
    std::function<void(const FeedPost&)> onUserClicked;
    std::function<void(const FeedPost&)> onCommentClicked;
    std::function<void(const FeedPost&)> onShareClicked;
    std::function<void(const FeedPost&)> onMoreClicked;
    std::function<void(const FeedPost&, float position)> onWaveformClicked; // Seek position
    std::function<void(const FeedPost&, bool willFollow)> onFollowToggled; // Follow/unfollow user
    std::function<void(const FeedPost&)> onAddToDAWClicked; // Download audio to DAW project folder

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // Timer override for animations and long-press detection
    void timerCallback() override;

    //==============================================================================
    // Layout constants
    static constexpr int CARD_HEIGHT = 120;
    static constexpr int AVATAR_SIZE = 50;
    static constexpr int BADGE_HEIGHT = 22;
    static constexpr int BUTTON_SIZE = 32;

private:
    //==============================================================================
    FeedPost post;

    // UI state
    bool isHovered = false;
    bool isPlaying = false;
    bool isLoading = false;
    float playbackProgress = 0.0f;

    // Like animation
    Animation likeAnimation{400, Animation::Easing::EaseOutCubic};

    // Long-press state for emoji reactions panel
    bool longPressActive = false;
    juce::Point<int> longPressPosition;
    juce::uint32 longPressStartTime = 0;
    static constexpr int LONG_PRESS_DURATION_MS = 400;  // Time before showing emoji panel

    // Cached avatar image (loaded via ImageCache)
    juce::Image avatarImage;

    //==============================================================================
    // Drawing methods
    void drawBackground(juce::Graphics& g);
    void drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawUserInfo(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFollowButton(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPlayButton(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawMetadataBadges(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSocialButtons(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawReactionCounts(juce::Graphics& g, juce::Rectangle<int> likeBounds);
    void drawLikeAnimation(juce::Graphics& g);

    // Animation helpers
    void startLikeAnimation();

    // Emoji reactions helpers
    void showEmojiReactionsPanel();
    void handleEmojiSelected(const juce::String& emoji);

    //==============================================================================
    // Hit testing helpers
    juce::Rectangle<int> getAvatarBounds() const;
    juce::Rectangle<int> getUserInfoBounds() const;
    juce::Rectangle<int> getWaveformBounds() const;
    juce::Rectangle<int> getPlayButtonBounds() const;
    juce::Rectangle<int> getLikeButtonBounds() const;
    juce::Rectangle<int> getCommentButtonBounds() const;
    juce::Rectangle<int> getShareButtonBounds() const;
    juce::Rectangle<int> getMoreButtonBounds() const;
    juce::Rectangle<int> getFollowButtonBounds() const;
    juce::Rectangle<int> getAddToDAWButtonBounds() const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostCardComponent)
};
