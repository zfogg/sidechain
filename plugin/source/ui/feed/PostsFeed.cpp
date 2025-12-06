#include "PostsFeed.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"
#include "../../util/Json.h"
#include "../../util/UIHelpers.h"
#include "../../util/Log.h"
#include "../../util/Async.h"
#include "../../util/Animation.h"
#include "../../util/Result.h"

//==============================================================================
PostsFeedComponent::PostsFeedComponent()
{
    Log::info("PostsFeedComponent: Initializing feed component");
    setSize(1000, 800);

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
    commentsPanelSlide.onValueChanged = [this](float slide) {
        if (commentsPanel != nullptr)
        {
            int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
            int targetX = static_cast<int>(getWidth() - panelWidth * slide);
            commentsPanel->setBounds(targetX, 0, panelWidth, getHeight());
        }
        repaint();
    };

    // Set up toast fade animation
    toastOpacity.onValueChanged = [this](float opacity) {
        repaint();
    };
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
    Log::info("PostsFeedComponent: Initialization complete");
}

PostsFeedComponent::~PostsFeedComponent()
{
    Log::debug("PostsFeedComponent: Destroying feed component");
    removeKeyListener(this);
    scrollBar.removeListener(this);
}

//==============================================================================
void PostsFeedComponent::setUserInfo(const juce::String& user, const juce::String& userEmail, const juce::String& picUrl)
{
    // Store user info (profile picture now displayed in central HeaderComponent)
    username = user;
    email = userEmail;
    profilePicUrl = picUrl;
    repaint();
}

