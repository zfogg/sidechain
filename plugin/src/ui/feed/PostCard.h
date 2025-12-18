#pragma once

#include "../../models/FeedPost.h"
#include "../../stores/AppStore.h"
#include "../../ui/animations/AnimationController.h"
#include "../../ui/animations/Easing.h"
#include "../../ui/animations/TransitionAnimation.h"
#include "../../ui/common/AppStoreComponent.h"
#include "../../util/HoverState.h"
#include "../../util/LongPressDetector.h"
#include "../common/WaveformImageView.h"
#include "EmojiReactionsPanel.h"
#include <JuceHeader.h>
#include <atomic>

// Forward declarations
class NetworkClient;

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
class PostCard : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::PostsState>,
                 public juce::Timer,
                 public juce::TooltipClient {
public:
  PostCard(Sidechain::Stores::AppStore *store = nullptr);
  ~PostCard() override;

  //==============================================================================
  // Data binding

  /** Set the post data to display
   * @param post The feed post to display
   */
  void setPost(const FeedPost &post);

  /** Get the current post data
   * @return Reference to the current post
   */
  const FeedPost &getPost() const {
    return post;
  }

  /** Get the post ID
   * @return The unique post identifier
   */
  juce::String getPostId() const {
    return post.id;
  }

  /** Set the network client for downloading waveform images
   * @param client Pointer to the NetworkClient instance
   */
  void setNetworkClient(NetworkClient *client);

  //==============================================================================
  // Update UI state (not persisted to PostsStore)
  // Note: Post data updates now come automatically via PostsStore subscription
  // (Task 2.5)

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
  void setPlaying(bool playing) {
    setIsPlaying(playing);
  }

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
  std::function<void(const FeedPost &)> onPlayClicked;

  /** Called when pause button is clicked */
  std::function<void(const FeedPost &)> onPauseClicked;

  /** Called when like button is toggled
   * @param post The post that was liked/unliked
   * @param liked true if post is now liked, false if unliked
   */
  std::function<void(const FeedPost &, bool liked)> onLikeToggled;

  /** Called when user selects an emoji reaction
   * @param post The post being reacted to
   * @param emoji The emoji string (empty to clear reaction)
   */
  std::function<void(const FeedPost &, const juce::String &emoji)> onEmojiReaction;

  /** Called when user avatar/name is clicked (navigate to profile) */
  std::function<void(const FeedPost &)> onUserClicked;

  /** Called when comment button is clicked
   * @note Currently not fully implemented - comment UI exists but not wired to
   * API See @ref notes/PLAN.md for comment system implementation status
   */
  std::function<void(const FeedPost &)> onCommentClicked;

  /** Called when share button is clicked (copies post URL to clipboard) */
  std::function<void(const FeedPost &)> onShareClicked;

  /** Called when "more" menu button is clicked
   * @warning NOT IMPLEMENTED - Currently only logs the action, no context menu
   * shown See @ref notes/PLAN.md for planned context menu features
   */
  std::function<void(const FeedPost &)> onMoreClicked;

  /** Called when save/bookmark button is toggled
   * @param post The post that was saved/unsaved
   * @param saved true if post is now saved, false if unsaved
   */
  std::function<void(const FeedPost &, bool saved)> onSaveToggled;

  /** Called when repost button is clicked
   * @param post The post to be reposted
   * @note Shows a confirmation dialog before reposting
   */
  std::function<void(const FeedPost &)> onRepostClicked;

  /** Called when waveform is clicked (seek to position)
   * @param post The post being seeked
   * @param position Normalized position (0.0 to 1.0) where waveform was clicked
   */
  std::function<void(const FeedPost &, float position)> onWaveformClicked;

  /** Called when follow/unfollow button is toggled
   * @param post The post whose author should be followed/unfollowed
   * @param willFollow true to follow, false to unfollow
   * @note UI updates optimistically and backend API call is implemented in
   * PostsFeed.cpp. Success/error notifications are shown to the user.
   */
  std::function<void(const FeedPost &, bool willFollow)> onFollowToggled;

  /** Called when "Add to DAW" button is clicked
   * @param post The post to download
   * @note Downloads audio file and saves to user-selected location.
   *       Success/error notifications are shown to the user (already
   * implemented).
   */
  std::function<void(const FeedPost &)> onAddToDAWClicked;

  /** Called when "Drop to Track" button is clicked
   * @param post The post to download and add to DAW project
   * @note Downloads audio file and places it in DAW project folder
   * automatically. This is the one-click "Drop to Track" feature from README.
   */
  std::function<void(const FeedPost &)> onDropToTrackClicked;

  /** Called when "Download MIDI" button is clicked
   * @param post The post with MIDI to download
   * @note Only shown when post.hasMidi is true. Downloads .mid file.
   *       Part of R.3.3 Cross-DAW MIDI Collaboration feature.
   */
  std::function<void(const FeedPost &)> onDownloadMIDIClicked;

  /** Called when "Download Project" button is clicked
   * @param post The post with project file to download
   * @note Only shown when post.hasProjectFile is true. Downloads DAW project
   * file. Part of R.3.4 Project File Exchange feature.
   */
  std::function<void(const FeedPost &)> onDownloadProjectClicked;

  /** Called when "Add to Playlist" button is clicked
   * @param post The post to add to playlist
   * @note Shows playlist selection dialog. Part of R.3.1 Collaborative
   * Playlists feature.
   */
  std::function<void(const FeedPost &)> onAddToPlaylistClicked;

  /** Called when "Remix" button is clicked
   * @param post The post to remix
   * @param remixType What to remix: "audio", "midi", or "both"
   * @note Initiates remix flow - downloads source content and prepares for
   * recording. Part of R.3.2 Remix Chains feature.
   */
  std::function<void(const FeedPost &, const juce::String &remixType)> onRemixClicked;

  /** Called when remix chain info is clicked (view remix lineage)
   * @param post The post whose remix chain to view
   * @note Shows the remix chain visualization. Part of R.3.2 Remix Chains
   * feature.
   */
  std::function<void(const FeedPost &)> onRemixChainClicked;

  /** Called when sound indicator is clicked (navigate to sound page)
   * @param soundId The ID of the sound to navigate to
   * @note Shows the sound page with all posts using this sound. Part of Feature
   * #15 Sound Pages.
   */
  std::function<void(const juce::String &soundId)> onSoundClicked;

  /** Called when card is tapped (for expanding details)
   * @param post The post that was tapped
   */
  std::function<void(const FeedPost &)> onCardTapped;

  /** Called when archive state is toggled
   * @param post The post that was archived/unarchived
   * @param archived true if post should be archived, false to unarchive
   * @note Used for "Archive" option in more menu
   */
  std::function<void(const FeedPost &, bool archived)> onArchiveToggled;

  /** Called when pin state is toggled
   * @param post The post that was pinned/unpinned
   * @param pinned true if post should be pinned, false to unpin
   * @note Only available for own posts. Max 3 pinned posts allowed.
   */
  std::function<void(const FeedPost &, bool pinned)> onPinToggled;

  //==============================================================================
  // Component overrides

  /** Paint the component
   * @param g Graphics context for drawing
   */
  void paint(juce::Graphics &g) override;

  /** Handle component resize
   */
  void resized() override;

  /** Handle mouse down events
   * @param event Mouse event details
   */
  void mouseDown(const juce::MouseEvent &event) override;

  /** Handle mouse up events
   * @param event Mouse event details
   */
  void mouseUp(const juce::MouseEvent &event) override;

  /** Handle mouse enter events
   * @param event Mouse event details
   */
  void mouseEnter(const juce::MouseEvent &event) override;

  /** Handle mouse exit events
   * @param event Mouse event details
   */
  void mouseExit(const juce::MouseEvent &event) override;

  //==============================================================================
  // Timer override for animations and long-press detection

  /** Timer callback for animations and long-press detection
   */
  void timerCallback() override;

  /** Get tooltip text based on current mouse position
   * @return Tooltip text for the element under the mouse, or empty string
   */
  juce::String getTooltip() override;

  //==============================================================================
  // Layout constants
  static constexpr int CARD_HEIGHT = 160;
  static constexpr int AVATAR_SIZE = 56;
  static constexpr int BADGE_HEIGHT = 26;
  static constexpr int BUTTON_SIZE = 36;
  static constexpr int RIGHT_PANEL_WIDTH = 200;

protected:
  void onAppStateChanged(const Sidechain::Stores::PostsState &state) override;
  void subscribeToAppStore() override;

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

  // Like animation - hearts bursting outward (managed by AnimationController)
  Sidechain::UI::Animations::AnimationHandle likeAnimationHandle;
  float likeAnimationProgress = 0.0f;

  // Fade-in animation for new posts (managed by AnimationController)
  Sidechain::UI::Animations::AnimationHandle fadeInAnimationHandle;
  float currentOpacity = 0.0f;

  // Long-press detector for emoji reactions panel
  LongPressDetector longPressDetector{400}; // 400ms threshold

  // Waveform image view (loads PNG from CDN)
  WaveformImageView waveformView;

  // User avatar image (loads from URL)
  juce::Image avatarImage;

  //==============================================================================
  // Drawing methods
  void drawBackground(juce::Graphics &g);
  void drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawUserInfo(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawFollowButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawWaveform(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawPlayButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawMetadataBadges(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawSocialButtons(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawReactionCounts(juce::Graphics &g, juce::Rectangle<int> likeBounds);
  void drawLikeAnimation(juce::Graphics &g);

  // Animation helpers
  void startLikeAnimation();

  // Emoji reactions helpers
  void showEmojiReactionsPanel();
  void handleEmojiSelected(const juce::String &emoji);

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
  juce::Rectangle<int> getPinButtonBounds() const;
  juce::Rectangle<int> getSoundBadgeBounds() const;

  // Drawing helpers for save/repost/pin
  void drawSaveButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawRepostButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawPinButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawRepostAttribution(juce::Graphics &g); // Shows "User reposted" header
  void drawPinnedBadge(juce::Graphics &g);       // Shows pin badge for pinned posts
  void drawSoundBadge(juce::Graphics &g);        // Shows sound usage indicator

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostCard)
};
