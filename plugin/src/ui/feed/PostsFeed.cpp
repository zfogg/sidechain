#include "PostsFeed.h"
#include "../../network/NetworkClient.h"
#include "../../network/StreamChatClient.h"
#include "../../util/Colors.h"
#include "../../util/Json.h"
#include "../../util/UIHelpers.h"
#include "../../util/Log.h"
#include "../../util/Async.h"
#include "../../ui/animations/TransitionAnimation.h"
#include "../../ui/animations/Easing.h"
#include "../../util/Result.h"
#include "../../util/DAWProjectFolder.h"
#include "../common/ToastNotification.h"
#include <set>
#include <vector>

using namespace Sidechain::UI::Animations;

//==============================================================================
// Local state enum for feed display
enum class FeedState
{
    Loading,
    Loaded,
    Empty,
    Error
};

static FeedState feedState = FeedState::Loading;

//==============================================================================
PostsFeed::PostsFeed()
{
    using namespace Sidechain::Stores;

    Log::info("PostsFeed: Initializing feed component");
    setSize(1000, 800);

    // Get FeedStore instance and subscribe (Task 2.6)
    feedStore = &FeedStore::getInstance();
    storeUnsubscribe = feedStore->subscribe([this](const FeedStoreState& state) {
        // FeedStore state changed - rebuild UI
        handleFeedStateChanged();
    });
    Log::debug("PostsFeed: Subscribed to FeedStore");

    // Add scroll bar
    addAndMakeVisible(scrollBar);
    scrollBar.addListener(this);
    scrollBar.setColour(juce::ScrollBar::thumbColourId, SidechainColors::surface());
    scrollBar.setColour(juce::ScrollBar::trackColourId, SidechainColors::backgroundLight());
    Log::debug("PostsFeedComponent: Scroll bar created and configured");

    // Enable keyboard focus for shortcuts
    setWantsKeyboardFocus(true);
    addKeyListener(this);
    Log::debug("PostsFeedComponent: Keyboard focus enabled for shortcuts");

    // Create comments panel (initially hidden)
    commentsPanel = std::make_unique<CommentsPanel>();
    commentsPanel->onClose = [this]() {
        Log::debug("PostsFeedComponent: Comments panel close requested");
        hideCommentsPanel();
    };

    // Set up comments panel slide animation
    // Animations will be created when needed (not in constructor)
    commentsPanel->onUserClicked = [this](const juce::String& userId) {
        Log::debug("PostsFeedComponent: User clicked in comments panel: " + userId);
        hideCommentsPanel();
        if (onNavigateToProfile && userId.isNotEmpty())
            onNavigateToProfile(userId);
        else
            Log::warn("PostsFeedComponent: User clicked in comments but callback not set or userId empty");
    };
    addChildComponent(commentsPanel.get());
    Log::debug("PostsFeedComponent: Comments panel created");

    // Create error state component (initially hidden)
    errorStateComponent = std::make_unique<ErrorState>();
    errorStateComponent->setErrorType(ErrorState::ErrorType::Network);
    errorStateComponent->setPrimaryAction("Try Again", [this]() {
        Log::info("PostsFeed: Retry requested from error state");
        loadFeed();
    });
    addChildComponent(errorStateComponent.get());
    Log::debug("PostsFeedComponent: Error state component created");

    // Create feed skeleton (shown during initial loading)
    feedSkeleton = std::make_unique<FeedSkeleton>(4);  // Show 4 skeleton cards
    addChildComponent(feedSkeleton.get());
    Log::debug("PostsFeedComponent: Feed skeleton created");

    Log::info("PostsFeedComponent: Initialization complete");
}

PostsFeed::~PostsFeed()
{
    Log::debug("PostsFeed: Destroying feed component");

    // Unsubscribe from FeedStore (Task 2.6)
    if (storeUnsubscribe)
        storeUnsubscribe();

    removeKeyListener(this);
    scrollBar.removeListener(this);
}

//==============================================================================
void PostsFeed::setUserInfo(const juce::String& user, const juce::String& userEmail, const juce::String& picUrl)
{
    // Store user info (profile picture now displayed in central HeaderComponent)
    username = user;
    email = userEmail;
    profilePicUrl = picUrl;
    repaint();
}

void PostsFeed::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    // Set NetworkClient on FeedStore (Task 2.6)
    if (feedStore)
        feedStore->setNetworkClient(client);
    Log::info("PostsFeed::setNetworkClient: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void PostsFeed::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;
    Log::info("PostsFeed::setStreamChatClient: StreamChatClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void PostsFeed::setAudioPlayer(HttpAudioPlayer* player)
{
    Log::info("PostsFeed::setAudioPlayer: Setting audio player " + juce::String(player != nullptr ? "(valid)" : "(null)"));
    audioPlayer = player;

    if (audioPlayer)
    {
        // Set up progress callback to update post cards
        audioPlayer->onProgressUpdate = [this](const juce::String& postId, double progress) {
            Log::debug("PostsFeedComponent: Audio progress update - postId: " + postId + ", progress: " + juce::String(progress, 2));
            // Find the card for this post and update its playback progress
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setPlaybackProgress(static_cast<float>(progress));
                    break;
                }
            }
        };

        // Handle playback state changes
        audioPlayer->onPlaybackStarted = [this](const juce::String& postId) {
            Log::info("PostsFeedComponent: Playback started - postId: " + postId);
            // Update all cards - only the playing one should show as playing
            for (auto* card : postCards)
            {
                card->setPlaying(card->getPostId() == postId);
            }

            // Record playback start time for duration tracking
            playbackStartTimes[postId] = juce::Time::getCurrentTime();
            Log::debug("PostsFeedComponent: Playback start time recorded for postId: " + postId);

            // Track the play in the backend
            if (networkClient != nullptr)
            {
                Log::debug("PostsFeedComponent: Tracking play in backend for postId: " + postId);
                networkClient->trackPlay(postId, [this, postId](Outcome<juce::var> responseOutcome) {
                    if (responseOutcome.isOk())
                    {
                        Log::debug("PostsFeedComponent: Play tracking successful for postId: " + postId);
                        // Update play count in UI if returned in response
                        auto response = responseOutcome.getValue();
                        int newPlayCount = Json::getInt(response, "play_count", -1);
                        if (newPlayCount >= 0)
                        {
                            Log::debug("PostsFeedComponent: Updating play count to " + juce::String(newPlayCount) + " for postId: " + postId);
                            // Note: PostCard now updates automatically via FeedStore subscription (Task 2.5)
                            // Manual update calls have been removed
                        }
                    }
                    else
                    {
                        Log::warn("PostsFeedComponent: Play tracking failed for postId: " + postId);
                    }
                });

                // Track recommendation click for CTR analysis (Gorse optimization)
                if (feedStore != nullptr)
                {
                    using namespace Sidechain::Stores;
                    auto currentFeedType = feedStore->getCurrentFeedType();
                    const auto& currentFeed = feedStore->getState().getCurrentFeed();

                    // Map feed type to source string for backend
                    juce::String source = "unknown";
                    switch (currentFeedType)
                    {
                        case FeedType::ForYou:    source = "for-you"; break;
                        case FeedType::Popular:   source = "popular"; break;
                        case FeedType::Latest:    source = "latest"; break;
                        case FeedType::Discovery: source = "discovery"; break;
                        case FeedType::Trending:  source = "trending"; break;
                        default: break;
                    }

                    // Find position of post in current feed
                    int position = -1;
                    for (int i = 0; i < currentFeed.posts.size(); ++i)
                    {
                        if (currentFeed.posts[i].id == postId)
                        {
                            position = i;
                            break;
                        }
                    }

                    // Only track if this is a recommendation feed (not Timeline/Global)
                    if (source != "unknown" && position >= 0)
                    {
                        Log::debug("PostsFeedComponent: Tracking recommendation click: postId=" + postId +
                                   " source=" + source + " position=" + juce::String(position));

                        networkClient->trackRecommendationClick(postId, source, position, 0.0, false, nullptr);
                    }
                }
            }
            else
            {
                Log::warn("PostsFeedComponent: Cannot track play - NetworkClient is null");
            }
        };

        audioPlayer->onPlaybackPaused = [this](const juce::String& postId) {
            Log::info("PostsFeedComponent: Playback paused - postId: " + postId);
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setPlaying(false);
                    break;
                }
            }
        };

        audioPlayer->onPlaybackStopped = [this](const juce::String& postId) {
            Log::info("PostsFeedComponent: Playback stopped - postId: " + postId);
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setPlaying(false);
                    card->setPlaybackProgress(0.0f);
                    break;
                }
            }

            // Track listen duration
            auto it = playbackStartTimes.find(postId);
            if (it != playbackStartTimes.end() && networkClient != nullptr)
            {
                juce::Time startTime = it->second;
                juce::Time endTime = juce::Time::getCurrentTime();
                double durationSeconds = (endTime.toMilliseconds() - startTime.toMilliseconds()) / 1000.0;

                Log::debug("PostsFeedComponent: Playback duration calculated - postId: " + postId + ", duration: " + juce::String(durationSeconds, 2) + "s");

                // Only track if duration is meaningful (at least 1 second)
                if (durationSeconds >= 1.0)
                {
                    Log::debug("PostsFeedComponent: Tracking listen duration for postId: " + postId);
                    networkClient->trackListenDuration(postId, durationSeconds, [postId](Outcome<juce::var> responseOutcome) {
                        if (responseOutcome.isOk())
                        {
                            Log::debug("PostsFeedComponent: Listen duration tracked successfully for postId: " + postId);
                        }
                        else
                        {
                            Log::warn("PostsFeedComponent: Listen duration tracking failed for postId: " + postId);
                        }
                    });
                }
                else
                {
                    Log::debug("PostsFeedComponent: Listen duration too short to track (" + juce::String(durationSeconds, 2) + "s < 1.0s)");
                }

                // Remove from tracking map
                playbackStartTimes.erase(it);
            }
            else if (it == playbackStartTimes.end())
            {
                Log::warn("PostsFeedComponent: No playback start time found for postId: " + postId);
            }
        };

        audioPlayer->onLoadingStarted = [this](const juce::String& postId) {
            Log::debug("PostsFeedComponent: Audio loading started - postId: " + postId);
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setLoading(true);
                    break;
                }
            }
        };

        audioPlayer->onLoadingComplete = [this](const juce::String& postId, bool success) {
            Log::debug("PostsFeedComponent: Audio loading complete - postId: " + postId + ", success: " + juce::String(success ? "true" : "false"));
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setLoading(false);
                    break;
                }
            }
        };

        Log::debug("PostsFeedComponent: Audio player callbacks configured");
    }
}