void PostsFeedComponent::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    feedDataManager.setNetworkClient(client);
    Log::info("PostsFeedComponent::setNetworkClient: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void PostsFeedComponent::setAudioPlayer(HttpAudioPlayer* player)
{
    Log::info("PostsFeedComponent::setAudioPlayer: Setting audio player " + juce::String(player != nullptr ? "(valid)" : "(null)"));
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
                            for (auto* card : postCards)
                            {
                                if (card->getPostId() == postId)
                                {
                                    card->updatePlayCount(newPlayCount);
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        Log::warn("PostsFeedComponent: Play tracking failed for postId: " + postId);
                    }
                });
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
void PostsFeedComponent::loadFeed()
{
    juce::String feedTypeStr = currentFeedType == FeedDataManager::FeedType::Timeline ? "Timeline" :
                               currentFeedType == FeedDataManager::FeedType::Trending ? "Trending" : "Global";
    Log::info("PostsFeedComponent::loadFeed: Loading feed - type: " + feedTypeStr);
    feedState = FeedState::Loading;
    repaint();

    feedDataManager.setCurrentFeedType(currentFeedType);
    feedDataManager.fetchFeed(currentFeedType, [this, feedTypeStr](const FeedResponse& response) {
        if (response.error.isNotEmpty())
        {
            Log::error("PostsFeedComponent::loadFeed: Feed load failed - type: " + feedTypeStr + ", error: " + response.error);
            onFeedError(response.error);
        }
        else
        {
            Log::info("PostsFeedComponent::loadFeed: Feed loaded successfully - type: " + feedTypeStr + ", posts: " + juce::String(response.posts.size()));
            onFeedLoaded(response);
        }
    });
}

void PostsFeedComponent::refreshFeed()
{
    juce::String feedTypeStr = currentFeedType == FeedDataManager::FeedType::Timeline ? "Timeline" :
                               currentFeedType == FeedDataManager::FeedType::Trending ? "Trending" : "Global";
    Log::info("PostsFeedComponent::refreshFeed: Refreshing feed - type: " + feedTypeStr);
    feedState = FeedState::Loading;
    repaint();

    feedDataManager.clearCache(currentFeedType);
    Log::debug("PostsFeedComponent::refreshFeed: Cache cleared for type: " + feedTypeStr);
    feedDataManager.fetchFeed(currentFeedType, [this, feedTypeStr](const FeedResponse& response) {
        if (response.error.isNotEmpty())
        {
            Log::error("PostsFeedComponent::refreshFeed: Feed refresh failed - type: " + feedTypeStr + ", error: " + response.error);
            onFeedError(response.error);
        }
        else
        {
            Log::info("PostsFeedComponent::refreshFeed: Feed refreshed successfully - type: " + feedTypeStr + ", posts: " + juce::String(response.posts.size()));
            onFeedLoaded(response);
        }
    });
}

void PostsFeedComponent::switchFeedType(FeedDataManager::FeedType type)
{
    juce::String typeStr = type == FeedDataManager::FeedType::Timeline ? "Timeline" :
                           type == FeedDataManager::FeedType::Trending ? "Trending" : "Global";

    if (currentFeedType == type)
    {
        Log::debug("PostsFeedComponent::switchFeedType: Already on feed type: " + typeStr);
        return;
    }

    juce::String oldTypeStr = currentFeedType == FeedDataManager::FeedType::Timeline ? "Timeline" :
                              currentFeedType == FeedDataManager::FeedType::Trending ? "Trending" : "Global";
    Log::info("PostsFeedComponent::switchFeedType: Switching from " + oldTypeStr + " to " + typeStr);

    currentFeedType = type;
    scrollPosition = 0.0;
    posts.clear();
    Log::debug("PostsFeedComponent::switchFeedType: Reset scroll position and cleared posts");

    // Check if we have valid cache for this feed type
    if (feedDataManager.isCacheValid(type))
    {
        Log::debug("PostsFeedComponent::switchFeedType: Using cached feed for type: " + typeStr);
        auto cached = feedDataManager.getCachedFeed(type);
        onFeedLoaded(cached);
    }
    else
    {
        Log::debug("PostsFeedComponent::switchFeedType: No valid cache, loading feed for type: " + typeStr);
        loadFeed();
    }
}

//==============================================================================
void PostsFeedComponent::onFeedLoaded(const FeedResponse& response)
{
    Log::info("PostsFeedComponent::onFeedLoaded: Feed loaded - posts: " + juce::String(response.posts.size()));
    posts = response.posts;

    if (posts.isEmpty())
    {
        feedState = FeedState::Empty;
        Log::debug("PostsFeedComponent::onFeedLoaded: Feed is empty");
    }
    else
    {
        feedState = FeedState::Loaded;
        Log::debug("PostsFeedComponent::onFeedLoaded: Feed has " + juce::String(posts.size()) + " posts");
    }

    rebuildPostCards();
    updateScrollBounds();
    updateAudioPlayerPlaylist();
    repaint();
}

void PostsFeedComponent::onFeedError(const juce::String& error)
{
    Log::error("PostsFeedComponent::onFeedError: Feed error - " + error);
    errorMessage = error;
    feedState = FeedState::Error;
    repaint();
}

//==============================================================================
void PostsFeedComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(SidechainColors::background());

    // Feed type tabs (top bar now handled by central HeaderComponent)
    drawFeedTabs(g);

    // Main feed area based on state
    switch (feedState)
    {
        case FeedState::Loading:
            drawLoadingState(g);
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
            drawErrorState(g);
            break;
    }
}

