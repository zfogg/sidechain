#pragma once

#include <JuceHeader.h>
#include "../../models/FeedPost.h"
#include "../../util/Animation.h"
#include "../../util/HoverState.h"
#include "../../util/LongPressDetector.h"
#include "EmojiReactionsPanel.h"

//==============================================================================
/**
 * PostCard displays a single post in the feed
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
class PostCard : public juce::Component,
                          public juce::Timer
{
public:
    PostCard();
    ~PostCard() override;

    //==============================================================================
    // Data binding

    /** Set the post data to display
     * @param post The feed post to display
     */
    void setPost(const FeedPost& post);

    /** Get the current post data
     * @return Reference to the current post
     */
    const FeedPost& getPost() const { return post; }

    /** Get the post ID
     * @return The unique post identifier
     */
    juce::String getPostId() const { return post.id; }

    //==============================================================================
    // Update specific fields without full refresh

    /** Update the like count and liked state
     * @param count The new like count
     * @param isLiked Whether the current user has liked this post
     */
    void updateLikeCount(int count, bool isLiked);

    /** Update the play count
     * @param count The new play count
     */
    void updatePlayCount(int count);

    /** Update the follow state for the post author
     * @param isFollowing Whether the current user is following the author
     */
    void updateFollowState(bool isFollowing);

    /** Update the user's reaction emoji
     * @param emoji The emoji reaction (empty string to clear)
     */
    void updateReaction(const juce::String& emoji);

    /** Set the playback progress indicator
     * @param progress Progress from 0.0 to 1.0
     */
    void setPlaybackProgress(float progress);

    /** Set whether the post is currently playing
     * @param playing true if playing, false otherwise
     */
    void setIsPlaying(bool playing);

    /** Set whether the post is currently playing (alias)
     * @param playing true if playing, false otherwise
     */
    void setPlaying(bool playing) { setIsPlaying(playing); }

    /** Set the loading state
     * @param loading true if loading audio, false otherwise
     */
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

    /** Paint the component
     * @param g Graphics context for drawing
     */
    void paint(juce::Graphics& g) override;

    /** Handle component resize
     */
    void resized() override;

    /** Handle mouse down events
     * @param event Mouse event details
     */
    void mouseDown(const juce::MouseEvent& event) override;

    /** Handle mouse up events
     * @param event Mouse event details
     */
    void mouseUp(const juce::MouseEvent& event) override;

    /** Handle mouse enter events
     * @param event Mouse event details
     */
    void mouseEnter(const juce::MouseEvent& event) override;

    /** Handle mouse exit events
     * @param event Mouse event details
     */
    void mouseExit(const juce::MouseEvent& event) override;

    //==============================================================================
    // Timer override for animations and long-press detection

    /** Timer callback for animations and long-press detection
     */
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
    HoverState hoverState;
    bool isPlaying = false;
    bool isLoading = false;
    float playbackProgress = 0.0f;

    // Like animation
    Animation likeAnimation{400, Animation::Easing::EaseOutCubic};

    // Long-press detector for emoji reactions panel
    LongPressDetector longPressDetector{400};  // 400ms threshold

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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostCard)
};