//==============================================================================
void PostsFeed::loadFeed()
{
    using namespace Sidechain::Stores;

    Log::info("PostsFeed::loadFeed: Loading feed via FeedStore (Task 2.6)");

    // FeedStore will handle loading and notify via subscription
    if (feedStore)
    {
        feedStore->loadFeed(feedStore->getCurrentFeedType(), false);
        Log::debug("PostsFeed::loadFeed: FeedStore.loadFeed() called");
    }
    else
    {
        Log::error("PostsFeed::loadFeed: FeedStore is null!");
    }
}

void PostsFeed::refreshFeed()
{
    using namespace Sidechain::Stores;

    Log::info("PostsFeed::refreshFeed: Refreshing feed via FeedStore (Task 2.6)");

    // FeedStore will handle refresh and notify via subscription
    if (feedStore)
    {
        feedStore->refreshCurrentFeed();
        Log::debug("PostsFeed::refreshFeed: FeedStore.refreshCurrentFeed() called");
    }
    else
    {
        Log::error("PostsFeed::refreshFeed: FeedStore is null!");
    }
}

void PostsFeed::switchFeedType(Sidechain::Stores::FeedType type)
{
    using namespace Sidechain::Stores;

    juce::String typeStr = feedTypeToString(type);
    juce::String currentTypeStr = feedTypeToString(feedStore ? feedStore->getCurrentFeedType() : FeedType::Timeline);

    if (feedStore && feedStore->getCurrentFeedType() == type)
    {
        Log::debug("PostsFeed::switchFeedType: Already on feed type: " + typeStr);
        return;
    }

    Log::info("PostsFeed::switchFeedType: Switching from " + currentTypeStr + " to " + typeStr + " (Task 2.6)");

    // Reset scroll position when switching feeds
    scrollPosition = 0.0;
    scrollBar.setCurrentRangeStart(0.0);

    // FeedStore will handle feed switching and notify via subscription
    if (feedStore)
    {
        feedStore->switchFeedType(type);
        Log::debug("PostsFeed::switchFeedType: FeedStore.switchFeedType() called");
    }
    else
    {
        Log::error("PostsFeed::switchFeedType: FeedStore is null!");
    }
}

//==============================================================================
void PostsFeed::handleFeedStateChanged()
{
    using namespace Sidechain::Stores;

    if (!feedStore)
    {
        Log::error("PostsFeed::handleFeedStateChanged: FeedStore is null!");
        return;
    }

    const auto& state = feedStore->getState();
    const auto& currentFeed = state.getCurrentFeed();

    Log::debug("PostsFeed::handleFeedStateChanged: State changed - loading: " + juce::String(currentFeed.isLoading ? "true" : "false") +
               ", error: " + currentFeed.error + ", posts: " + juce::String(currentFeed.posts.size()));

    // Handle loading state
    if (currentFeed.isLoading || currentFeed.isRefreshing)
    {
        feedState = FeedState::Loading;
        if (feedSkeleton != nullptr)
            feedSkeleton->setVisible(true);
        if (errorStateComponent != nullptr)
            errorStateComponent->setVisible(false);
        repaint();
        return;
    }

    // Handle error state
    if (currentFeed.error.isNotEmpty())
    {
        feedState = FeedState::Error;
        Log::error("PostsFeed::handleFeedStateChanged: Feed error - " + currentFeed.error);

        // Check if this is an authentication error - if so, redirect to auth screen
        if (currentFeed.error.containsIgnoreCase("not authenticated") ||
            currentFeed.error.containsIgnoreCase("unauthorized") ||
            currentFeed.error.containsIgnoreCase("401"))
        {
            Log::warn("PostsFeed: Authentication error detected - redirecting to auth screen");
            if (onAuthenticationRequired)
            {
                onAuthenticationRequired();
            }
            return;
        }

        if (feedSkeleton != nullptr)
            feedSkeleton->setVisible(false);

        if (errorStateComponent != nullptr)
        {
            errorStateComponent->configureFromError(currentFeed.error);
            errorStateComponent->setVisible(true);
        }

        repaint();
        return;
    }

    // Handle loaded state
    Log::info("PostsFeed::handleFeedStateChanged: Feed loaded - posts: " + juce::String(currentFeed.posts.size()));

    // Hide error state and skeleton on successful load
    if (errorStateComponent != nullptr)
        errorStateComponent->setVisible(false);
    if (feedSkeleton != nullptr)
        feedSkeleton->setVisible(false);

    // Determine if feed is empty or loaded
    if (currentFeed.posts.isEmpty())
        feedState = FeedState::Empty;
    else
        feedState = FeedState::Loaded;

    // Rebuild post cards from FeedStore state
    rebuildPostCards();
    updateScrollBounds();
    updateAudioPlayerPlaylist();

    // Query presence for all unique post authors
    queryPresenceForPosts();

    repaint();
}

//==============================================================================
void PostsFeed::queryPresenceForPosts()
{
    using namespace Sidechain::Stores;

    if (!streamChatClient || !feedStore)
    {
        Log::debug("PostsFeed::queryPresenceForPosts: Skipping - streamChatClient or feedStore is null");
        return;
    }

    const auto& posts = feedStore->getState().getCurrentFeed().posts;
    if (posts.isEmpty())
    {
        Log::debug("PostsFeed::queryPresenceForPosts: Skipping - no posts");
        return;
    }

    // Collect unique user IDs from posts
    std::set<juce::String> uniqueUserIds;
    for (const auto& post : posts)
    {
        if (post.userId.isNotEmpty() && !post.isOwnPost)
        {
            uniqueUserIds.insert(post.userId);
        }
    }

    if (uniqueUserIds.empty())
    {
        Log::debug("PostsFeed::queryPresenceForPosts: No unique user IDs to query");
        return;
    }

    // Convert to vector for queryPresence
    std::vector<juce::String> userIds(uniqueUserIds.begin(), uniqueUserIds.end());

    Log::debug("PostsFeed::queryPresenceForPosts: Querying presence for " + juce::String(userIds.size()) + " users");

    // Query presence
    streamChatClient->queryPresence(userIds, [this](Outcome<std::vector<StreamChatClient::UserPresence>> result) {
        if (result.isError())
        {
            Log::warn("PostsFeed::queryPresenceForPosts: Failed to query presence: " + result.getError());
            return;
        }

        auto presenceList = result.getValue();
        Log::debug("PostsFeed::queryPresenceForPosts: Received presence data for " + juce::String(presenceList.size()) + " users");

        // Update PostCards with presence data
        for (const auto& presence : presenceList)
        {
            for (auto* card : postCards)
            {
                if (card->getPost().userId == presence.userId)
                {
                    auto updatedPost = card->getPost();
                    updatedPost.isOnline = presence.online;
                    updatedPost.isInStudio = (presence.status == "in_studio" || presence.status == "in studio");
                    card->setPost(updatedPost);
                }
            }
        }

        // Repaint to show updated online indicators
        repaint();
    });
}

void PostsFeed::updateUserPresence(const juce::String& userId, bool isOnline, const juce::String& status)
{
    if (userId.isEmpty())
        return;

    bool isInStudio = (status == "in_studio" || status == "in studio" || status == "recording");

    // Update PostCards with presence data (Task 2.6 - no local posts array)
    for (auto* card : postCards)
    {
        if (card->getPost().userId == userId)
        {
            auto updatedPost = card->getPost();
            updatedPost.isOnline = isOnline;
            updatedPost.isInStudio = isInStudio;
            card->setPost(updatedPost);
        }
    }

    // Repaint to show updated online indicators
    repaint();
}

//==============================================================================
void PostsFeed::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(SidechainColors::background());

    // Feed type tabs (top bar now handled by central HeaderComponent)
    drawFeedTabs(g);

    // Clip content area so posts scroll under the tabs header
    {
        juce::Graphics::ScopedSaveState saveState(g);
        g.reduceClipRegion(getFeedContentBounds());

        // Main feed area based on state
        switch (feedState)
        {
            case FeedState::Loading:
            // FeedSkeleton component handles the loading UI as a child component
            // Just ensure background is drawn (already done above)
            break;
        case FeedState::Loaded:
            drawFeedPosts(g);
            // Draw toast on top of feed if showing (5.5.2)
            if (showingNewPostsToast && pendingNewPostsCount > 0)
            {
                drawNewPostsToast(g);
            }
            break;
        case FeedState::Empty:
            drawEmptyState(g);
            break;
        case FeedState::Error:
            // ErrorState component handles the error UI as a child component
            // Just ensure background is drawn (already done above)
            break;
        }
    } // End clip region scope
}