void PostsFeedComponent::drawFeedTabs(juce::Graphics& g)
{
    // Tabs now start at top (header handled by central HeaderComponent)
    auto tabsBounds = getLocalBounds().withHeight(FEED_TABS_HEIGHT);

    // Tabs background
    g.setColour(SidechainColors::background());
    g.fillRect(tabsBounds);

    // Timeline (Following) tab
    auto timelineTab = getTimelineTabBounds();
    bool isTimelineActive = (currentFeedType == FeedDataManager::FeedType::Timeline);

    // Use UI::drawButton for consistent tab styling
    if (isTimelineActive)
    {
        UI::drawButton(g, timelineTab.reduced(5), "Following",
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
    bool isTrendingActive = (currentFeedType == FeedDataManager::FeedType::Trending);

    // Use UI::drawButton for consistent tab styling
    if (isTrendingActive)
    {
        UI::drawButton(g, trendingTab.reduced(5), "Trending",
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
    bool isGlobalActive = (currentFeedType == FeedDataManager::FeedType::Global);

    // Use UI::drawButton for consistent tab styling
    if (isGlobalActive)
    {
        UI::drawButton(g, globalTab.reduced(5), "Discover",
            SidechainColors::primary(), SidechainColors::textPrimary(), false, 4.0f);
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(13.0f);
        g.drawText("Discover", globalTab, juce::Justification::centred);
    }

    // Refresh button
    auto refreshBtn = getRefreshButtonBounds();
    g.setColour(feedDataManager.isFetching() ? SidechainColors::textMuted() : SidechainColors::textSecondary());
    g.setFont(18.0f);
    g.drawText("Refresh", refreshBtn, juce::Justification::centred);

    // Bottom border - use UI::drawDivider for consistency
    UI::drawDivider(g, 0, tabsBounds.getBottom(), getWidth(),
        SidechainColors::borderSubtle(), 1.0f);
}

void PostsFeedComponent::drawLoadingState(juce::Graphics& g)
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

void PostsFeedComponent::drawEmptyState(juce::Graphics& g)
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 300);

    // Different message for Timeline vs Global
    juce::String title, subtitle1, subtitle2;

    if (currentFeedType == FeedDataManager::FeedType::Timeline)
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
    // Use UI::drawButton for consistent button styling
    auto actionBtn = getRecordButtonBounds();
    UI::drawButton(g, actionBtn, "Start Recording",
        SidechainColors::primary(), SidechainColors::textPrimary(), false, 8.0f);
}

void PostsFeedComponent::drawErrorState(juce::Graphics& g)
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
    juce::String displayError = errorMessage.isEmpty() ? "Network error. Please check your connection." : errorMessage;
    g.drawFittedText(displayError, centerBounds.withY(centerBounds.getY() + 130).withHeight(40), juce::Justification::centred, 2);

    // Retry button
    // Use UI::drawButton for consistent button styling
    auto retryBtn = getRetryButtonBounds();
    UI::drawButton(g, retryBtn, "Try Again",
        SidechainColors::primary(), SidechainColors::textPrimary(), false, 8.0f);
}

void PostsFeedComponent::drawFeedPosts(juce::Graphics& g)
{
    // Post cards are now child components, just update their visibility
    updatePostCardPositions();

    // Loading more indicator at bottom
    if (feedDataManager.isFetching() && feedDataManager.hasMorePosts())
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

void PostsFeedComponent::drawNewPostsToast(juce::Graphics& g)
{
    if (!showingNewPostsToast)
        return;

    // Draw toast at top of feed content area with fade animation (5.5.2)
    auto contentBounds = getFeedContentBounds();
    auto toastBounds = contentBounds.withHeight(40).withY(contentBounds.getY() + 10);

    float opacity = toastOpacity.getValue();
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
    g.setFont(juce::Font(14.0f).boldened());

    juce::String toastText;
    if (pendingNewPostsCount == 1)
        toastText = "1 new post";
    else
        toastText = juce::String(pendingNewPostsCount) + " new posts";
    toastText += " - Click to refresh";

    g.drawText(toastText, toastBounds.reduced(15, 0), juce::Justification::centredLeft);

    // Clickable indicator (faded)
    g.setFont(juce::Font(12.0f));
    g.drawText("↻", toastBounds.removeFromRight(30), juce::Justification::centred);
}

//==============================================================================
void PostsFeedComponent::rebuildPostCards()
{
    Log::info("PostsFeedComponent::rebuildPostCards: Rebuilding post cards - current: " + juce::String(postCards.size()) + ", posts: " + juce::String(posts.size()));
    postCards.clear();

    for (const auto& post : posts)
    {
        auto* card = postCards.add(new PostCard());
        card->setPost(post);
        setupPostCardCallbacks(card);
        addAndMakeVisible(card);
        Log::debug("PostsFeedComponent::rebuildPostCards: Created card for post: " + post.id);
    }

    updatePostCardPositions();
    Log::debug("PostsFeedComponent::rebuildPostCards: Rebuilt " + juce::String(postCards.size()) + " post cards");
}

void PostsFeedComponent::updatePostCardPositions()
{
    auto contentBounds = getFeedContentBounds();
    int cardWidth = contentBounds.getWidth() - 40; // Padding
    int visibleCount = 0;

    for (int i = 0; i < postCards.size(); ++i)
    {
        auto* card = postCards[i];
        int cardY = contentBounds.getY() - static_cast<int>(scrollPosition) + i * (POST_CARD_HEIGHT + POST_CARD_SPACING);

        card->setBounds(contentBounds.getX() + 20, cardY, cardWidth, POST_CARD_HEIGHT);

        // Show/hide based on visibility
        bool visible = (cardY + POST_CARD_HEIGHT > contentBounds.getY()) &&
                      (cardY < contentBounds.getBottom());
        card->setVisible(visible);
        if (visible)
            visibleCount++;
    }

    Log::debug("PostsFeedComponent::updatePostCardPositions: Updated positions - total: " + juce::String(postCards.size()) + ", visible: " + juce::String(visibleCount) + ", scrollPosition: " + juce::String(scrollPosition, 1));
}

void PostsFeedComponent::setupPostCardCallbacks(PostCard* card)
{
    card->onPlayClicked = [this](const FeedPost& post) {
        Log::debug("Play clicked for post: " + post.id);
        if (audioPlayer && post.audioUrl.isNotEmpty())
        {
            audioPlayer->loadAndPlay(post.id, post.audioUrl);
        }
    };

    card->onPauseClicked = [this](const FeedPost& post) {
        Log::debug("Pause clicked for post: " + post.id);
        if (audioPlayer && audioPlayer->isPostPlaying(post.id))
        {
            audioPlayer->pause();
        }
    };

    card->onLikeToggled = [this, card](const FeedPost& post, bool liked) {
        Log::debug("Like toggled for post: " + post.id + " -> " + (liked ? "liked" : "unliked"));

        // Store original state for conflict resolution (5.5.6)
        int originalCount = post.likeCount;
        bool originalLiked = post.isLiked;

        // Optimistic UI update (5.5.5) - update immediately for instant feedback
        int optimisticCount = originalCount + (liked ? 1 : -1);
        card->updateLikeCount(optimisticCount, liked);

        // Call backend API with callback to handle conflicts (5.5.5, 5.5.6)
        if (networkClient != nullptr)
        {
            auto callback = [card, originalCount, originalLiked](Outcome<juce::var> responseOutcome) {
                if (responseOutcome.isOk())
                {
                    // Server confirmed - check if count matches our optimistic update (5.5.6)
                    // Note: likePost API may not return like_count, so we rely on WebSocket updates
                    // But we can still verify the action succeeded
                    Log::debug("Like API call succeeded");
                    // Real count will come via WebSocket update (5.5.3)
                }
                else
                {
                    // API call failed - revert optimistic update (5.5.6)
                    Log::warn("Like API call failed - reverting optimistic update: " + responseOutcome.getError());
                    card->updateLikeCount(originalCount, originalLiked);
                }
            };

            if (liked)
                networkClient->likePost(post.id, "", callback);
            else
                networkClient->unlikePost(post.id, callback);
        }
    };

    card->onEmojiReaction = [this](const FeedPost& post, const juce::String& emoji) {
        Log::debug("Emoji reaction for post: " + post.id + " -> " + emoji);

        // Optimistic UI update is already done in handleEmojiSelected
        // Call backend API with the emoji
        if (networkClient != nullptr)
        {
            networkClient->likePost(post.id, emoji);
        }
    };

    card->onUserClicked = [this](const FeedPost& post) {
        Log::debug("User clicked: " + post.username + " (id: " + post.userId + ")");
        if (onNavigateToProfile && post.userId.isNotEmpty())
            onNavigateToProfile(post.userId);
    };

    card->onCommentClicked = [this](const FeedPost& post) {
        Log::debug("Comments clicked for post: " + post.id);
        showCommentsForPost(post);
    };

    card->onShareClicked = [](const FeedPost& post) {
        Log::debug("Share clicked for post: " + post.id);
        // Copy shareable link to clipboard
        juce::String shareUrl = "https://sidechain.live/post/" + post.id;
        juce::SystemClipboard::copyTextToClipboard(shareUrl);
    };

    card->onMoreClicked = [](const FeedPost& post) {
        Log::info("PostsFeedComponent: More menu clicked for post: " + post.id);
        // TODO: Show context menu
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
                            // TODO: Show success notification
                        });
                    }
                    else
                    {
                        juce::MessageManager::callAsync([targetFile]() {
                            Log::error("Failed to write audio file: " + targetFile.getFullPathName());
                            // TODO: Show error notification
                        });
                    }
                }
                else
                {
                    juce::MessageManager::callAsync([post]() {
                        Log::error("Failed to download audio from: " + post.audioUrl);
                        // TODO: Show error notification
                    });
                }
            });
        });
    };

    card->onFollowToggled = [this, card](const FeedPost& post, bool willFollow) {
        Log::info("PostsFeedComponent: Follow toggled for user: " + post.userId + " -> " + (willFollow ? "follow" : "unfollow"));

        // Optimistic UI update
        card->updateFollowState(willFollow);

        // Update all other cards by the same user
        int updatedCards = 0;
        for (auto* otherCard : postCards)
        {
            if (otherCard != card && otherCard->getPost().userId == post.userId)
            {
                otherCard->updateFollowState(willFollow);
                updatedCards++;
            }
        }
        if (updatedCards > 0)
        {
            Log::debug("PostsFeedComponent: Updated follow state for " + juce::String(updatedCards) + " other card(s) by same user");
        }

        // TODO: Call backend API to follow/unfollow
        // feedDataManager.followUser(post.userId, willFollow, [this, post, willFollow](bool success) {
        //     if (!success) {
        //         // Revert on failure
        //         for (auto* c : postCards) {
        //             if (c->getPost().userId == post.userId)
        //                 c->updateFollowState(!willFollow);
        //         }
        //     }
        // });
    };

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
}

