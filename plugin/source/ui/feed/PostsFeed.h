#pragma once

#include <JuceHeader.h>
#include "../../stores/FeedDataManager.h"
#include "../../models/FeedResponse.h"
#include "../../util/Animation.h"
#include "PostCard.h"
#include "Comment.h"
#include "../../audio/HttpAudioPlayer.h"
#include "../../models/Playlist.h"

class NetworkClient;
class StreamChatClient;

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
class PostsFeed : public juce::Component,
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

    // Callback for opening discovery/search
    std::function<void()> onGoToDiscovery;

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
    void switchFeedType(FeedDataManager::FeedType type);

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
    // Feed state
    enum class FeedState
    {
        Loading,    // Initial loading or refreshing
        Loaded,     // Successfully loaded with posts
        Empty,      // Loaded but no posts
        Error       // Error occurred
    };

    FeedState feedState = FeedState::Loading;
    juce::String errorMessage;
    juce::Array<FeedPost> posts;
    FeedDataManager feedDataManager;
    FeedDataManager::FeedType currentFeedType = FeedDataManager::FeedType::Timeline;

    // Real-time update state (5.5)
    int pendingNewPostsCount = 0;  // Count of new posts received while user is viewing feed
    juce::Time lastNewPostTime;    // Track when last new post notification arrived
    bool showingNewPostsToast = false;
    AnimationValue<float> toastOpacity{0.0f, 200, Animation::Easing::EaseOutCubic};  // Fade in/out animation

    // Scroll state
    double scrollPosition = 0.0;
    int totalContentHeight = 0;
    static constexpr int POST_CARD_HEIGHT = 120;
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
    AnimationValue<float> commentsPanelSlide{0.0f, 250, Animation::Easing::EaseOutCubic};  // 0.0 = hidden, 1.0 = visible
    juce::String currentUserId;

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

    // Feed callback handlers
    void onFeedLoaded(const FeedResponse& response);
    void onFeedError(const juce::String& error);

    // Presence querying
    void queryPresenceForPosts();

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