void PostsFeed::drawFeedTabs(juce::Graphics& g)
{
    // Tabs now start at top (header handled by central HeaderComponent)
    auto tabsBounds = getLocalBounds().withHeight(FEED_TABS_HEIGHT);

    // Tabs background
    g.setColour(SidechainColors::background());
    g.fillRect(tabsBounds);

    // Get current feed type from store
    using namespace Sidechain::Stores;
    auto currentFeedType = feedStore ? feedStore->getCurrentFeedType() : FeedType::Timeline;

    // Timeline (Following) tab
    auto timelineTab = getTimelineTabBounds();
    bool isTimelineActive = (currentFeedType == FeedType::Timeline);

    // Use UIHelpers::drawButton for consistent tab styling
    if (isTimelineActive)
    {
        UIHelpers::drawButton(g, timelineTab.reduced(5), "Following",
            SidechainColors::primary(), SidechainColors::textPrimary(), false, 4.0f);
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(13.0f);
        g.drawText("Following", timelineTab, juce::Justification::centred);
    }

    // Trending tab
    auto trendingTab = getTrendingTabBounds();
    bool isTrendingActive = (currentFeedType == FeedType::Trending);

    // Use UIHelpers::drawButton for consistent tab styling
    if (isTrendingActive)
    {
        UIHelpers::drawButton(g, trendingTab.reduced(5), "Trending",
            SidechainColors::primary(), SidechainColors::textPrimary(), false, 4.0f);
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(13.0f);
        g.drawText("Trending", trendingTab, juce::Justification::centred);
    }

    // Global (Discover) tab
    auto globalTab = getGlobalTabBounds();
    bool isGlobalActive = (currentFeedType == FeedType::Discovery);

    // Use UIHelpers::drawButton for consistent tab styling
    if (isGlobalActive)
    {
        UIHelpers::drawButton(g, globalTab.reduced(5), "Discover",
            SidechainColors::primary(), SidechainColors::textPrimary(), false, 4.0f);
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(13.0f);
        g.drawText("Discover", globalTab, juce::Justification::centred);
    }

    // For You tab
    auto forYouTab = getForYouTabBounds();
    bool isForYouActive = (currentFeedType == FeedType::ForYou);

    // Use UIHelpers::drawButton for consistent tab styling
    if (isForYouActive)
    {
        UIHelpers::drawButton(g, forYouTab.reduced(5), "For You",
            SidechainColors::primary(), SidechainColors::textPrimary(), false, 4.0f);
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(13.0f);
        g.drawText("For You", forYouTab, juce::Justification::centred);
    }

    // Refresh button
    auto refreshBtn = getRefreshButtonBounds();
    auto currentState = feedStore ? feedStore->getState().getCurrentFeed() : Sidechain::Stores::SingleFeedState{};
    g.setColour(currentState.isLoading ? SidechainColors::textMuted() : SidechainColors::textSecondary());
    g.setFont(18.0f);
    g.drawText("Refresh", refreshBtn, juce::Justification::centred);

    // Bottom border - use UIHelpers::drawDivider for consistency
    UIHelpers::drawDivider(g, 0, tabsBounds.getBottom(), getWidth(),
        SidechainColors::borderSubtle(), 1.0f);
}

void PostsFeed::drawLoadingState(juce::Graphics& g)
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(300, 150);

    // Loading spinner placeholder (animated dots)
    g.setColour(SidechainColors::primary());
    g.setFont(32.0f);
    g.drawText("...", centerBounds.withHeight(50), juce::Justification::centred);

    g.setColour(SidechainColors::textPrimary());
    g.setFont(18.0f);
    g.drawText("Loading feed...", centerBounds.withY(centerBounds.getY() + 60).withHeight(30), juce::Justification::centred);

    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    g.drawText("Fetching latest posts", centerBounds.withY(centerBounds.getY() + 95).withHeight(25), juce::Justification::centred);
}

void PostsFeed::drawEmptyState(juce::Graphics& g)
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 300);

    // Different message for Timeline vs Global
    using namespace Sidechain::Stores;
    auto currentFeedType = feedStore ? feedStore->getCurrentFeedType() : FeedType::Timeline;
    juce::String title, subtitle1, subtitle2;

    if (currentFeedType == FeedType::Timeline)
    {
        title = "Your Feed is Empty";
        subtitle1 = "Follow other producers to see their loops here,";
        subtitle2 = "or create your first loop!";
    }
    else
    {
        title = "No Loops Yet";
        subtitle1 = "Be the first to share a loop!";
        subtitle2 = "Record from your DAW to get started.";
    }

    // Icon
    g.setColour(SidechainColors::textMuted());
    g.setFont(48.0f);
    g.drawText("~", centerBounds.withHeight(80), juce::Justification::centred);

    // Main message
    g.setColour(SidechainColors::textPrimary());
    g.setFont(24.0f);
    g.drawText(title, centerBounds.withY(centerBounds.getY() + 100).withHeight(40), juce::Justification::centred);

    // Subtitle
    g.setColour(SidechainColors::textSecondary());
    g.setFont(16.0f);
    g.drawText(subtitle1, centerBounds.withY(centerBounds.getY() + 150).withHeight(30), juce::Justification::centred);
    g.drawText(subtitle2, centerBounds.withY(centerBounds.getY() + 180).withHeight(30), juce::Justification::centred);

    // Action button
    // Use UIHelpers::drawButton for consistent button styling
    auto actionBtn = getRecordButtonBounds();
    UIHelpers::drawButton(g, actionBtn, "Start Recording",
        SidechainColors::primary(), SidechainColors::textPrimary(), false, 8.0f);
}

void PostsFeed::drawErrorState(juce::Graphics& g)
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 250);

    // Error icon
    g.setColour(SidechainColors::error());
    g.setFont(48.0f);
    g.drawText("!", centerBounds.withHeight(80), juce::Justification::centred);

    // Error message
    g.setColour(SidechainColors::textPrimary());
    g.setFont(20.0f);
    g.drawText("Couldn't Load Feed", centerBounds.withY(centerBounds.getY() + 90).withHeight(35), juce::Justification::centred);

    // Error details
    g.setColour(SidechainColors::textSecondary());
    g.setFont(14.0f);
    auto errorMsg = feedStore ? feedStore->getState().getCurrentFeed().error : juce::String("Network error");
    juce::String displayError = errorMsg.isEmpty() ? "Network error. Please check your connection." : errorMsg;
    g.drawFittedText(displayError, centerBounds.withY(centerBounds.getY() + 130).withHeight(40), juce::Justification::centred, 2);

    // Retry button
    // Use UIHelpers::drawButton for consistent button styling
    auto retryBtn = getRetryButtonBounds();
    UIHelpers::drawButton(g, retryBtn, "Try Again",
        SidechainColors::primary(), SidechainColors::textPrimary(), false, 8.0f);
}

void PostsFeed::drawFeedPosts(juce::Graphics& g)
{
    // Post cards are now child components, just update their visibility
    updatePostCardPositions();

    // Loading more indicator at bottom
    if (feedStore)
    {
        auto currentFeed = feedStore->getState().getCurrentFeed();
        if (currentFeed.isLoading)
        {
            auto contentBounds = getFeedContentBounds();
            int loadingY = contentBounds.getY() + totalContentHeight - static_cast<int>(scrollPosition);

            if (loadingY < contentBounds.getBottom())
            {
                g.setColour(SidechainColors::textMuted());
                g.setFont(14.0f);
                g.drawText("Loading more...",
                           contentBounds.getX(), loadingY, contentBounds.getWidth(), 40,
                           juce::Justification::centred);
            }
        }
    }
}

void PostsFeed::drawNewPostsToast(juce::Graphics& g)
{
    if (!showingNewPostsToast)
        return;

    // Draw toast at top of feed content area with fade animation (5.5.2)
    auto contentBounds = getFeedContentBounds();
    auto toastBounds = contentBounds.withHeight(40).withY(contentBounds.getY() + 10);

    float opacity = currentToastOpacity;
    if (opacity <= 0.0f)
        return;

    // Background with rounded corners (faded)
    g.setColour(SidechainColors::primary().withAlpha(0.95f * opacity));
    g.fillRoundedRectangle(toastBounds.toFloat(), 8.0f);

    // Border (faded)
    g.setColour(SidechainColors::textPrimary().withAlpha(0.3f * opacity));
    g.drawRoundedRectangle(toastBounds.toFloat(), 8.0f, 1.0f);

    // Text (faded)
    g.setColour(SidechainColors::textPrimary().withAlpha(opacity));
    g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)).boldened());

    juce::String toastText;
    if (pendingNewPostsCount == 1)
        toastText = "1 new post";
    else
        toastText = juce::String(pendingNewPostsCount) + " new posts";
    toastText += " - Click to refresh";

    g.drawText(toastText, toastBounds.reduced(15, 0), juce::Justification::centredLeft);

    // Clickable indicator (faded)
    g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    g.drawText("↻", toastBounds.removeFromRight(30), juce::Justification::centred);
}

//==============================================================================
void PostsFeed::rebuildPostCards()
{
    using namespace Sidechain::Stores;

    if (!feedStore)
    {
        Log::error("PostsFeed::rebuildPostCards: FeedStore is null!");
        return;
    }

    const auto currentFeedType = feedStore->getCurrentFeedType();
    const bool isAggregated = isAggregatedFeedType(currentFeedType);

    Log::debug("PostsFeed::rebuildPostCards: FeedType=" + feedTypeToString(currentFeedType) +
               ", isAggregated=" + juce::String(isAggregated ? "true" : "false"));

    // Debug: Log all feed types in state
    const auto& fullState = feedStore->getState();
    Log::debug("PostsFeed::rebuildPostCards: State has " + juce::String(fullState.feeds.size()) + " feeds, " +
               juce::String(fullState.aggregatedFeeds.size()) + " aggregated feeds");

    if (isAggregated)
    {
        // Build aggregated feed cards
        const auto& groups = feedStore->getState().getCurrentAggregatedFeed().groups;

        Log::info("PostsFeed::rebuildPostCards: Rebuilding aggregated cards - current: " +
                  juce::String(aggregatedCards.size()) + ", groups: " + juce::String(groups.size()));

        postCards.clear();
        aggregatedCards.clear();

        for (const auto& group : groups)
        {
            auto* card = aggregatedCards.add(new AggregatedFeedCard());
            card->setGroup(group);

            // Set up callbacks
            card->onUserClicked = [this](const juce::String& userId) {
                if (onNavigateToProfile)
                    onNavigateToProfile(userId);
            };

            card->onPlayClicked = [this](const juce::String& postId) {
                // Find the post and play it
                const auto& groups = feedStore->getState().getCurrentAggregatedFeed().groups;
                for (const auto& g : groups)
                {
                    for (const auto& post : g.activities)
                    {
                        if (post.id == postId && audioPlayer && post.audioUrl.isNotEmpty())
                        {
                            audioPlayer->loadAndPlay(post.id, post.audioUrl);
                            return;
                        }
                    }
                }
            };

            addAndMakeVisible(card);
            Log::debug("PostsFeed::rebuildPostCards: Created aggregated card for group: " + group.id);
        }

        Log::debug("PostsFeed::rebuildPostCards: Rebuilt " + juce::String(aggregatedCards.size()) + " aggregated cards");
    }
    else
    {
        // Build regular post cards
        const auto& state = feedStore->getState();
        const auto& currentFeed = state.getCurrentFeed();
        const auto& posts = currentFeed.posts;

        Log::info("PostsFeed::rebuildPostCards: Rebuilding post cards - current: " +
                  juce::String(postCards.size()) + ", posts: " + juce::String(posts.size()));
        Log::debug("PostsFeed::rebuildPostCards: CurrentFeed isLoading=" + juce::String(currentFeed.isLoading ? "true" : "false") +
                   ", hasMore=" + juce::String(currentFeed.hasMore ? "true" : "false") +
                   ", error=" + currentFeed.error);

        postCards.clear();
        aggregatedCards.clear();

        for (const auto& post : posts)
        {
            auto* card = postCards.add(new PostCard());
            card->setNetworkClient(networkClient);  // Pass NetworkClient for waveform downloads
            card->setFeedStore(feedStore);  // Task 2.6: Connect PostCard to FeedStore for reactive updates
            card->setPost(post);
            setupPostCardCallbacks(card);
            addAndMakeVisible(card);
            Log::debug("PostsFeed::rebuildPostCards: Created card for post: " + post.id);
        }

        Log::debug("PostsFeed::rebuildPostCards: Rebuilt " + juce::String(postCards.size()) + " post cards");
    }

    updatePostCardPositions();
}

