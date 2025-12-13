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

    /** Update the save state and count
     * @param count The new save count
     * @param isSaved Whether the current user has saved this post
     */
    void updateSaveState(int count, bool isSaved);

    /** Update the repost state and count
     * @param count The new repost count
     * @param isReposted Whether the current user has reposted this post
     */
    void updateRepostState(int count, bool isReposted);

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

    /** Set the download progress (0.0 to 1.0)
     * @param progress Download progress from 0.0 to 1.0
     */
    void setDownloadProgress(float progress);

    //==============================================================================
    // Callbacks for user actions

    /** Called when play button is clicked */
    std::function<void(const FeedPost&)> onPlayClicked;

    /** Called when pause button is clicked */
    std::function<void(const FeedPost&)> onPauseClicked;

    /** Called when like button is toggled
     * @param post The post that was liked/unliked
     * @param liked true if post is now liked, false if unliked
     */
    std::function<void(const FeedPost&, bool liked)> onLikeToggled;

    /** Called when user selects an emoji reaction
     * @param post The post being reacted to
     * @param emoji The emoji string (empty to clear reaction)
     */
    std::function<void(const FeedPost&, const juce::String& emoji)> onEmojiReaction;

    /** Called when user avatar/name is clicked (navigate to profile) */
    std::function<void(const FeedPost&)> onUserClicked;

    /** Called when comment button is clicked
     * @note Currently not fully implemented - comment UI exists but not wired to API
     *       See @ref notes/PLAN.md for comment system implementation status
     */
    std::function<void(const FeedPost&)> onCommentClicked;

    /** Called when share button is clicked (copies post URL to clipboard) */
    std::function<void(const FeedPost&)> onShareClicked;

    /** Called when "more" menu button is clicked
     * @warning NOT IMPLEMENTED - Currently only logs the action, no context menu shown
     *          See @ref notes/PLAN.md for planned context menu features
     */
    std::function<void(const FeedPost&)> onMoreClicked;

    /** Called when save/bookmark button is toggled
     * @param post The post that was saved/unsaved
     * @param saved true if post is now saved, false if unsaved
     */
    std::function<void(const FeedPost&, bool saved)> onSaveToggled;

    /** Called when repost button is clicked
     * @param post The post to be reposted
     * @note Shows a confirmation dialog before reposting
     */
    std::function<void(const FeedPost&)> onRepostClicked;

    /** Called when waveform is clicked (seek to position)
     * @param post The post being seeked
     * @param position Normalized position (0.0 to 1.0) where waveform was clicked
     */
    std::function<void(const FeedPost&, float position)> onWaveformClicked;

    /** Called when follow/unfollow button is toggled
     * @param post The post whose author should be followed/unfollowed
     * @param willFollow true to follow, false to unfollow
     * @note UI updates optimistically and backend API call is implemented in PostsFeed.cpp.
     *       Success/error notifications are shown to the user.
     */
    std::function<void(const FeedPost&, bool willFollow)> onFollowToggled;

    /** Called when "Add to DAW" button is clicked
     * @param post The post to download
     * @note Downloads audio file and saves to user-selected location.
     *       Success/error notifications are shown to the user (already implemented).
     */
    std::function<void(const FeedPost&)> onAddToDAWClicked;

    /** Called when "Drop to Track" button is clicked
     * @param post The post to download and add to DAW project
     * @note Downloads audio file and places it in DAW project folder automatically.
     *       This is the one-click "Drop to Track" feature from README.
     */
    std::function<void(const FeedPost&)> onDropToTrackClicked;

    /** Called when "Download MIDI" button is clicked
     * @param post The post with MIDI to download
     * @note Only shown when post.hasMidi is true. Downloads .mid file.
     *       Part of R.3.3 Cross-DAW MIDI Collaboration feature.
     */
    std::function<void(const FeedPost&)> onDownloadMIDIClicked;

    /** Called when "Download Project" button is clicked
     * @param post The post with project file to download
     * @note Only shown when post.hasProjectFile is true. Downloads DAW project file.
     *       Part of R.3.4 Project File Exchange feature.
     */
    std::function<void(const FeedPost&)> onDownloadProjectClicked;

    /** Called when "Add to Playlist" button is clicked
     * @param post The post to add to playlist
     * @note Shows playlist selection dialog. Part of R.3.1 Collaborative Playlists feature.
     */
    std::function<void(const FeedPost&)> onAddToPlaylistClicked;

    /** Called when "Remix" button is clicked
     * @param post The post to remix
     * @param remixType What to remix: "audio", "midi", or "both"
     * @note Initiates remix flow - downloads source content and prepares for recording.
     *       Part of R.3.2 Remix Chains feature.
     */
    std::function<void(const FeedPost&, const juce::String& remixType)> onRemixClicked;

    /** Called when remix chain info is clicked (view remix lineage)
     * @param post The post whose remix chain to view
     * @note Shows the remix chain visualization. Part of R.3.2 Remix Chains feature.
     */
    std::function<void(const FeedPost&)> onRemixChainClicked;

    /** Called when card is tapped (for expanding details)
     * @param post The post that was tapped
     */
    std::function<void(const FeedPost&)> onCardTapped;

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
    bool isDownloading = false;
    float downloadProgress = 0.0f;

    // Like animation
    Animation likeAnimation{400, Animation::Easing::EaseOutCubic};

    // Fade-in animation for new posts
    AnimationValue<float> fadeInOpacity{0.0f, 300, Animation::Easing::EaseOutCubic};

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
    juce::Rectangle<int> getDropToTrackButtonBounds() const;
    juce::Rectangle<int> getDownloadMIDIButtonBounds() const;
    juce::Rectangle<int> getDownloadProjectButtonBounds() const;
    juce::Rectangle<int> getAddToPlaylistButtonBounds() const;
    juce::Rectangle<int> getRemixButtonBounds() const;
    juce::Rectangle<int> getRemixChainBadgeBounds() const;
    juce::Rectangle<int> getSaveButtonBounds() const;
    juce::Rectangle<int> getRepostButtonBounds() const;

    // Drawing helpers for save/repost
    void drawSaveButton(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawRepostButton(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawRepostAttribution(juce::Graphics& g);  // Shows "User reposted" header

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostCard)
};