//==============================================================================
void PostsFeedComponent::resized()
{
    Log::debug("PostsFeedComponent::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
    auto bounds = getLocalBounds();
    auto contentBounds = getFeedContentBounds();

    // Position scroll bar on right
    scrollBar.setBounds(bounds.getRight() - 12, contentBounds.getY(), 12, contentBounds.getHeight());
    updateScrollBounds();
    updatePostCardPositions();

    // Position comments panel if visible (animation will handle position updates)
    if (commentsPanel != nullptr && commentsPanelVisible)
    {
        // Animation callback will update position, but ensure initial position is set
        if (!commentsPanelSlide.isAnimating())
        {
            int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
            commentsPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
        }
        Log::debug("PostsFeedComponent::resized: Comments panel repositioned");
    }
}

void PostsFeedComponent::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &scrollBar)
    {
        scrollPosition = newRangeStart;
        checkLoadMore();
        repaint();
    }
}

void PostsFeedComponent::mouseWheelMove(const juce::MouseEvent& /*event*/, const juce::MouseWheelDetails& wheel)
{
    if (feedState != FeedState::Loaded)
    {
        Log::debug("PostsFeedComponent::mouseWheelMove: Ignoring wheel - feed not loaded");
        return;
    }

    double scrollAmount = wheel.deltaY * 50.0;
    double oldPosition = scrollPosition;
    scrollPosition = juce::jlimit(0.0, static_cast<double>(juce::jmax(0, totalContentHeight - getFeedContentBounds().getHeight())), scrollPosition - scrollAmount);
    Log::debug("PostsFeedComponent::mouseWheelMove: Wheel scroll - delta: " + juce::String(wheel.deltaY, 2) + ", position: " + juce::String(oldPosition, 1) + " -> " + juce::String(scrollPosition, 1));
    scrollBar.setCurrentRangeStart(scrollPosition);
    checkLoadMore();
    repaint();
}