void PostsFeed::updatePostCardPositions()
{
    auto contentBounds = getFeedContentBounds();
    int cardWidth = contentBounds.getWidth() - 40; // Padding
    int visibleCount = 0;

    if (!aggregatedCards.isEmpty())
    {
        // Layout aggregated cards (variable height)
        int currentY = contentBounds.getY() + POSTS_TOP_PADDING - static_cast<int>(scrollPosition);

        for (auto* card : aggregatedCards)
        {
            int cardHeight = card->getHeight();
            card->setBounds(contentBounds.getX() + 20, currentY, cardWidth, cardHeight);

            // Show/hide based on visibility
            bool visible = (currentY + cardHeight > contentBounds.getY()) &&
                          (currentY < contentBounds.getBottom());
            card->setVisible(visible);
            if (visible)
                visibleCount++;

            currentY += cardHeight + POST_CARD_SPACING;
        }

        Log::debug("PostsFeed::updatePostCardPositions: Updated aggregated cards - total: " +
                   juce::String(aggregatedCards.size()) + ", visible: " + juce::String(visibleCount));
    }
    else
    {
        // Layout regular post cards (fixed height)
        for (int i = 0; i < postCards.size(); ++i)
        {
            auto* card = postCards[i];
            int cardY = contentBounds.getY() + POSTS_TOP_PADDING - static_cast<int>(scrollPosition) + i * (POST_CARD_HEIGHT + POST_CARD_SPACING);

            card->setBounds(contentBounds.getX() + 20, cardY, cardWidth, POST_CARD_HEIGHT);

            // Show/hide based on visibility
            bool visible = (cardY + POST_CARD_HEIGHT > contentBounds.getY()) &&
                          (cardY < contentBounds.getBottom());
            card->setVisible(visible);
            if (visible)
                visibleCount++;
        }

        Log::debug("PostsFeed::updatePostCardPositions: Updated positions - total: " +
                   juce::String(postCards.size()) + ", visible: " + juce::String(visibleCount) +
                   ", scrollPosition: " + juce::String(scrollPosition, 1));
    }
}

