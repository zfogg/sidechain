#pragma once

#include "../../models/FeedPost.h"
#include "../../stores/AppStore.h"
#include "../../ui/animations/AnimationController.h"
#include "../common/AppStoreComponent.h"
#include "../common/ErrorState.h"
#include "../social/FollowersList.h"
#include "../stories/StoryHighlights.h"
#include <JuceHeader.h>

class NetworkClient;
class StreamChatClient;
class PostCard;

//==============================================================================
/**
 * UserProfile represents the profile data fetched from the backend
 */
struct UserProfile {
  juce::String id;
  juce::String username;
  juce::String displayName;
  juce::String bio;
  juce::String location;
  juce::String avatarUrl;
  juce::String profilePictureUrl;
  juce::String dawPreference;
  juce::String genre;
  juce::var socialLinks; // JSON object with platform keys
  int followerCount = 0;
  int followingCount = 0;
  int postCount = 0;
  bool isFollowing = false;
  bool isFollowedBy = false;
  bool isPrivate = false;
  bool isMuted = false;             // Mute users without blocking
  juce::String followRequestStatus; // "pending", "accepted", "rejected", or empty
  juce::String followRequestId;
  juce::Time createdAt;

  // Online status (presence)
  bool isOnline = false;
  bool isInStudio = false;
  juce::String lastActive;

  static UserProfile fromJson(const juce::var &json);
  juce::String getAvatarUrl() const;
  juce::String getMemberSince() const;
  bool isOwnProfile(const juce::String &currentUserId) const;
};

//==============================================================================
/**
 * Profile displays a user's profile with their posts
 *
 * Features:
 * - Profile header (avatar, display name, username, bio)
 * - Follower/following counts (tappable)
 * - User's posts in a scrollable list
 * - Follow/unfollow button (for other users)
 * - Edit profile button (for own profile)
 * - Social links display
 * - Genre tags
 * - Member since date
 * - Profile sharing
 */