void PostsFeedComponent::updateScrollBounds()
{
    auto contentBounds = getFeedContentBounds();
    totalContentHeight = static_cast<int>(posts.size()) * (POST_CARD_HEIGHT + POST_CARD_SPACING);

    double visibleHeight = contentBounds.getHeight();
    scrollBar.setRangeLimits(0.0, juce::jmax(static_cast<double>(totalContentHeight), visibleHeight));
    scrollBar.setCurrentRange(scrollPosition, visibleHeight);
    Log::debug("PostsFeedComponent::updateScrollBounds: Scroll bounds updated - totalHeight: " + juce::String(totalContentHeight) + ", visibleHeight: " + juce::String(visibleHeight, 1));
}

void PostsFeedComponent::checkLoadMore()
{
    if (feedState != FeedState::Loaded || !feedDataManager.hasMorePosts() || feedDataManager.isFetching())
    {
        if (feedState != FeedState::Loaded)
            Log::debug("PostsFeedComponent::checkLoadMore: Feed not loaded, skipping");
        else if (!feedDataManager.hasMorePosts())
            Log::debug("PostsFeedComponent::checkLoadMore: No more posts available");
        else if (feedDataManager.isFetching())
            Log::debug("PostsFeedComponent::checkLoadMore: Already fetching, skipping");
        return;
    }

    auto contentBounds = getFeedContentBounds();
    double scrollEnd = scrollPosition + contentBounds.getHeight();
    double threshold = totalContentHeight - 200; // Load more when 200px from bottom

    Log::debug("PostsFeedComponent::checkLoadMore: Checking threshold - scrollEnd: " + juce::String(scrollEnd, 1) + ", threshold: " + juce::String(threshold, 1) + ", totalHeight: " + juce::String(totalContentHeight));

    if (scrollEnd >= threshold)
    {
        Log::info("PostsFeedComponent::checkLoadMore: Threshold reached, loading more posts");
        feedDataManager.loadMorePosts([this](const FeedResponse& response) {
            if (response.error.isEmpty())
            {
                // Add new posts to array
                posts.addArray(response.posts);
                Log::info("PostsFeedComponent::checkLoadMore: Loaded " + juce::String(response.posts.size()) + " more posts (total: " + juce::String(posts.size()) + ")");

                // Create card components for new posts
                for (const auto& post : response.posts)
                {
                    auto* card = postCards.add(new PostCard());
                    card->setPost(post);
                    setupPostCardCallbacks(card);
                    addAndMakeVisible(card);
                    Log::debug("PostsFeedComponent::checkLoadMore: Created card for new post: " + post.id);
                }

                updateScrollBounds();
                updatePostCardPositions();
                repaint();
            }
            else
            {
                Log::error("PostsFeedComponent::checkLoadMore: Failed to load more posts - error: " + response.error);
            }
        });
    }
}