void PostsFeed::setupPostCardCallbacks(PostCard* card)
{
    card->onPlayClicked = [this](const FeedPost& post) {
        Log::info("PostsFeed: Play clicked for post: " + post.id + ", audioUrl: " + post.audioUrl);
        Log::debug("PostsFeed: audioPlayer=" + juce::String::toHexString((juce::pointer_sized_int)audioPlayer) +
                   ", audioUrl.isNotEmpty=" + juce::String(post.audioUrl.isNotEmpty() ? "true" : "false"));
        if (audioPlayer && post.audioUrl.isNotEmpty())
        {
            Log::info("PostsFeed: Calling audioPlayer->loadAndPlay()");
            audioPlayer->loadAndPlay(post.id, post.audioUrl);

            // Pre-buffer next post for seamless playback
            if (feedStore)
            {
                const auto& posts = feedStore->getState().getCurrentFeed().posts;
                int currentIndex = -1;
                for (int i = 0; i < posts.size(); ++i)
                {
                    if (posts[i].id == post.id)
                    {
                        currentIndex = i;
                        break;
                    }
                }

                if (currentIndex >= 0 && currentIndex < posts.size() - 1)
                {
                    const FeedPost& nextPost = posts[currentIndex + 1];
                    if (nextPost.audioUrl.isNotEmpty())
                    {
                        Log::debug("PostsFeed: Pre-buffering next post: " + nextPost.id);
                        audioPlayer->preloadAudio(nextPost.id, nextPost.audioUrl);
                    }
                }
            }
        }
    };

    card->onPauseClicked = [this](const FeedPost& post) {
        Log::debug("Pause clicked for post: " + post.id);
        if (audioPlayer && audioPlayer->isPostPlaying(post.id))
        {
            audioPlayer->pause();
        }
    };

    card->onCardTapped = [this](const FeedPost& post) {
        Log::debug("Card tapped for post: " + post.id);
        // Open comments panel to show post details
        showCommentsForPost(post);
    };

    // Like/unlike handled by FeedStore.toggleLike() (Task 2.1 - reactive refactoring)
    // The callback is no longer needed as PostCard now uses FeedStore directly

    // Emoji reactions handled by FeedStore.addReaction() (Task 2.1 - reactive refactoring)
    // The callback is no longer needed as PostCard now uses FeedStore directly

    card->onUserClicked = [this](const FeedPost& post) {
        Log::debug("User clicked: " + post.username + " (id: " + post.userId + ")");
        if (onNavigateToProfile && post.userId.isNotEmpty())
            onNavigateToProfile(post.userId);
    };

    card->onCommentClicked = [this](const FeedPost& post) {
        Log::debug("Comments clicked for post: " + post.id);
        showCommentsForPost(post);
    };

    card->onShareClicked = [this](const FeedPost& post) {
        Log::debug("Share clicked for post: " + post.id);

        juce::PopupMenu shareMenu;
        shareMenu.addItem(1, "Copy Link");
        shareMenu.addItem(2, "Send to...");

        shareMenu.showMenuAsync(juce::PopupMenu::Options(),
            [this, post](int result) {
                if (result == 1)
                {
                    // Copy shareable link to clipboard
                    juce::String shareUrl = "https://sidechain.live/post/" + post.id;
                    juce::SystemClipboard::copyTextToClipboard(shareUrl);
                    Log::info("PostsFeed: Copied link for post " + post.id);
                }
                else if (result == 2)
                {
                    // Send to message
                    if (onSendPostToMessage)
                        onSendPostToMessage(post);
                }
            });
    };

    card->onMoreClicked = [this](const FeedPost& post) {
        Log::info("PostsFeedComponent: More menu clicked for post: " + post.id);

        juce::PopupMenu menu;

        // More like this option (always available)
        menu.addItem(1, "More like this");

        // Copy link option
        menu.addItem(2, "Copy Link");

        if (post.isOwnPost)
        {
            // Delete option for own posts
            menu.addSeparator();
            menu.addItem(3, "Delete Post");
        }
        else
        {
            // Report option for other users' posts
            menu.addSeparator();
            menu.addItem(4, "Report Post");
        }

        menu.showMenuAsync(juce::PopupMenu::Options(),
            [this, post](int result) {
                if (result == 1)
                {
                    // More like this - switch to similar posts view
                    Log::info("PostsFeedComponent: More like this clicked for post: " + post.id);
                    if (networkClient != nullptr)
                    {
                        // Fetch similar posts and show in feed
                        networkClient->getSimilarPosts(post.id, 20, [this, post](Outcome<juce::var> result) {
                            if (result.isOk())
                            {
                                // Parse similar posts and show in feed
                                auto data = result.getValue();
                                if (data.isObject())
                                {
                                    auto activities = data.getProperty("activities", juce::var());
                                    if (activities.isArray())
                                    {
                                        // Convert to FeedResponse format
                                        FeedResponse response;
                                        for (int i = 0; i < activities.size(); ++i)
                                        {
                                            auto feedPost = FeedPost::fromJson(activities[i]);
                                            if (feedPost.isValid())
                                                response.posts.add(feedPost);
                                        }
                                        response.hasMore = false; // Similar posts don't paginate
                                        // onFeedLoaded(response); // Task 2.6: Refactored to use FeedStore subscription
                                        Log::info("PostsFeedComponent: Loaded " + juce::String(response.posts.size()) + " similar posts");
                                    }
                                }
                            }
                            else
                            {
                                Log::error("PostsFeedComponent: Failed to get similar posts: " + result.getError());
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Error",
                                    "Failed to load similar posts. Please try again.");
                            }
                        });
                    }
                }
                else if (result == 2)
                {
                    // Copy link
                    juce::String shareUrl = "https://sidechain.live/post/" + post.id;
                    juce::SystemClipboard::copyTextToClipboard(shareUrl);
                    Log::info("PostsFeedComponent: Copied post link to clipboard");
                }
                else if (result == 2)
                {
                    // Copy link
                    juce::String shareUrl = "https://sidechain.live/post/" + post.id;
                    juce::SystemClipboard::copyTextToClipboard(shareUrl);
                    Log::info("PostsFeedComponent: Copied post link to clipboard");
                }
                else if (result == 3 && post.isOwnPost)
                {
                    // Delete post
                    auto options = juce::MessageBoxOptions()
                        .withTitle("Delete Post")
                        .withMessage("Are you sure you want to delete this post? This action cannot be undone.")
                        .withButton("Delete")
                        .withButton("Cancel");

                    juce::AlertWindow::showAsync(options, [this, post](int deleteResult) {
                        if (deleteResult == 1 && networkClient != nullptr)
                        {
                            networkClient->deletePost(post.id, [this, postId = post.id](Outcome<juce::var> result) {
                                if (result.isOk())
                                {
                                    Log::info("PostsFeedComponent: Post deleted successfully - " + postId);
                                    // Task 2.6: Post deletion now handled by FeedStore subscription
                                    // The feed will automatically update when the delete succeeds
                                    juce::MessageManager::callAsync([]() {
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::InfoIcon,
                                            "Post Deleted",
                                            "Your post has been deleted successfully.");
                                    });
                                }
                                else
                                {
                                    Log::error("PostsFeedComponent: Failed to delete post - " + result.getError());
                                    juce::MessageManager::callAsync([result]() {
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Error",
                                            "Failed to delete post: " + result.getError());
                                    });
                                }
                            });
                        }
                    });
                }
                else if (result == 3 && !post.isOwnPost)
                {
                    // Report post
                    auto options = juce::MessageBoxOptions()
                        .withTitle("Report Post")
                        .withMessage("Why are you reporting this post?")
                        .withButton("Spam")
                        .withButton("Harassment")
                        .withButton("Inappropriate")
                        .withButton("Other")
                        .withButton("Cancel");

                    juce::AlertWindow::showAsync(options, [this, post](int reportResult) {
                        if (reportResult >= 1 && reportResult <= 4 && networkClient != nullptr)
                        {
                            juce::String reasons[] = {"spam", "harassment", "inappropriate", "other"};
                            juce::String reason = reasons[reportResult - 1];
                            juce::String description = "Reported post: " + post.id;

                            networkClient->reportPost(post.id, reason, description, [postId = post.id, reason](Outcome<juce::var> result) {
                                if (result.isOk())
                                {
                                    Log::info("PostsFeedComponent: Post reported successfully - " + postId + ", reason: " + reason);
                                    juce::MessageManager::callAsync([]() {
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::InfoIcon,
                                            "Report Submitted",
                                            "Thank you for reporting this post. We will review it shortly.");
                                    });
                                }
                                else
                                {
                                    Log::error("PostsFeedComponent: Failed to report post - " + result.getError());
                                    juce::MessageManager::callAsync([result]() {
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Error",
                                            "Failed to report post: " + result.getError());
                                    });
                                }
                            });
                        }
                    });
                }
            });
    };

    card->onAddToDAWClicked = [](const FeedPost& post) {
        Log::debug("Add to DAW clicked for post: " + post.id);

        if (post.audioUrl.isEmpty())
        {
            Log::warn("No audio URL available for post: " + post.id);
            return;
        }

        // Show file chooser to let user select where to save
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save audio to DAW project folder...",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav,*.mp3,*.flac");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode,
                           [post, chooser](const juce::FileChooser& fc) {
            auto targetFile = fc.getResult();

            if (targetFile == juce::File())
                return; // User cancelled

            // Download the audio file in background
            Async::runVoid([post, targetFile]() {
                juce::URL audioUrl(post.audioUrl);
                juce::MemoryBlock audioData;

                if (audioUrl.readEntireBinaryStream(audioData))
                {
                    // Write to file
                    juce::FileOutputStream output(targetFile);
                    if (output.openedOk())
                    {
                        output.write(audioData.getData(), audioData.getSize());
                        output.flush();

                        juce::MessageManager::callAsync([targetFile]() {
                            Log::info("Audio saved to: " + targetFile.getFullPathName());
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::InfoIcon,
                                "Success",
                                "Audio saved to:\n" + targetFile.getFullPathName());
                        });
                    }
                    else
                    {
                        juce::MessageManager::callAsync([targetFile]() {
                            Log::error("Failed to write audio file: " + targetFile.getFullPathName());
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::WarningIcon,
                                "Error",
                                "Failed to save audio file:\n" + targetFile.getFullPathName());
                        });
                    }
                }
                else
                {
                    juce::MessageManager::callAsync([post]() {
                        Log::error("Failed to download audio from: " + post.audioUrl);
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Error",
                            "Failed to download audio file. Please check your connection and try again.");
                    });
                }
            });
        });
    };

    card->onDropToTrackClicked = [this, card](const FeedPost& post) {
        Log::debug("Drop to Track clicked for post: " + post.id);

        if (networkClient == nullptr)
        {
            Log::warn("PostsFeedComponent: Cannot download - networkClient is null");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Unable to download. Please try again later.");
            return;
        }

        // Get download info from backend
        networkClient->getPostDownloadInfo(post.id, [this, post, card](Outcome<NetworkClient::DownloadInfo> downloadInfoOutcome) {
            if (!downloadInfoOutcome.isOk())
            {
                Log::error("Failed to get download info: " + downloadInfoOutcome.getError());
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Error",
                    "Failed to get download information. Please try again.");
                return;
            }

            auto info = downloadInfoOutcome.getValue();
            
            // Determine target location based on DAW
            // For now, use a generic location - we'll enhance this with DAW-specific paths later
            juce::File targetDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("Sidechain")
                .getChildFile("Downloads");
            
            if (!targetDir.exists())
            {
                targetDir.createDirectory();
            }

            juce::File targetFile = targetDir.getChildFile(info.filename);

            // Download the file with progress
            networkClient->downloadFile(
                info.downloadUrl,
                targetFile,
                [card](float progress) {
                    // Update card with download progress
                    if (card != nullptr)
                        card->setDownloadProgress(progress);
                },
                [card, targetFile, info](Outcome<juce::var> result) {
                    if (result.isOk())
                    {
                        // Reset download progress
                        if (card != nullptr)
                            card->setDownloadProgress(1.0f);
                        
                        Log::info("Loop added to project folder: " + targetFile.getFullPathName());
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::InfoIcon,
                            "Success",
                            "Loop added to project folder!\n\n" + targetFile.getFullPathName());
                        
                        // Clear progress after a moment
                        juce::MessageManager::callAsync([card]() {
                            if (card != nullptr)
                                card->setDownloadProgress(0.0f);
                        });
                    }
                    else
                    {
                        // Reset download progress on error
                        if (card != nullptr)
                            card->setDownloadProgress(0.0f);
                        
                        Log::error("Failed to download file: " + result.getError());
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Error",
                            "Failed to download file. Please try again.");
                    }
                }
            );
        });
    };

    card->onDownloadMIDIClicked = [this](const FeedPost& post) {
        Log::debug("Download MIDI clicked for post: " + post.id + ", midiId: " + post.midiId);

        if (!post.hasMidi || post.midiId.isEmpty())
        {
            Log::warn("PostsFeedComponent: Cannot download MIDI - no MIDI data available");
            return;
        }

        if (networkClient == nullptr)
        {
            Log::warn("PostsFeedComponent: Cannot download MIDI - networkClient is null");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Unable to download MIDI. Please try again later.");
            return;
        }

        // Determine target location for MIDI files (R.3.3.7.2)
        // Try DAW project folder first, fallback to default location
        juce::File targetDir = DAWProjectFolder::getMIDIFileLocation();
        
        // Ensure target directory exists
        if (!targetDir.exists())
        {
            auto result = targetDir.createDirectory();
            if (result.failed())
            {
                Log::warn("Failed to create MIDI directory: " + result.getErrorMessage());
                // Fallback to default location
                targetDir = DAWProjectFolder::getDefaultMIDIFolder();
            }
        }

        // Create filename from post username and ID
        juce::String safeName = post.username.isNotEmpty() ? post.username : "unknown";
        safeName = safeName.replaceCharacters(" /\\:*?\"<>|", "__________");
        juce::String filename = safeName + "_" + post.midiId.substring(0, 8) + ".mid";
        juce::File targetFile = targetDir.getChildFile(filename);

        // Detect DAW for notification message
        auto dawInfo = DAWProjectFolder::detectDAWProjectFolder();
        juce::String notificationMessage = "MIDI saved to:\n" + targetFile.getFullPathName();
        if (dawInfo.isAccessible && dawInfo.dawName != "Unknown")
        {
            notificationMessage += "\n\nFile is in your " + dawInfo.dawName + " project folder and ready to import.";
        }

        // Download the MIDI file
        networkClient->downloadMIDI(
            post.midiId,
            targetFile,
            [targetFile, notificationMessage](Outcome<juce::var> result) {
                if (result.isOk())
                {
                    Log::info("MIDI downloaded: " + targetFile.getFullPathName());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon,
                        "MIDI Downloaded",
                        notificationMessage);
                }
                else
                {
                    Log::error("Failed to download MIDI: " + result.getError());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Error",
                        "Failed to download MIDI. Please try again.");
                }
            }
        );
    };

    card->onAddToPlaylistClicked = [this](const FeedPost& post) {
        Log::debug("Add to Playlist clicked for post: " + post.id);

        if (networkClient == nullptr)
        {
            Log::warn("PostsFeed: Cannot add to playlist - networkClient is null");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Unable to add to playlist. Please try again later.");
            return;
        }

        // Fetch user's playlists
        networkClient->getPlaylists("all", [this, post](Outcome<juce::var> result) {
            juce::MessageManager::callAsync([this, post, result]() {
                if (!result.isOk())
                {
                    Log::warn("PostsFeed: Failed to load playlists: " + result.getError());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Error",
                        "Failed to load playlists. Please try again later.");
                    return;
                }

                juce::Array<Playlist> playlists;
                auto response = result.getValue();
                if (response.hasProperty("playlists"))
                {
                    auto playlistsArray = response["playlists"];
                    if (playlistsArray.isArray())
                    {
                        for (int i = 0; i < playlistsArray.size(); ++i)
                        {
                            playlists.add(Playlist::fromJSON(playlistsArray[i]));
                        }
                    }
                }

                // Show playlist picker menu
                juce::PopupMenu menu;
                menu.addItem(1, "Create New Playlist...", true, false);

                if (playlists.isEmpty())
                {
                    menu.addSeparator();
                    menu.addItem(2, "No playlists available", false);
                }
                else
                {
                    menu.addSeparator();
                    for (int i = 0; i < playlists.size(); ++i)
                    {
                        juce::String name = playlists[i].name;
                        if (playlists[i].isCollaborative)
                            name += " (Collaborative)";
                        menu.addItem(i + 3, name, true, false);
                    }
                }

                menu.showMenuAsync(juce::PopupMenu::Options(), [this, post, playlists](int result) {
                    if (result == 1)
                    {
                        // Create new playlist - show message directing user to PlaylistsComponent
                        // (Full playlist creation UI is in PlaylistsComponent)
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::InfoIcon,
                            "Create Playlist",
                            "Please go to Playlists to create a new playlist, then add this track.");
                    }
                    else if (result >= 3 && result - 3 < playlists.size())
                    {
                        // Add to selected playlist
                        int index = result - 3;
                        
                        // Extract post ID from foreignId (format: "loop:uuid")
                        juce::String postId = post.foreignId;
                        if (postId.startsWith("loop:"))
                            postId = postId.substring(5);
                        
                        if (networkClient)
                        {
                            networkClient->addPlaylistEntry(playlists[index].id, postId, -1, [](Outcome<juce::var> addResult) {
                                juce::MessageManager::callAsync([addResult]() {
                                    if (addResult.isOk())
                                    {
                                        Log::info("PostsFeed: Post added to playlist successfully");
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::InfoIcon,
                                            "Added to Playlist",
                                            "Track added to playlist successfully!");
                                    }
                                    else
                                    {
                                        Log::warn("PostsFeed: Failed to add post to playlist: " + addResult.getError());
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Error",
                                            "Failed to add track to playlist. Please try again.");
                                    }
                                });
                            });
                        }
                    }
                });
            });
        }        );
    };

    card->onAddToPlaylistClicked = [this](const FeedPost& post) {
        Log::debug("Add to Playlist clicked for post: " + post.id);

        if (networkClient == nullptr)
        {
            Log::warn("PostsFeed: Cannot add to playlist - networkClient is null");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Unable to add to playlist. Please try again later.");
            return;
        }

        // Fetch user's playlists
        networkClient->getPlaylists("all", [this, post](Outcome<juce::var> result) {
            juce::MessageManager::callAsync([this, post, result]() {
                if (!result.isOk())
                {
                    Log::warn("PostsFeed: Failed to load playlists: " + result.getError());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Error",
                        "Failed to load playlists. Please try again later.");
                    return;
                }

                juce::Array<Playlist> playlists;
                auto response = result.getValue();
                if (response.hasProperty("playlists"))
                {
                    auto playlistsArray = response["playlists"];
                    if (playlistsArray.isArray())
                    {
                        for (int i = 0; i < playlistsArray.size(); ++i)
                        {
                            playlists.add(Playlist::fromJSON(playlistsArray[i]));
                        }
                    }
                }

                // Show playlist picker menu
                juce::PopupMenu menu;
                menu.addItem(1, "Create New Playlist...", true, false);

                if (playlists.isEmpty())
                {
                    menu.addSeparator();
                    menu.addItem(2, "No playlists available", false);
                }
                else
                {
                    menu.addSeparator();
                    for (int i = 0; i < playlists.size(); ++i)
                    {
                        juce::String name = playlists[i].name;
                        if (playlists[i].isCollaborative)
                            name += " (Collaborative)";
                        menu.addItem(i + 3, name, true, false);
                    }
                }

                menu.showMenuAsync(juce::PopupMenu::Options(), [this, post, playlists](int result) {
                    if (result == 1)
                    {
                        // Create new playlist - navigate to playlists view (user can create there)
                        // For now, just show a message
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::InfoIcon,
                            "Create Playlist",
                            "Please go to Playlists to create a new playlist, then add this track.");
                    }
                    else if (result >= 3 && result - 3 < playlists.size())
                    {
                        // Add to selected playlist
                        int index = result - 3;
                        
                        // Extract post ID from foreignId (format: "loop:uuid")
                        juce::String postId = post.foreignId;
                        if (postId.startsWith("loop:"))
                            postId = postId.substring(5);

                        if (networkClient)
                        {
                            networkClient->addPlaylistEntry(playlists[index].id, postId, -1, [](Outcome<juce::var> addResult) {
                                juce::MessageManager::callAsync([addResult]() {
                                    if (addResult.isOk())
                                    {
                                        Log::info("PostsFeed: Post added to playlist successfully");
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::InfoIcon,
                                            "Added to Playlist",
                                            "Track added to playlist successfully!");
                                    }
                                    else
                                    {
                                        Log::warn("PostsFeed: Failed to add post to playlist: " + addResult.getError());
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::WarningIcon,
                                            "Error",
                                            "Failed to add track to playlist. Please try again.");
                                    }
                                });
                            });
                        }
                    }
                });
            });
        });
    };

    card->onDownloadProjectClicked = [this](const FeedPost& post) {
        Log::debug("Download Project clicked for post: " + post.id + ", projectFileId: " + post.projectFileId);

        if (!post.hasProjectFile || post.projectFileId.isEmpty())
        {
            Log::warn("PostsFeedComponent: Cannot download project file - no project file available");
            return;
        }

        if (networkClient == nullptr)
        {
            Log::warn("PostsFeedComponent: Cannot download project file - networkClient is null");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Unable to download project file. Please try again later.");
            return;
        }

        // Determine target location for project files
        juce::File targetDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Sidechain")
            .getChildFile("Projects");

        if (!targetDir.exists())
        {
            targetDir.createDirectory();
        }

        // Create filename from post username, DAW type and ID
        juce::String safeName = post.username.isNotEmpty() ? post.username : "unknown";
        safeName = safeName.replaceCharacters(" /\\:*?\"<>|", "__________");

        // Determine file extension based on DAW type
        juce::String extension = ".zip"; // Default to zip
        if (post.projectFileDaw == "ableton")
            extension = ".als";
        else if (post.projectFileDaw == "fl_studio")
            extension = ".flp";
        else if (post.projectFileDaw == "logic")
            extension = ".logicx";
        else if (post.projectFileDaw == "reaper")
            extension = ".rpp";
        else if (post.projectFileDaw == "cubase")
            extension = ".cpr";
        else if (post.projectFileDaw == "studio_one")
            extension = ".song";
        else if (post.projectFileDaw == "bitwig")
            extension = ".bwproject";
        else if (post.projectFileDaw == "pro_tools")
            extension = ".ptx";

        juce::String filename = safeName + "_" + post.projectFileId.substring(0, 8) + extension;
        juce::File targetFile = targetDir.getChildFile(filename);

        // Download the project file
        networkClient->downloadProjectFile(
            post.projectFileId,
            targetFile,
            nullptr, // No progress callback for now
            [targetFile](Outcome<juce::var> result) {
                if (result.isOk())
                {
                    Log::info("Project file downloaded: " + targetFile.getFullPathName());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon,
                        "Project Downloaded",
                        "Project saved to:\n" + targetFile.getFullPathName());
                }
                else
                {
                    Log::error("Failed to download project file: " + result.getError());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Error",
                        "Failed to download project file. Please try again.");
                }
            }
        );
    };

    // R.3.2 Remix Chains - Remix button clicked
    card->onRemixClicked = [this](const FeedPost& post, const juce::String& remixType) {
        Log::info("PostsFeed: Remix clicked for post: " + post.id + " type: " + remixType);

        if (networkClient == nullptr)
        {
            Log::warn("PostsFeed: Cannot start remix - networkClient is null");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Unable to start remix. Please try again later.");
            return;
        }

        // If post has both audio and MIDI, show a menu to choose what to remix
        if (post.hasMidi && post.audioUrl.isNotEmpty() && remixType == "both")
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Remix Audio Only");
            menu.addItem(2, "Remix MIDI Only");
            menu.addItem(3, "Remix Both Audio & MIDI");

            menu.showMenuAsync(juce::PopupMenu::Options(), [this, post](int result) {
                if (result == 0) return; // Cancelled

                juce::String selectedType = (result == 1) ? "audio" : (result == 2) ? "midi" : "both";
                startRemixFlow(post, selectedType);
            });
        }
        else
        {
            // Only one option available, start remix directly
            startRemixFlow(post, remixType);
        }
    };

    // R.3.2 Remix Chains - Remix chain badge clicked (view lineage)
    card->onRemixChainClicked = [this](const FeedPost& post) {
        Log::info("PostsFeed: Remix chain clicked for post: " + post.id);

        if (networkClient == nullptr)
        {
            Log::warn("PostsFeed: Cannot view remix chain - networkClient is null");
            return;
        }

        // Fetch and display the remix chain
        networkClient->getRemixChain(post.id, [postId = post.id](Outcome<juce::var> result) {
            juce::MessageManager::callAsync([result, postId]() {
                if (!result.isOk())
                {
                    Log::warn("PostsFeed: Failed to fetch remix chain: " + result.getError());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Error",
                        "Failed to load remix chain. Please try again.");
                    return;
                }

                auto data = result.getValue();
                auto chain = data["chain"];
                int totalDepth = Json::getInt(data, "total_depth", 0);

                if (!chain.isArray() || chain.size() == 0)
                {
                    Log::debug("PostsFeed: Remix chain is empty for post: " + postId);
                    return;
                }

                // Build a visual representation of the remix chain
                juce::String chainText = "Remix Chain (depth " + juce::String(totalDepth) + "):\n\n";

                for (int i = 0; i < chain.size(); ++i)
                {
                    auto item = chain[i];
                    juce::String type = Json::getString(item, "type");
                    juce::String username = Json::getString(item, "username");
                    bool isCurrent = Json::getBool(item, "is_current");
                    int depth = Json::getInt(item, "depth", 0);

                    // Indent based on depth
                    juce::String indent = "";
                    for (int d = 0; d < depth; ++d)
                        indent += "  ";

                    juce::String arrow = (i > 0) ? "└→ " : "";
                    juce::String marker = isCurrent ? " [YOU ARE HERE]" : "";
                    juce::String typeLabel = (type == "story") ? "(story)" : "";

                    chainText += indent + arrow + "@" + username + " " + typeLabel + marker + "\n";
                }

                // Show the chain in a dialog
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Remix Chain",
                    chainText);
            });
        });
    };

    // Follow/Unfollow handled by FeedStore.toggleFollow() (Task 2.2 - reactive refactoring)
    // The callback is no longer needed as PostCard now uses FeedStore directly

    // Save/Bookmark handled by FeedStore.toggleSave() (Task 2.1 - reactive refactoring)
    // The callback is no longer needed as PostCard now uses FeedStore directly

    // Repost handled by FeedStore.toggleRepost() (Task 2.1 - reactive refactoring)
    // The callback is no longer needed as PostCard now uses FeedStore directly
    // Note: Confirmation dialogs moved to a future enhancement in PostCard or a dialog service

    card->onWaveformClicked = [this](const FeedPost& post, float position) {
        Log::debug("Waveform seek for post: " + post.id + " to " + juce::String(position, 2));
        if (audioPlayer)
        {
            // If this post isn't playing, start it at the clicked position
            if (!audioPlayer->isPostPlaying(post.id))
            {
                audioPlayer->loadAndPlay(post.id, post.audioUrl);
                // Seek after a short delay to let it load
                juce::Timer::callAfterDelay(100, [this, position]() {
                    if (audioPlayer)
                        audioPlayer->seekToNormalizedPosition(position);
                });
            }
            else
            {
                audioPlayer->seekToNormalizedPosition(position);
            }
        }
    };

    // Feature #15 - Sound/Sample Pages
    card->onSoundClicked = [this](const juce::String& soundId) {
        Log::debug("Sound clicked: " + soundId);
        if (onSoundClicked)
            onSoundClicked(soundId);
    };
}

