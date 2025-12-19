#pragma once

#include "../../audio/HttpAudioPlayer.h"
#include "../../models/FeedResponse.h"
#include "../../models/Playlist.h"
#include "../../stores/slices/AppSlices.h"
#include "../../ui/animations/AnimationController.h"
#include "../../ui/animations/Easing.h"
#include "../../ui/animations/TransitionAnimation.h"
#include "../common/AppStoreComponent.h"
#include "../common/ErrorState.h"
#include "../common/SkeletonLoader.h"
#include "../common/SmoothScrollable.h"
#include "AggregatedFeedCard.h"
#include "Comment.h"
#include "PostCard.h"
#include <JuceHeader.h>

class NetworkClient;
class StreamChatClient;

// ==============================================================================
/**
 * PostsFeed displays the main social feed of audio posts
 *
 * Features:
 * - Multiple feed types (Timeline, Trending, Global)
 * - Infinite scroll with pagination
 * - Real-time updates via WebSocket notifications
 * - Comments panel (slide-in overlay)
 * - New posts toast notification
 * - Keyboard navigation support
 * - Playback progress tracking
 * - Pull-to-refresh functionality
 *
 * Thread Safety:
 * - All UI operations must be on the message thread
 * - Network callbacks are automatically marshalled to message thread
 */
class PostsFeed : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::PostsState>,
                  public Sidechain::UI::SmoothScrollable,
                  public juce::KeyListener {
public:
  explicit PostsFeed(Sidechain::Stores::AppStore *appStore);
  ~PostsFeed() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // Bring Component::keyPressed into scope to avoid hiding warning
  using juce::Component::keyPressed;

  // KeyListener override for keyboard shortcuts
  bool keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) override;

  // Callback for when user wants to go to profile
  std::function<void()> onGoToProfile;

  // Callback for navigating to a specific user's profile
  std::function<void(const juce::String &userId)> onNavigateToProfile;

  // Callback for logout
  std::function<void()> onLogout;

  // Callback for authentication required (redirects to auth screen)
  std::function<void()> onAuthenticationRequired;

  // Callback for starting recording
  std::function<void()> onStartRecording;

  // Callback for starting remix recording
  // Parameters: sourcePostId, sourceStoryId, remixType ("audio", "midi", or
  // "both")
  std::function<void(const juce::String &sourcePostId, const juce::String &sourceStoryId,
                     const juce::String &remixType)>
      onStartRemix;

  // Callback for opening discovery/search
  std::function<void()> onGoToDiscovery;

  // Callback for sharing a post to DMs
  std::function<void(const FeedPost &post)> onSendPostToMessage;

  // Callback for navigating to a sound page
  std::function<void(const juce::String &soundId)> onSoundClicked;

  // Set user info
  void setUserInfo(const juce::String &username, const juce::String &email, const juce::String &profilePicUrl);

  // Network client integration
  void setNetworkClient(NetworkClient *client);

  // Stream chat client for presence queries
  void setStreamChatClient(StreamChatClient *client);

  // Audio player integration
  void setAudioPlayer(HttpAudioPlayer *player);

  // Feed control
  void loadFeed();
  void refreshFeed();
  void switchFeedType(Sidechain::Stores::FeedType type);

  // Real-time updates
  void handleNewPostNotification(const juce::var &postData);
  void handleLikeCountUpdate(const juce::String &postId, int likeCount);
  void handleFollowerCountUpdate(const juce::String &userId, int followerCount);
  void showNewPostsToast(int count);

  // Presence updates
  void updateUserPresence(const juce::String &userId, bool isOnline, const juce::String &status);

protected:
  // SmoothScrollable implementation
  virtual void onScrollUpdate(double newScrollPosition) override;
  virtual int getScrollableWidth(int scrollBarWidth) const override;
  virtual juce::String getComponentName() const override;

