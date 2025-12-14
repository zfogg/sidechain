#pragma once

#include <JuceHeader.h>
#include "../../stores/FeedStore.h"
#include "../../models/FeedResponse.h"
#include "../../ui/animations/TransitionAnimation.h"
#include "../../ui/animations/Easing.h"
#include "../../util/reactive/ReactiveBoundComponent.h"
#include "PostCard.h"
#include "Comment.h"
#include "../../audio/HttpAudioPlayer.h"
#include "../../models/Playlist.h"
#include "../common/ErrorState.h"
#include "../common/SkeletonLoader.h"

class NetworkClient;
class StreamChatClient;

// Forward declarations
namespace Sidechain { namespace Stores { class FeedStore; } }

//==============================================================================
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
class PostsFeed : public Sidechain::Util::ReactiveBoundComponent,
                           public juce::ScrollBar::Listener,
                           public juce::KeyListener,
                           public juce::Timer
{
public:
    PostsFeed();
    ~PostsFeed() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Bring Component::keyPressed into scope to avoid hiding warning
    using juce::Component::keyPressed;

    // KeyListener override for keyboard shortcuts
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // Callback for when user wants to go to profile
    std::function<void()> onGoToProfile;

    // Callback for navigating to a specific user's profile
    std::function<void(const juce::String& userId)> onNavigateToProfile;

    // Callback for logout
    std::function<void()> onLogout;

    // Callback for starting recording
    std::function<void()> onStartRecording;

    // Callback for starting remix recording (R.3.2 Remix Chains)
    // Parameters: sourcePostId, sourceStoryId, remixType ("audio", "midi", or "both")
    std::function<void(const juce::String& sourcePostId, const juce::String& sourceStoryId, const juce::String& remixType)> onStartRemix;

    // Callback for opening discovery/search
    std::function<void()> onGoToDiscovery;

    // Callback for sharing a post to DMs
    std::function<void(const FeedPost& post)> onSendPostToMessage;

    // Callback for navigating to a sound page (Feature #15)
    std::function<void(const juce::String& soundId)> onSoundClicked;

    // Set user info
    void setUserInfo(const juce::String& username, const juce::String& email, const juce::String& profilePicUrl);

    // Network client integration
    void setNetworkClient(NetworkClient* client);

    // Stream chat client for presence queries
    void setStreamChatClient(StreamChatClient* client);

    // Audio player integration
    void setAudioPlayer(HttpAudioPlayer* player);

    // Feed control
    void loadFeed();
    void refreshFeed();
    void switchFeedType(Sidechain::Stores::FeedType type);

    // Real-time updates (5.5)
    void handleNewPostNotification(const juce::var& postData);
    void handleLikeCountUpdate(const juce::String& postId, int likeCount);
    void handleFollowerCountUpdate(const juce::String& userId, int followerCount);
    void showNewPostsToast(int count);

    // Presence updates (6.5.2.7)
    void updateUserPresence(const juce::String& userId, bool isOnline, const juce::String& status);

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    // Timer override
    void timerCallback() override;

private:
    //==============================================================================
    // Feed store subscription (Task 2.6 - reactive refactoring)
    Sidechain::Stores::FeedStore* feedStore = nullptr;
    std::function<void()> storeUnsubscribe;

    // Real-time update state (5.5)
    int pendingNewPostsCount = 0;  // Count of new posts received while user is viewing feed
    juce::Time lastNewPostTime;    // Track when last new post notification arrived
    bool showingNewPostsToast = false;
    std::shared_ptr<Sidechain::UI::Animations::TransitionAnimation<float>> toastAnimation;
    float currentToastOpacity = 0.0f;

    // Scroll state
    double scrollPosition = 0.0;
    int totalContentHeight = 0;
    static constexpr int POST_CARD_HEIGHT = PostCard::CARD_HEIGHT;  // Use PostCard's height constant
    static constexpr int POST_CARD_SPACING = 10;

    //==============================================================================
    // User info (profile picture now displayed in central HeaderComponent)
    juce::String username;
    juce::String email;
    juce::String profilePicUrl;

    //==============================================================================
    // Audio playback
    HttpAudioPlayer* audioPlayer = nullptr;

    //==============================================================================
    // Network client for play tracking
    NetworkClient* networkClient = nullptr;

    // Stream chat client for presence queries
    StreamChatClient* streamChatClient = nullptr;

    //==============================================================================
    // Listen duration tracking (postId -> start time)
    std::map<juce::String, juce::Time> playbackStartTimes;

    //==============================================================================
    // UI Components
    juce::ScrollBar scrollBar { true }; // vertical
    juce::OwnedArray<PostCard> postCards;

    // Comments panel (slide-in overlay)
    std::unique_ptr<CommentsPanel> commentsPanel;
    bool commentsPanelVisible = false;
    std::shared_ptr<Sidechain::UI::Animations::TransitionAnimation<float>> commentsPanelAnimation;
    float currentCommentsPanelSlide = 0.0f;
    juce::String currentUserId;

    // Error state component (shown when feedState == Error)
    std::unique_ptr<ErrorState> errorStateComponent;

    // Skeleton loader (shown when feedState == Loading)
    std::unique_ptr<FeedSkeleton> feedSkeleton;

    void showCommentsForPost(const FeedPost& post);
    void hideCommentsPanel();

    //==============================================================================
    // UI layout constants (TOP_BAR removed - now handled by central HeaderComponent)
    static constexpr int FEED_TABS_HEIGHT = 50;

    //==============================================================================
    // UI helper methods
    void drawFeedTabs(juce::Graphics& g);
    void drawLoadingState(juce::Graphics& g);
    void drawEmptyState(juce::Graphics& g);
    void drawErrorState(juce::Graphics& g);
    void drawFeedPosts(juce::Graphics& g);
    void drawNewPostsToast(juce::Graphics& g); // 5.5.2

    // Post card management
    void rebuildPostCards();
    void updatePostCardPositions();
    void setupPostCardCallbacks(PostCard* card);
    void updateAudioPlayerPlaylist();

    // Feed state handling (Task 2.6 - now from FeedStore subscription)
    void handleFeedStateChanged();

    // Presence querying
    void queryPresenceForPosts();

    // Remix flow (R.3.2)
    void startRemixFlow(const FeedPost& post, const juce::String& remixType);

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