//==============================================================================
void PostsFeed::resized()
{
    Log::debug("PostsFeed::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    auto bounds = getLocalBounds();
    auto contentBounds = getFeedContentBounds();

    // Position scroll bar on right
    scrollBar.setBounds(bounds.getRight() - 12, contentBounds.getY(), 12, contentBounds.getHeight());
    updateScrollBounds();
    updatePostCardPositions();

    // Position error state component in content area
    if (errorStateComponent != nullptr)
    {
        errorStateComponent->setBounds(contentBounds);
    }

    // Position feed skeleton in content area
    if (feedSkeleton != nullptr)
    {
        feedSkeleton->setBounds(contentBounds);
    }

    // Position comments panel if visible (animation will handle position updates)
    if (commentsPanel != nullptr && commentsPanelVisible)
    {
        // Animation callback will update position, but ensure initial position is set
        if (!commentsPanelAnimation || currentCommentsPanelSlide >= 1.0f)
        {
            int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
            commentsPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
        }
        Log::debug("PostsFeed::resized: Comments panel repositioned");
    }
}

void PostsFeed::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &scrollBar)
    {
        scrollPosition = newRangeStart;
        checkLoadMore();
        repaint();
    }
}

void PostsFeed::mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel)
{
    if (feedState != FeedState::Loaded)
    {
        Log::debug("PostsFeed::mouseWheelMove: Ignoring wheel - feed not loaded");
        return;
    }

    double scrollAmount = wheel.deltaY * 50.0;
    double oldPosition = scrollPosition;
    scrollPosition = juce::jlimit(0.0, static_cast<double>(juce::jmax(0, totalContentHeight - getFeedContentBounds().getHeight())), scrollPosition - scrollAmount);
    Log::debug("PostsFeed::mouseWheelMove: Wheel scroll - delta: " + juce::String(wheel.deltaY, 2) + ", position: " + juce::String(oldPosition, 1) + " -> " + juce::String(scrollPosition, 1));
    scrollBar.setCurrentRangeStart(scrollPosition);
    checkLoadMore();
    repaint();
}

