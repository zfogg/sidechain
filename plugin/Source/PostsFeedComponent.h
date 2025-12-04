#pragma once

#include <JuceHeader.h>
#include "FeedDataManager.h"
#include "FeedPost.h"
#include "PostCardComponent.h"
#include "AudioPlayer.h"

class NetworkClient;

class PostsFeedComponent : public juce::Component,
                           public juce::ScrollBar::Listener,
                           public juce::KeyListener
{
public:
    PostsFeedComponent();
    ~PostsFeedComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // KeyListener override for keyboard shortcuts
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // Callback for when user wants to go to profile
    std::function<void()> onGoToProfile;

    // Callback for logout
    std::function<void()> onLogout;

    // Callback for starting recording
    std::function<void()> onStartRecording;

    // Set user info
    void setUserInfo(const juce::String& username, const juce::String& email, const juce::String& profilePicUrl);

    // Network client integration
    void setNetworkClient(NetworkClient* client);

    // Audio player integration
    void setAudioPlayer(AudioPlayer* player);

    // Feed control
    void loadFeed();
    void refreshFeed();
    void switchFeedType(FeedDataManager::FeedType type);

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

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

    // Scroll state
    double scrollPosition = 0.0;
    int totalContentHeight = 0;
    static constexpr int POST_CARD_HEIGHT = 120;
    static constexpr int POST_CARD_SPACING = 10;

    //==============================================================================
    // User info
    juce::String username;
    juce::String email;
    juce::String profilePicUrl;

    //==============================================================================
    // Audio playback
    AudioPlayer* audioPlayer = nullptr;

    //==============================================================================
    // Network client for play tracking
    NetworkClient* networkClient = nullptr;

    //==============================================================================
    // UI Components
    juce::ScrollBar scrollBar { true }; // vertical
    juce::OwnedArray<PostCardComponent> postCards;

    //==============================================================================
    // UI layout constants
    static constexpr int TOP_BAR_HEIGHT = 70;
    static constexpr int FEED_TABS_HEIGHT = 50;

    //==============================================================================
    // UI helper methods
    void drawTopBar(juce::Graphics& g);
    void drawFeedTabs(juce::Graphics& g);
    void drawLoadingState(juce::Graphics& g);
    void drawEmptyState(juce::Graphics& g);
    void drawErrorState(juce::Graphics& g);
    void drawFeedPosts(juce::Graphics& g);
    void drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds, bool small = false);

    // Post card management
    void rebuildPostCards();
    void updatePostCardPositions();
    void setupPostCardCallbacks(PostCardComponent* card);
    void updateAudioPlayerPlaylist();

    // Feed callback handlers
    void onFeedLoaded(const FeedResponse& response);
    void onFeedError(const juce::String& error);

    // Infinite scroll
    void checkLoadMore();
    void updateScrollBounds();

    // Hit testing
    juce::Rectangle<int> getTimelineTabBounds() const;
    juce::Rectangle<int> getGlobalTabBounds() const;
    juce::Rectangle<int> getRefreshButtonBounds() const;
    juce::Rectangle<int> getRetryButtonBounds() const;
    juce::Rectangle<int> getRecordButtonBounds() const;
    juce::Rectangle<int> getFeedContentBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostsFeedComponent)
};