private:
  // ==============================================================================
  // Real-time update state
  int pendingNewPostsCount = 0; // Count of new posts received while user is viewing feed
  juce::Time lastNewPostTime;   // Track when last new post notification arrived
  bool showingNewPostsToast = false;
  Sidechain::UI::Animations::AnimationHandle toastAnimationHandle;
  float currentToastOpacity = 0.0f;

  // Scroll state
  double scrollPosition = 0.0;
  double targetScrollPosition = 0.0;
  Sidechain::UI::Animations::AnimationHandle scrollAnimationHandle;
  int totalContentHeight = 0;
  int previousContentHeight = 0;
  static constexpr int POST_CARD_HEIGHT = PostCard::CARD_HEIGHT; // Use PostCard's height constant
  static constexpr int POST_CARD_SPACING = 10;
  static constexpr int POSTS_TOP_PADDING = 16; // Top padding for posts container

  // ==============================================================================
  // User info (profile picture now displayed in central HeaderComponent)
  juce::String username;
  juce::String email;
  juce::String profilePicUrl;

  // ==============================================================================
  // Audio playback
  HttpAudioPlayer *audioPlayer = nullptr;

  // ==============================================================================
  // Network client for play tracking
  NetworkClient *networkClient = nullptr;

  // Stream chat client for presence queries
  StreamChatClient *streamChatClient = nullptr;

  // Posts slice for direct state management
  std::shared_ptr<Sidechain::Stores::Slices::PostsSlice> postsSlice;

  // ==============================================================================
  // Listen duration tracking (postId -> start time)
  std::map<juce::String, juce::Time> playbackStartTimes;

  // ==============================================================================
  // UI Components
  juce::ScrollBar scrollBar{true}; // vertical
  juce::OwnedArray<PostCard> postCards;
  juce::OwnedArray<AggregatedFeedCard> aggregatedCards;

  // Comments panel (slide-in overlay)
  std::unique_ptr<CommentsPanel> commentsPanel;
  bool commentsPanelVisible = false;
  Sidechain::UI::Animations::AnimationHandle commentsPanelAnimationHandle;
  float currentCommentsPanelSlide = 0.0f;
  juce::String currentUserId;

  // Error state component (shown when feedState == Error)
  std::unique_ptr<ErrorState> errorStateComponent;

  // Skeleton loader (shown when feedState == Loading)
  std::unique_ptr<FeedSkeleton> feedSkeleton;

  void showCommentsForPost(const FeedPost &post);
  void hideCommentsPanel();

  // ==============================================================================
  // UI layout constants (TOP_BAR removed - now handled by central
  // HeaderComponent)
  static constexpr int FEED_TABS_HEIGHT = 50;

  // ==============================================================================
  // UI helper methods
  void drawFeedTabs(juce::Graphics &g);
  void drawLoadingState(juce::Graphics &g);
  void drawEmptyState(juce::Graphics &g);
  void drawErrorState(juce::Graphics &g);
  void drawFeedPosts(juce::Graphics &g);
  void drawNewPostsToast(juce::Graphics &g);

  // Post card management
  void rebuildPostCards();
  void updatePostCardPositions();
  void setupPostCardCallbacks(PostCard *card);
  void updateAudioPlayerPlaylist();

  // Feed state handling (now from PostsStore subscription)
  void handleFeedStateChanged();

protected:
  void onAppStateChanged(const Sidechain::Stores::PostsState &state) override;
  void subscribeToAppStore() override;

private:
  // Presence querying
  void queryPresenceForPosts();

  // Remix flow
  void startRemixFlow(const FeedPost &post, const juce::String &remixType);

  // Infinite scroll
  void checkLoadMore();
  void updateScrollBounds();

  // Hit testing
  juce::Rectangle<int> getTimelineTabBounds() const;
  juce::Rectangle<int> getTrendingTabBounds() const;
  juce::Rectangle<int> getGlobalTabBounds() const;
  juce::Rectangle<int> getForYouTabBounds() const;
  juce::Rectangle<int> getRefreshButtonBounds() const;
  juce::Rectangle<int> getRetryButtonBounds() const;
  juce::Rectangle<int> getRecordButtonBounds() const;
  juce::Rectangle<int> getFeedContentBounds() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostsFeed)
};