void PostsFeed::updateScrollBounds()
{
    auto contentBounds = getFeedContentBounds();
    // Task 2.6: Use FeedStore instead of local posts array
    const auto& posts = feedStore ? feedStore->getState().getCurrentFeed().posts : juce::Array<FeedPost>{};
    totalContentHeight = POSTS_TOP_PADDING + static_cast<int>(posts.size()) * (POST_CARD_HEIGHT + POST_CARD_SPACING);

    double visibleHeight = contentBounds.getHeight();
    scrollBar.setRangeLimits(0.0, juce::jmax(static_cast<double>(totalContentHeight), visibleHeight));
    scrollBar.setCurrentRange(scrollPosition, visibleHeight);
    Log::debug("PostsFeed::updateScrollBounds: Scroll bounds updated - totalHeight: " + juce::String(totalContentHeight) + ", visibleHeight: " + juce::String(visibleHeight, 1));
}

void PostsFeed::checkLoadMore()
{
    // Task 2.6: Use FeedStore instead of feedDataManager
    if (!feedStore)
    {
        Log::debug("PostsFeed::checkLoadMore: FeedStore is null, skipping");
        return;
    }

    const auto& currentFeed = feedStore->getState().getCurrentFeed();
    if (feedState != FeedState::Loaded || !currentFeed.hasMore || currentFeed.isLoading)
    {
        if (feedState != FeedState::Loaded)
            Log::debug("PostsFeed::checkLoadMore: Feed not loaded, skipping");
        else if (!currentFeed.hasMore)
            Log::debug("PostsFeed::checkLoadMore: No more posts available");
        else if (currentFeed.isLoading)
            Log::debug("PostsFeed::checkLoadMore: Already fetching, skipping");
        return;
    }

    auto contentBounds = getFeedContentBounds();
    double scrollEnd = scrollPosition + contentBounds.getHeight();
    double threshold = totalContentHeight - 200; // Load more when 200px from bottom

    Log::debug("PostsFeed::checkLoadMore: Checking threshold - scrollEnd: " + juce::String(scrollEnd, 1) + ", threshold: " + juce::String(threshold, 1) + ", totalHeight: " + juce::String(totalContentHeight));

    if (scrollEnd >= threshold)
    {
        Log::info("PostsFeed::checkLoadMore: Threshold reached, loading more posts");
        // Task 2.6: Use FeedStore.loadMore() - it will notify via subscription
        feedStore->loadMore();
    }
}

//==============================================================================
void PostsFeed::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("PostsFeed::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

    // Check if clicked on toast to refresh (5.5.2)
    if (showingNewPostsToast && pendingNewPostsCount > 0)
    {
        auto contentBounds = getFeedContentBounds();
        auto toastBounds = contentBounds.withHeight(40).withY(contentBounds.getY() + 10);
        if (toastBounds.contains(pos))
        {
            Log::info("PostsFeed::mouseUp: New posts toast clicked, refreshing feed");
            refreshFeed();
            pendingNewPostsCount = 0;
            showingNewPostsToast = false;
            stopTimer();
            repaint();
            return;
        }
    }

    // Check feed tabs - Following, Trending, Discover
    if (getTimelineTabBounds().contains(pos))
    {
        Log::info("PostsFeed::mouseUp: Timeline tab clicked");
        switchFeedType(Sidechain::Stores::FeedType::Timeline);
        return;
    }

    if (getTrendingTabBounds().contains(pos))
    {
        Log::info("PostsFeed::mouseUp: Trending tab clicked");
        switchFeedType(Sidechain::Stores::FeedType::Trending);
        return;
    }

    if (getGlobalTabBounds().contains(pos))
    {
        Log::info("PostsFeed::mouseUp: Discover tab clicked (Gorse discovery feed)");
        switchFeedType(Sidechain::Stores::FeedType::Discovery);
        return;
    }

    if (getForYouTabBounds().contains(pos))
    {
        Log::info("PostsFeed::mouseUp: For You tab clicked");
        switchFeedType(Sidechain::Stores::FeedType::ForYou);
        return;
    }

    // Check refresh button - Task 2.6: Use FeedStore instead of feedDataManager
    const auto& currentFeed = feedStore ? feedStore->getState().getCurrentFeed() : Sidechain::Stores::SingleFeedState{};
    if (getRefreshButtonBounds().contains(pos) && !currentFeed.isLoading)
    {
        Log::info("PostsFeed::mouseUp: Refresh button clicked");
        refreshFeed();
        return;
    }

    // Check retry button (error state)
    if (feedState == FeedState::Error && getRetryButtonBounds().contains(pos))
    {
        Log::info("PostsFeed::mouseUp: Retry button clicked");
        loadFeed();
        return;
    }

    // Check record button (empty state)
    if (feedState == FeedState::Empty && getRecordButtonBounds().contains(pos))
    {
        Log::info("PostsFeed::mouseUp: Record button clicked");
        if (onStartRecording)
            onStartRecording();
        else
            Log::warn("PostsFeed::mouseUp: Record button clicked but callback not set");
        return;
    }

    // Note: Discover/search and profile clicks now handled by central HeaderComponent
}

//==============================================================================
juce::Rectangle<int> PostsFeed::getTimelineTabBounds() const
{
    // Tabs now start at y=0 (header handled by central HeaderComponent)
    // Three tabs: Following, Trending, Discover - each 80px wide with 10px gaps
    return juce::Rectangle<int>(15, 10, 80, 30);
}

juce::Rectangle<int> PostsFeed::getTrendingTabBounds() const
{
    return juce::Rectangle<int>(105, 10, 80, 30);
}

juce::Rectangle<int> PostsFeed::getGlobalTabBounds() const
{
    return juce::Rectangle<int>(195, 10, 80, 30);
}

juce::Rectangle<int> PostsFeed::getForYouTabBounds() const
{
    return juce::Rectangle<int>(285, 10, 80, 30);
}

juce::Rectangle<int> PostsFeed::getRefreshButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 100, 10, 80, 30);
}

juce::Rectangle<int> PostsFeed::getRetryButtonBounds() const
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 250);
    return juce::Rectangle<int>(centerBounds.getCentreX() - 75, centerBounds.getY() + 190, 150, 45);
}

juce::Rectangle<int> PostsFeed::getRecordButtonBounds() const
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 300);
    return juce::Rectangle<int>(centerBounds.getCentreX() - 100, centerBounds.getY() + 230, 200, 50);
}

juce::Rectangle<int> PostsFeed::getFeedContentBounds() const
{
    // Content starts below feed tabs (header handled by central HeaderComponent)
    return getLocalBounds().withTrimmedTop(FEED_TABS_HEIGHT);
}