class Profile : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::UserState>,
                public juce::ScrollBar::Listener,
                public juce::TooltipClient {
public:
  Profile(Sidechain::Stores::AppStore *store = nullptr);
  ~Profile() override;

  //==============================================================================
  // Data binding
  void setNetworkClient(NetworkClient *client);
  void setStreamChatClient(StreamChatClient *client);
  void setCurrentUserId(const juce::String &userId);
  void loadProfile(const juce::String &userId);
  void loadOwnProfile();
  void setProfile(const UserProfile &profile);
  const UserProfile &getProfile() const {
    return profile;
  }

  //==============================================================================
  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void(const juce::String &userId)> onFollowersClicked;
  std::function<void(const juce::String &userId)> onFollowingClicked;
  std::function<void()> onEditProfile;
  std::function<void()> onSavedPostsClicked;           // Navigate to saved posts view (own profile only)
  std::function<void()> onArchivedPostsClicked;        // Navigate to archived posts
                                                       // view (own profile only)
  std::function<void()> onNotificationSettingsClicked; // Navigate to notification settings (own
                                                       // profile only)
  std::function<void()> onActivityStatusClicked;       // Navigate to activity status
                                                       // settings (own profile only)
  std::function<void()> onTwoFactorSettingsClicked;    // Navigate to 2FA settings (own profile only)
  std::function<void(const FeedPost &)> onPostClicked;
  std::function<void(const FeedPost &)> onPlayClicked;
  std::function<void(const FeedPost &)> onPauseClicked;
  std::function<void(const juce::String &userId)> onFollowToggled;
  std::function<void(const juce::String &userId, bool isMuted)> onMuteToggled; // Mute/unmute user
  std::function<void()> onMutedUsersClicked;                          // Navigate to muted users list (own profile only)
  std::function<void(const juce::String &userId)> onMessageClicked;   // Opens DM with user
  std::function<void(const juce::String &userId)> onViewStoryClicked; // Opens story viewer for user's story
  std::function<void(const juce::String &userId)> onNavigateToProfile; // Navigates to another user's profile
  std::function<void(const StoryHighlight &)> onHighlightClicked;      // Opens story viewer for a highlight
  std::function<void()> onCreateHighlightClicked;                      // Opens create highlight flow
                                                                       // (own profile only)

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;
  void scrollBarMoved(juce::ScrollBar *scrollBarThatHasMoved, double newRangeStart) override;
  juce::String getTooltip() override;

  //==============================================================================
  // Post playback state
  void setCurrentlyPlayingPost(const juce::String &postId);
  void setPlaybackProgress(float progress);
  void clearPlayingState();

  //==============================================================================
  // Refresh
  void refresh();

  //==============================================================================
  // Presence updates (6.5.2.7)
  void updateUserPresence(const juce::String &userId, bool isOnline, const juce::String &status);

private:
  //==============================================================================
  // Data
  UserProfile profile; // For other users OR populated from UserStore for own profile
  juce::String currentUserId;
  juce::Array<FeedPost> userPosts;
  NetworkClient *networkClient = nullptr;
  StreamChatClient *streamChatClient = nullptr;

  // Loading/error states (For other users; own profile uses UserStore state)
  bool isLoading = false;
  bool hasError = false;
  juce::String errorMessage;

  // Scroll state
  double scrollOffset = 0.0;
  double targetScrollOffset = 0.0;
  Sidechain::UI::Animations::AnimationHandle scrollAnimationHandle;
  std::unique_ptr<juce::ScrollBar> scrollBar;

  // Cached avatar image
  juce::Image avatarImage;

  // Story state
  bool hasActiveStory = false;

  // Post cards
  juce::OwnedArray<PostCard> postCards;
  juce::String currentlyPlayingPostId;
  float currentPlaybackProgress = 0.0f;

  //==============================================================================
  // Drawing methods
  void drawBackground(juce::Graphics &g);
  void drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawUserInfo(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawStats(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawActionButtons(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawBio(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawSocialLinks(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawGenreTags(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawMemberSince(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawPosts(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawLoadingState(juce::Graphics &g);
  void drawErrorState(juce::Graphics &g);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds);

  //==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getAvatarBounds() const;
  juce::Rectangle<int> getFollowersBounds() const;
  juce::Rectangle<int> getFollowingBounds() const;
  juce::Rectangle<int> getFollowButtonBounds() const;
  juce::Rectangle<int> getMessageButtonBounds() const;
  juce::Rectangle<int> getMuteButtonBounds() const; // Mute button (for other users)
  juce::Rectangle<int> getEditButtonBounds() const;
  juce::Rectangle<int> getSavedPostsButtonBounds() const;
  juce::Rectangle<int> getArchivedPostsButtonBounds() const;
  juce::Rectangle<int> getNotificationSettingsButtonBounds() const;
  juce::Rectangle<int> getActivityStatusButtonBounds() const;
  juce::Rectangle<int> getTwoFactorSettingsButtonBounds() const; // 2FA settings (own profile only)
  juce::Rectangle<int> getMutedUsersButtonBounds() const;        // Muted users list (own profile only)
  juce::Rectangle<int> getShareButtonBounds() const;
  juce::Rectangle<int> getSocialLinkBounds(int index) const;
  juce::Rectangle<int> getPostsAreaBounds() const;

  //==============================================================================
  // Network operations
  void fetchProfile(const juce::String &userId);
  void fetchUserPosts(const juce::String &userId);
  void handleFollowToggle();
  void handleMuteToggle(); // Mute/unmute toggle
  void shareProfile();

  // Presence querying
  void queryPresenceForProfile();

  // Check if user has active stories
  void checkForActiveStories(const juce::String &userId);

  // Sync follow state from PostsStore to local userPosts array
  void syncFollowStateFromPostsStore();

  //==============================================================================
  // AppStoreComponent overrides
protected:
  void onAppStateChanged(const Sidechain::Stores::UserState &state) override;
  void subscribeToAppStore() override;

private:
  //==============================================================================
  // Helpers
  void updatePostCards();
  int calculateContentHeight() const;
  bool isOwnProfile() const;

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 280;
  static constexpr int AVATAR_SIZE = 100;
  static constexpr int STAT_WIDTH = 80;
  static constexpr int BUTTON_HEIGHT = 36;
  static constexpr int POST_CARD_HEIGHT = 120;
  static constexpr int PADDING = 20;

  //==============================================================================
  // Colors
  struct Colors {
    static inline juce::Colour background{0xff1a1a1e};
    static inline juce::Colour headerBg{0xff252529};
    static inline juce::Colour cardBg{0xff2d2d32};
    static inline juce::Colour textPrimary{0xffffffff};
    static inline juce::Colour textSecondary{0xffa0a0a0};
    static inline juce::Colour accent{0xff00d4ff};
    static inline juce::Colour accentHover{0xff00b8e0};
    static inline juce::Colour followButton{0xff00d4ff};
    static inline juce::Colour followingButton{0xff3a3a3e};
    static inline juce::Colour badge{0xff3a3a3e};
    static inline juce::Colour link{0xff00d4ff};
    static inline juce::Colour errorRed{0xffff4757};
  };

  //==============================================================================
  // Story Highlights component
  std::unique_ptr<StoryHighlights> storyHighlights;
  static constexpr int HIGHLIGHTS_HEIGHT = 96; // Height of highlights row

  //==============================================================================
  // Followers/Following list panel
  std::unique_ptr<FollowersList> followersListPanel;
  bool followersListVisible = false;

  //==============================================================================
  // Error state component (shown when profile loading fails)
  std::unique_ptr<ErrorState> errorStateComponent;

  void showFollowersList(const juce::String &userId, FollowersList::ListType type);
  void hideFollowersList();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Profile)
};