//==============================================================================
void PostsFeedComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    Log::debug("PostsFeedComponent::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

    // Check if clicked on toast to refresh (5.5.2)
    if (showingNewPostsToast && pendingNewPostsCount > 0)
    {
        auto contentBounds = getFeedContentBounds();
        auto toastBounds = contentBounds.withHeight(40).withY(contentBounds.getY() + 10);
        if (toastBounds.contains(pos))
        {
            Log::info("PostsFeedComponent::mouseUp: New posts toast clicked, refreshing feed");
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
        Log::info("PostsFeedComponent::mouseUp: Timeline tab clicked");
        switchFeedType(FeedDataManager::FeedType::Timeline);
        return;
    }

    if (getTrendingTabBounds().contains(pos))
    {
        Log::info("PostsFeedComponent::mouseUp: Trending tab clicked");
        switchFeedType(FeedDataManager::FeedType::Trending);
        return;
    }

    if (getGlobalTabBounds().contains(pos))
    {
        Log::info("PostsFeedComponent::mouseUp: Global/Discover tab clicked");
        switchFeedType(FeedDataManager::FeedType::Global);
        return;
    }

    // Check refresh button
    if (getRefreshButtonBounds().contains(pos) && !feedDataManager.isFetching())
    {
        Log::info("PostsFeedComponent::mouseUp: Refresh button clicked");
        refreshFeed();
        return;
    }

    // Check retry button (error state)
    if (feedState == FeedState::Error && getRetryButtonBounds().contains(pos))
    {
        Log::info("PostsFeedComponent::mouseUp: Retry button clicked");
        loadFeed();
        return;
    }

    // Check record button (empty state)
    if (feedState == FeedState::Empty && getRecordButtonBounds().contains(pos))
    {
        Log::info("PostsFeedComponent::mouseUp: Record button clicked");
        if (onStartRecording)
            onStartRecording();
        else
            Log::warn("PostsFeedComponent::mouseUp: Record button clicked but callback not set");
        return;
    }

    // Note: Discover/search and profile clicks now handled by central HeaderComponent
}

//==============================================================================
juce::Rectangle<int> PostsFeedComponent::getTimelineTabBounds() const
{
    // Tabs now start at y=0 (header handled by central HeaderComponent)
    // Three tabs: Following, Trending, Discover - each 80px wide with 10px gaps
    return juce::Rectangle<int>(15, 10, 80, 30);
}

juce::Rectangle<int> PostsFeedComponent::getTrendingTabBounds() const
{
    return juce::Rectangle<int>(105, 10, 80, 30);
}

juce::Rectangle<int> PostsFeedComponent::getGlobalTabBounds() const
{
    return juce::Rectangle<int>(195, 10, 80, 30);
}

juce::Rectangle<int> PostsFeedComponent::getRefreshButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 100, 10, 80, 30);
}