//==============================================================================
// Keyboard shortcuts

bool PostsFeed::keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/)
{
    if (audioPlayer == nullptr)
    {
        Log::debug("PostsFeed::keyPressed: AudioPlayer is null, ignoring key press");
        return false;
    }

    // Space bar - toggle play/pause
    if (key == juce::KeyPress::spaceKey)
    {
        Log::info("PostsFeed::keyPressed: Space bar - toggling play/pause");
        audioPlayer->togglePlayPause();
        return true;
    }

    // Right arrow - skip to next
    if (key == juce::KeyPress::rightKey)
    {
        Log::info("PostsFeed::keyPressed: Right arrow - skipping to next");
        audioPlayer->playNext();
        return true;
    }

    // Left arrow - skip to previous / restart
    if (key == juce::KeyPress::leftKey)
    {
        Log::info("PostsFeed::keyPressed: Left arrow - skipping to previous");
        audioPlayer->playPrevious();
        return true;
    }

        // Note: Card tap to expand details is implemented - opens comments panel (line 699-703)
        // Note: Post author online status is implemented - queries getstream.io Chat presence and shows green dot on avatar if online
        // Note: Pre-buffering next post is already implemented (line 668-687) - uses preloadAudio() method

    // Up arrow - volume up
    if (key == juce::KeyPress::upKey)
    {
        float oldVolume = audioPlayer->getVolume();
        float newVolume = juce::jmin(1.0f, audioPlayer->getVolume() + 0.1f);
        audioPlayer->setVolume(newVolume);
        Log::debug("PostsFeed::keyPressed: Up arrow - volume " + juce::String(oldVolume, 2) + " -> " + juce::String(newVolume, 2));
        return true;
    }

    // Down arrow - volume down
    if (key == juce::KeyPress::downKey)
    {
        float oldVolume = audioPlayer->getVolume();
        float newVolume = juce::jmax(0.0f, audioPlayer->getVolume() - 0.1f);
        audioPlayer->setVolume(newVolume);
        Log::debug("PostsFeed::keyPressed: Down arrow - volume " + juce::String(oldVolume, 2) + " -> " + juce::String(newVolume, 2));
        return true;
    }

    // M key - toggle mute
    if (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M')
    {
        bool wasMuted = audioPlayer->isMuted();
        audioPlayer->setMuted(!wasMuted);
        Log::info("PostsFeed::keyPressed: M key - mute toggled " + juce::String(wasMuted ? "off" : "on"));
        return true;
    }

    // Escape - close comments panel
    if (key == juce::KeyPress::escapeKey && commentsPanelVisible)
    {
        Log::info("PostsFeed::keyPressed: Escape key - closing comments panel");
        hideCommentsPanel();
        return true;
    }

    return false;
}

//==============================================================================
// Comments panel

void PostsFeed::showCommentsForPost(const FeedPost& post)
{
    if (commentsPanel == nullptr)
    {
        Log::warn("PostsFeed::showCommentsForPost: Comments panel is null");
        return;
    }

    Log::info("PostsFeed::showCommentsForPost: Showing comments for post: " + post.id);

    // Set up the panel
    commentsPanel->setNetworkClient(networkClient);
    commentsPanel->setCurrentUserId(currentUserId);
    commentsPanel->loadCommentsForPost(post.id);
    Log::debug("PostsFeed::showCommentsForPost: Comments panel configured and loading comments");

    // Position as right-side panel (takes 40% of width)
    int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
    commentsPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
    Log::debug("PostsFeed::showCommentsForPost: Comments panel positioned - width: " + juce::String(panelWidth));

    // Show with slide animation
    commentsPanel->setVisible(true);
    commentsPanelVisible = true;

    // Create and start slide-in animation
    currentCommentsPanelSlide = 0.0f;
    commentsPanelAnimation = TransitionAnimation<float>::create(0.0f, 1.0f, 250)
        ->withEasing(Easing::easeOutCubic)
        ->onProgress([this](float slide) {
            if (commentsPanel != nullptr)
            {
                int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
                int targetX = static_cast<int>(getWidth() - panelWidth * slide);
                commentsPanel->setBounds(targetX, 0, panelWidth, getHeight());
            }
            repaint();
        })
        ->start();

    // Bring to front
    commentsPanel->toFront(true);

    Log::debug("PostsFeed::showCommentsForPost: Comments panel shown with animation");
}

void PostsFeed::hideCommentsPanel()
{
    if (commentsPanel == nullptr)
        return;

    commentsPanel->setVisible(false);
    commentsPanelVisible = false;
    repaint();
}

//==============================================================================
// Playlist management for auto-play

void PostsFeed::updateAudioPlayerPlaylist()
{
    if (audioPlayer == nullptr)
    {
        Log::warn("PostsFeed::updateAudioPlayerPlaylist: AudioPlayer is null");
        return;
    }

    juce::StringArray postIds;
    juce::StringArray audioUrls;

    // Task 2.6: Use FeedStore instead of local posts array
    const auto& posts = feedStore ? feedStore->getState().getCurrentFeed().posts : juce::Array<FeedPost>{};
    for (const auto& post : posts)
    {
        if (post.audioUrl.isNotEmpty())
        {
            postIds.add(post.id);
            audioUrls.add(post.audioUrl);
        }
    }

    Log::info("PostsFeed::updateAudioPlayerPlaylist: Updating playlist - posts: " + juce::String(postIds.size()) + " with audio");
    audioPlayer->setPlaylist(postIds, audioUrls);
}

//==============================================================================
// Real-time Feed Updates (5.5)
//==============================================================================

void PostsFeed::handleNewPostNotification(const juce::var& postData)
{
    Log::info("PostsFeed::handleNewPostNotification: New post notification received");
    // Increment pending new posts count (5.5.2)
    pendingNewPostsCount++;
    lastNewPostTime = juce::Time::getCurrentTime();
    Log::debug("PostsFeed::handleNewPostNotification: Pending count: " + juce::String(pendingNewPostsCount));

    // Show toast notification if feed is visible and user is not at the top
    if (isVisible() && scrollPosition > 0.1)
    {
        Log::debug("PostsFeed::handleNewPostNotification: User scrolled, showing toast");
        showNewPostsToast(pendingNewPostsCount);
    }

    // If user is at the top of the feed, refresh immediately (5.5.1)
    if (isVisible() && scrollPosition < 0.1)
    {
        Log::info("PostsFeed::handleNewPostNotification: User at top, refreshing feed immediately");
        refreshFeed();
        pendingNewPostsCount = 0;
    }

    repaint();
}

void PostsFeed::handleLikeCountUpdate(const juce::String& postId, int likeCount)
{
    Log::debug("PostsFeed::handleLikeCountUpdate: Updating like count - postId: " + postId + ", count: " + juce::String(likeCount));
    // Note: PostCard now updates automatically via FeedStore subscription (Task 2.5)
    // This method is kept for backward compatibility with WebSocket notifications
    // but manual update calls have been removed
    juce::ignoreUnused(postId, likeCount);
}

void PostsFeed::handleFollowerCountUpdate(const juce::String& userId, int followerCount)
{
    // Update follower count in user profile if visible (5.5.4)
    // This would typically update a profile component, but for now we just log
    Log::debug("Follower count update for user " + userId + ": " + juce::String(followerCount));
    // In a full implementation, this would update the profile component
}

void PostsFeed::showNewPostsToast(int count)
{
    // Show toast notification with fade-in animation (5.5.2)
    showingNewPostsToast = true;

    // Fade in toast (0.0 to 1.0 opacity over 200ms)
    toastAnimation = TransitionAnimation<float>::create(0.0f, 1.0f, 200)
        ->withEasing(Easing::easeOutCubic)
        ->onProgress([this](float opacity) {
            currentToastOpacity = opacity;
            repaint();
        })
        ->start();

    // Schedule fade-out after 3 seconds
    juce::Timer::callAfterDelay(3000, [this]() {
        if (!showingNewPostsToast)
            return;  // Toast was manually hidden

        // Fade out toast (1.0 to 0.0 opacity over 200ms)
        toastAnimation = TransitionAnimation<float>::create(1.0f, 0.0f, 200)
            ->withEasing(Easing::easeOutCubic)
            ->onProgress([this](float opacity) {
                currentToastOpacity = opacity;
                repaint();
            })
            ->onComplete([this]() {
                showingNewPostsToast = false;
                repaint();
            })
            ->start();
    });
}

void PostsFeed::timerCallback()
{
    // Toast fade-out is now handled by AnimationValue callback
    // This timer is only used for other timing needs if any
}

//==============================================================================
// R.3.2 Remix Chains - Start remix flow

void PostsFeed::startRemixFlow(const FeedPost& post, const juce::String& remixType)
{
    Log::info("PostsFeed::startRemixFlow: Starting remix - post: " + post.id + ", type: " + remixType);

    // Confirm the remix action
    juce::String confirmMessage = "You're about to remix ";
    if (remixType == "audio")
        confirmMessage += "the audio";
    else if (remixType == "midi")
        confirmMessage += "the MIDI";
    else
        confirmMessage += "both audio and MIDI";
    confirmMessage += " from @" + post.username + "'s post.\n\nYou'll be taken to the recording view to create your remix.";

    auto options = juce::MessageBoxOptions()
        .withTitle("Start Remix")
        .withMessage(confirmMessage)
        .withButton("Start Remix")
        .withButton("Cancel");

    juce::AlertWindow::showAsync(options, [this, post, remixType](int result) {
        if (result != 1)
        {
            Log::debug("PostsFeed::startRemixFlow: User cancelled remix");
            return;
        }

        // Call the remix callback to navigate to recording view with context
        if (onStartRemix)
        {
            // For posts, we pass the post ID (story ID would be empty)
            onStartRemix(post.id, "", remixType);
            Log::info("PostsFeed::startRemixFlow: Navigating to remix recording - postId: " + post.id);
        }
        else
        {
            Log::warn("PostsFeed::startRemixFlow: onStartRemix callback not set");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Error",
                "Remix recording is not available. Please update the plugin.");
        }
    });
}