juce::Rectangle<int> PostsFeedComponent::getRetryButtonBounds() const
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 250);
    return juce::Rectangle<int>(centerBounds.getCentreX() - 75, centerBounds.getY() + 190, 150, 45);
}

juce::Rectangle<int> PostsFeedComponent::getRecordButtonBounds() const
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 300);
    return juce::Rectangle<int>(centerBounds.getCentreX() - 100, centerBounds.getY() + 230, 200, 50);
}

juce::Rectangle<int> PostsFeedComponent::getFeedContentBounds() const
{
    // Content starts below feed tabs (header handled by central HeaderComponent)
    return getLocalBounds().withTrimmedTop(FEED_TABS_HEIGHT);
}

//==============================================================================
// Keyboard shortcuts

bool PostsFeedComponent::keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/)
{
    if (audioPlayer == nullptr)
    {
        Log::debug("PostsFeedComponent::keyPressed: AudioPlayer is null, ignoring key press");
        return false;
    }

    // Space bar - toggle play/pause
    if (key == juce::KeyPress::spaceKey)
    {
        Log::info("PostsFeedComponent::keyPressed: Space bar - toggling play/pause");
        audioPlayer->togglePlayPause();
        return true;
    }

    // Right arrow - skip to next
    if (key == juce::KeyPress::rightKey)
    {
        Log::info("PostsFeedComponent::keyPressed: Right arrow - skipping to next");
        audioPlayer->playNext();
        return true;
    }

    // Left arrow - skip to previous / restart
    if (key == juce::KeyPress::leftKey)
    {
        Log::info("PostsFeedComponent::keyPressed: Left arrow - skipping to previous");
        audioPlayer->playPrevious();
        return true;
    }

    // Up arrow - volume up
    if (key == juce::KeyPress::upKey)
    {
        float oldVolume = audioPlayer->getVolume();
        float newVolume = juce::jmin(1.0f, audioPlayer->getVolume() + 0.1f);
        audioPlayer->setVolume(newVolume);
        Log::debug("PostsFeedComponent::keyPressed: Up arrow - volume " + juce::String(oldVolume, 2) + " -> " + juce::String(newVolume, 2));
        return true;
    }

    // Down arrow - volume down
    if (key == juce::KeyPress::downKey)
    {
        float oldVolume = audioPlayer->getVolume();
        float newVolume = juce::jmax(0.0f, audioPlayer->getVolume() - 0.1f);
        audioPlayer->setVolume(newVolume);
        Log::debug("PostsFeedComponent::keyPressed: Down arrow - volume " + juce::String(oldVolume, 2) + " -> " + juce::String(newVolume, 2));
        return true;
    }

    // M key - toggle mute
    if (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M')
    {
        bool wasMuted = audioPlayer->isMuted();
        audioPlayer->setMuted(!wasMuted);
        Log::info("PostsFeedComponent::keyPressed: M key - mute toggled " + juce::String(wasMuted ? "off" : "on"));
        return true;
    }

    // Escape - close comments panel
    if (key == juce::KeyPress::escapeKey && commentsPanelVisible)
    {
        Log::info("PostsFeedComponent::keyPressed: Escape key - closing comments panel");
        hideCommentsPanel();
        return true;
    }

    return false;
}

//==============================================================================
// Comments panel

void PostsFeedComponent::showCommentsForPost(const FeedPost& post)
{
    if (commentsPanel == nullptr)
    {
        Log::warn("PostsFeedComponent::showCommentsForPost: Comments panel is null");
        return;
    }

    Log::info("PostsFeedComponent::showCommentsForPost: Showing comments for post: " + post.id);

    // Set up the panel
    commentsPanel->setNetworkClient(networkClient);
    commentsPanel->setCurrentUserId(currentUserId);
    commentsPanel->loadCommentsForPost(post.id);
    Log::debug("PostsFeedComponent::showCommentsForPost: Comments panel configured and loading comments");

    // Position as right-side panel (takes 40% of width)
    int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
    commentsPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
    Log::debug("PostsFeedComponent::showCommentsForPost: Comments panel positioned - width: " + juce::String(panelWidth));

    // Show with slide animation
    commentsPanel->setVisible(true);
    commentsPanelVisible = true;
    commentsPanelSlide.animateTo(1.0f);  // Slide in from right

    // Bring to front
    commentsPanel->toFront(true);

    Log::debug("PostsFeedComponent::showCommentsForPost: Comments panel shown with animation");
}

void PostsFeedComponent::hideCommentsPanel()
{
    if (commentsPanel == nullptr)
        return;

    commentsPanel->setVisible(false);
    commentsPanelVisible = false;
    repaint();
}

//==============================================================================
// Playlist management for auto-play

void PostsFeedComponent::updateAudioPlayerPlaylist()
{
    if (audioPlayer == nullptr)
    {
        Log::warn("PostsFeedComponent::updateAudioPlayerPlaylist: AudioPlayer is null");
        return;
    }

    juce::StringArray postIds;
    juce::StringArray audioUrls;

    for (const auto& post : posts)
    {
        if (post.audioUrl.isNotEmpty())
        {
            postIds.add(post.id);
            audioUrls.add(post.audioUrl);
        }
    }

    Log::info("PostsFeedComponent::updateAudioPlayerPlaylist: Updating playlist - posts: " + juce::String(postIds.size()) + " with audio");
    audioPlayer->setPlaylist(postIds, audioUrls);
}

//==============================================================================
// Real-time Feed Updates (5.5)
//==============================================================================

void PostsFeedComponent::handleNewPostNotification(const juce::var& postData)
{
    Log::info("PostsFeedComponent::handleNewPostNotification: New post notification received");
    // Increment pending new posts count (5.5.2)
    pendingNewPostsCount++;
    lastNewPostTime = juce::Time::getCurrentTime();
    Log::debug("PostsFeedComponent::handleNewPostNotification: Pending count: " + juce::String(pendingNewPostsCount));

    // Show toast notification if feed is visible and user is not at the top
    if (isVisible() && scrollPosition > 0.1)
    {
        Log::debug("PostsFeedComponent::handleNewPostNotification: User scrolled, showing toast");
        showNewPostsToast(pendingNewPostsCount);
    }

    // If user is at the top of the feed, refresh immediately (5.5.1)
    if (isVisible() && scrollPosition < 0.1)
    {
        Log::info("PostsFeedComponent::handleNewPostNotification: User at top, refreshing feed immediately");
        refreshFeed();
        pendingNewPostsCount = 0;
    }

    repaint();
}

void PostsFeedComponent::handleLikeCountUpdate(const juce::String& postId, int likeCount)
{
    Log::debug("PostsFeedComponent::handleLikeCountUpdate: Updating like count - postId: " + postId + ", count: " + juce::String(likeCount));
    // Find the post card and update like count (5.5.3)
    bool found = false;
    for (auto* card : postCards)
    {
        if (card->getPostId() == postId)
        {
            // Get current liked state before updating
            bool wasLiked = card->getPost().isLiked;
            card->updateLikeCount(likeCount, wasLiked);
            found = true;
            Log::debug("PostsFeedComponent::handleLikeCountUpdate: Updated like count for post: " + postId);
            break;
        }
    }

    if (!found)
    {
        Log::warn("PostsFeedComponent::handleLikeCountUpdate: Post card not found for postId: " + postId);
    }
}

void PostsFeedComponent::handleFollowerCountUpdate(const juce::String& userId, int followerCount)
{
    // Update follower count in user profile if visible (5.5.4)
    // This would typically update a profile component, but for now we just log
    Log::debug("Follower count update for user " + userId + ": " + juce::String(followerCount));
    // In a full implementation, this would update the profile component
}

void PostsFeedComponent::showNewPostsToast(int count)
{
    // Show toast notification with fade-in animation (5.5.2)
    showingNewPostsToast = true;
    toastOpacity.animateTo(1.0f);  // Fade in

    // Hide toast after 3 seconds with fade-out
    juce::Timer::callAfterDelay(3000, [this]() {
        toastOpacity.animateTo(0.0f);
        toastOpacity.onAnimationComplete = [this]() {
            showingNewPostsToast = false;
            repaint();
        };
    });
}

void PostsFeedComponent::timerCallback()
{
    // Toast fade-out is now handled by AnimationValue callback
    // This timer is only used for other timing needs if any
}
