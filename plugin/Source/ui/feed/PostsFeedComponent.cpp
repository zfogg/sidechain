#include "PostsFeedComponent.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"

//==============================================================================
PostsFeedComponent::PostsFeedComponent()
{
    setSize(1000, 800);

    // Add scroll bar
    addAndMakeVisible(scrollBar);
    scrollBar.addListener(this);
    scrollBar.setColour(juce::ScrollBar::thumbColourId, SidechainColors::surface());
    scrollBar.setColour(juce::ScrollBar::trackColourId, SidechainColors::backgroundLight());

    // Enable keyboard focus for shortcuts
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    // Create comments panel (initially hidden)
    commentsPanel = std::make_unique<CommentsPanelComponent>();
    commentsPanel->onClose = [this]() { hideCommentsPanel(); };
    commentsPanel->onUserClicked = [this](const juce::String& userId) {
        DBG("User clicked in comments panel: " + userId);
        hideCommentsPanel();
        if (onNavigateToProfile && userId.isNotEmpty())
            onNavigateToProfile(userId);
    };
    addChildComponent(commentsPanel.get());
}

PostsFeedComponent::~PostsFeedComponent()
{
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
}

void PostsFeedComponent::setAudioPlayer(AudioPlayer* player)
{
    audioPlayer = player;

    if (audioPlayer)
    {
        // Set up progress callback to update post cards
        audioPlayer->onProgressUpdate = [this](const juce::String& postId, double progress) {
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
            // Update all cards - only the playing one should show as playing
            for (auto* card : postCards)
            {
                card->setPlaying(card->getPostId() == postId);
            }

            // Track the play in the backend
            if (networkClient != nullptr)
            {
                networkClient->trackPlay(postId, [this, postId](bool success, const juce::var& response) {
                    if (success)
                    {
                        // Update play count in UI if returned in response
                        int newPlayCount = static_cast<int>(response.getProperty("play_count", -1));
                        if (newPlayCount >= 0)
                        {
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
                });
            }
        };

        audioPlayer->onPlaybackPaused = [this](const juce::String& postId) {
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
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setPlaying(false);
                    card->setPlaybackProgress(0.0f);
                    break;
                }
            }
        };

        audioPlayer->onLoadingStarted = [this](const juce::String& postId) {
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setLoading(true);
                    break;
                }
            }
        };

        audioPlayer->onLoadingComplete = [this](const juce::String& postId, bool /*success*/) {
            for (auto* card : postCards)
            {
                if (card->getPostId() == postId)
                {
                    card->setLoading(false);
                    break;
                }
            }
        };
    }
}

//==============================================================================
void PostsFeedComponent::loadFeed()
{
    feedState = FeedState::Loading;
    repaint();

    feedDataManager.setCurrentFeedType(currentFeedType);
    feedDataManager.fetchFeed(currentFeedType, [this](const FeedResponse& response) {
        if (response.error.isNotEmpty())
            onFeedError(response.error);
        else
            onFeedLoaded(response);
    });
}

void PostsFeedComponent::refreshFeed()
{
    feedState = FeedState::Loading;
    repaint();

    feedDataManager.clearCache(currentFeedType);
    feedDataManager.fetchFeed(currentFeedType, [this](const FeedResponse& response) {
        if (response.error.isNotEmpty())
            onFeedError(response.error);
        else
            onFeedLoaded(response);
    });
}

void PostsFeedComponent::switchFeedType(FeedDataManager::FeedType type)
{
    if (currentFeedType == type)
        return;

    currentFeedType = type;
    scrollPosition = 0.0;
    posts.clear();

    // Check if we have valid cache for this feed type
    if (feedDataManager.isCacheValid(type))
    {
        auto cached = feedDataManager.getCachedFeed(type);
        onFeedLoaded(cached);
    }
    else
    {
        loadFeed();
    }
}

//==============================================================================
void PostsFeedComponent::onFeedLoaded(const FeedResponse& response)
{
    posts = response.posts;

    if (posts.isEmpty())
        feedState = FeedState::Empty;
    else
        feedState = FeedState::Loaded;

    rebuildPostCards();
    updateScrollBounds();
    updateAudioPlayerPlaylist();
    repaint();
}

void PostsFeedComponent::onFeedError(const juce::String& error)
{
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

    if (isTimelineActive)
    {
        g.setColour(SidechainColors::primary());
        g.fillRoundedRectangle(timelineTab.reduced(5).toFloat(), 4.0f);
        g.setColour(SidechainColors::textPrimary());
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
    }
    g.setFont(13.0f);
    g.drawText("Following", timelineTab, juce::Justification::centred);

    // Trending tab
    auto trendingTab = getTrendingTabBounds();
    bool isTrendingActive = (currentFeedType == FeedDataManager::FeedType::Trending);

    if (isTrendingActive)
    {
        g.setColour(SidechainColors::primary());
        g.fillRoundedRectangle(trendingTab.reduced(5).toFloat(), 4.0f);
        g.setColour(SidechainColors::textPrimary());
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
    }
    g.drawText("Trending", trendingTab, juce::Justification::centred);

    // Global (Discover) tab
    auto globalTab = getGlobalTabBounds();
    bool isGlobalActive = (currentFeedType == FeedDataManager::FeedType::Global);

    if (isGlobalActive)
    {
        g.setColour(SidechainColors::primary());
        g.fillRoundedRectangle(globalTab.reduced(5).toFloat(), 4.0f);
        g.setColour(SidechainColors::textPrimary());
    }
    else
    {
        g.setColour(SidechainColors::textMuted());
    }
    g.drawText("Discover", globalTab, juce::Justification::centred);

    // Refresh button
    auto refreshBtn = getRefreshButtonBounds();
    g.setColour(feedDataManager.isFetching() ? SidechainColors::textMuted() : SidechainColors::textSecondary());
    g.setFont(18.0f);
    g.drawText("Refresh", refreshBtn, juce::Justification::centred);

    // Bottom border
    g.setColour(SidechainColors::borderSubtle());
    g.drawLine(0.0f, static_cast<float>(tabsBounds.getBottom()), static_cast<float>(getWidth()), static_cast<float>(tabsBounds.getBottom()), 1.0f);
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
    auto actionBtn = getRecordButtonBounds();
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(actionBtn.toFloat(), 8.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    g.drawText("Start Recording", actionBtn, juce::Justification::centred);
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
    auto retryBtn = getRetryButtonBounds();
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(retryBtn.toFloat(), 8.0f);
    g.setColour(SidechainColors::textPrimary());
    g.setFont(16.0f);
    g.drawText("Try Again", retryBtn, juce::Justification::centred);
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

//==============================================================================
void PostsFeedComponent::rebuildPostCards()
{
    postCards.clear();

    for (const auto& post : posts)
    {
        auto* card = postCards.add(new PostCardComponent());
        card->setPost(post);
        setupPostCardCallbacks(card);
        addAndMakeVisible(card);
    }

    updatePostCardPositions();
}

void PostsFeedComponent::updatePostCardPositions()
{
    auto contentBounds = getFeedContentBounds();
    int cardWidth = contentBounds.getWidth() - 40; // Padding

    for (int i = 0; i < postCards.size(); ++i)
    {
        auto* card = postCards[i];
        int cardY = contentBounds.getY() - static_cast<int>(scrollPosition) + i * (POST_CARD_HEIGHT + POST_CARD_SPACING);

        card->setBounds(contentBounds.getX() + 20, cardY, cardWidth, POST_CARD_HEIGHT);

        // Show/hide based on visibility
        bool visible = (cardY + POST_CARD_HEIGHT > contentBounds.getY()) &&
                      (cardY < contentBounds.getBottom());
        card->setVisible(visible);
    }
}

void PostsFeedComponent::setupPostCardCallbacks(PostCardComponent* card)
{
    card->onPlayClicked = [this](const FeedPost& post) {
        DBG("Play clicked for post: " + post.id);
        if (audioPlayer && post.audioUrl.isNotEmpty())
        {
            audioPlayer->loadAndPlay(post.id, post.audioUrl);
        }
    };

    card->onPauseClicked = [this](const FeedPost& post) {
        DBG("Pause clicked for post: " + post.id);
        if (audioPlayer && audioPlayer->isPostPlaying(post.id))
        {
            audioPlayer->pause();
        }
    };

    card->onLikeToggled = [this, card](const FeedPost& post, bool liked) {
        DBG("Like toggled for post: " + post.id + " -> " + (liked ? "liked" : "unliked"));

        // Optimistic UI update
        card->updateLikeCount(post.likeCount + (liked ? 1 : -1), liked);

        // Call backend API (fire-and-forget)
        if (networkClient != nullptr && liked)
        {
            networkClient->likePost(post.id);
        }
    };

    card->onEmojiReaction = [this, card](const FeedPost& post, const juce::String& emoji) {
        DBG("Emoji reaction for post: " + post.id + " -> " + emoji);

        // Optimistic UI update is already done in handleEmojiSelected
        // Call backend API with the emoji
        if (networkClient != nullptr)
        {
            networkClient->likePost(post.id, emoji);
        }
    };

    card->onUserClicked = [this](const FeedPost& post) {
        DBG("User clicked: " + post.username + " (id: " + post.userId + ")");
        if (onNavigateToProfile && post.userId.isNotEmpty())
            onNavigateToProfile(post.userId);
    };

    card->onCommentClicked = [this](const FeedPost& post) {
        DBG("Comments clicked for post: " + post.id);
        showCommentsForPost(post);
    };

    card->onShareClicked = [this](const FeedPost& post) {
        DBG("Share clicked for post: " + post.id);
        // Copy shareable link to clipboard
        juce::String shareUrl = "https://sidechain.live/post/" + post.id;
        juce::SystemClipboard::copyTextToClipboard(shareUrl);
    };

    card->onMoreClicked = [this](const FeedPost& post) {
        DBG("More menu clicked for post: " + post.id);
        // TODO: Show context menu
    };

    card->onFollowToggled = [this, card](const FeedPost& post, bool willFollow) {
        DBG("Follow toggled for user: " + post.userId + " -> " + (willFollow ? "follow" : "unfollow"));

        // Optimistic UI update
        card->updateFollowState(willFollow);

        // Update all other cards by the same user
        for (auto* otherCard : postCards)
        {
            if (otherCard != card && otherCard->getPost().userId == post.userId)
            {
                otherCard->updateFollowState(willFollow);
            }
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
        DBG("Waveform seek for post: " + post.id + " to " + juce::String(position, 2));
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
    auto bounds = getLocalBounds();
    auto contentBounds = getFeedContentBounds();

    // Position scroll bar on right
    scrollBar.setBounds(bounds.getRight() - 12, contentBounds.getY(), 12, contentBounds.getHeight());
    updateScrollBounds();
    updatePostCardPositions();

    // Position comments panel if visible
    if (commentsPanel != nullptr && commentsPanelVisible)
    {
        int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
        commentsPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());
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
        return;

    double scrollAmount = wheel.deltaY * 50.0;
    scrollPosition = juce::jlimit(0.0, static_cast<double>(juce::jmax(0, totalContentHeight - getFeedContentBounds().getHeight())), scrollPosition - scrollAmount);
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
}

void PostsFeedComponent::checkLoadMore()
{
    if (feedState != FeedState::Loaded || !feedDataManager.hasMorePosts() || feedDataManager.isFetching())
        return;

    auto contentBounds = getFeedContentBounds();
    double scrollEnd = scrollPosition + contentBounds.getHeight();
    double threshold = totalContentHeight - 200; // Load more when 200px from bottom

    if (scrollEnd >= threshold)
    {
        feedDataManager.loadMorePosts([this](const FeedResponse& response) {
            if (response.error.isEmpty())
            {
                // Add new posts to array
                posts.addArray(response.posts);

                // Create card components for new posts
                for (const auto& post : response.posts)
                {
                    auto* card = postCards.add(new PostCardComponent());
                    card->setPost(post);
                    setupPostCardCallbacks(card);
                    addAndMakeVisible(card);
                }

                updateScrollBounds();
                updatePostCardPositions();
                repaint();
            }
        });
    }
}

//==============================================================================
void PostsFeedComponent::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check feed tabs - Following, Trending, Discover
    if (getTimelineTabBounds().contains(pos))
    {
        switchFeedType(FeedDataManager::FeedType::Timeline);
        return;
    }

    if (getTrendingTabBounds().contains(pos))
    {
        switchFeedType(FeedDataManager::FeedType::Trending);
        return;
    }

    if (getGlobalTabBounds().contains(pos))
    {
        switchFeedType(FeedDataManager::FeedType::Global);
        return;
    }

    // Check refresh button
    if (getRefreshButtonBounds().contains(pos) && !feedDataManager.isFetching())
    {
        refreshFeed();
        return;
    }

    // Check retry button (error state)
    if (feedState == FeedState::Error && getRetryButtonBounds().contains(pos))
    {
        loadFeed();
        return;
    }

    // Check record button (empty state)
    if (feedState == FeedState::Empty && getRecordButtonBounds().contains(pos))
    {
        if (onStartRecording)
            onStartRecording();
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
        return false;

    // Space bar - toggle play/pause
    if (key == juce::KeyPress::spaceKey)
    {
        audioPlayer->togglePlayPause();
        return true;
    }

    // Right arrow - skip to next
    if (key == juce::KeyPress::rightKey)
    {
        audioPlayer->playNext();
        return true;
    }

    // Left arrow - skip to previous / restart
    if (key == juce::KeyPress::leftKey)
    {
        audioPlayer->playPrevious();
        return true;
    }

    // Up arrow - volume up
    if (key == juce::KeyPress::upKey)
    {
        float newVolume = juce::jmin(1.0f, audioPlayer->getVolume() + 0.1f);
        audioPlayer->setVolume(newVolume);
        return true;
    }

    // Down arrow - volume down
    if (key == juce::KeyPress::downKey)
    {
        float newVolume = juce::jmax(0.0f, audioPlayer->getVolume() - 0.1f);
        audioPlayer->setVolume(newVolume);
        return true;
    }

    // M key - toggle mute
    if (key.getTextCharacter() == 'm' || key.getTextCharacter() == 'M')
    {
        audioPlayer->setMuted(!audioPlayer->isMuted());
        return true;
    }

    // Escape - close comments panel
    if (key == juce::KeyPress::escapeKey && commentsPanelVisible)
    {
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
        return;

    // Set up the panel
    commentsPanel->setNetworkClient(networkClient);
    commentsPanel->setCurrentUserId(currentUserId);
    commentsPanel->loadCommentsForPost(post.id);

    // Position as right-side panel (takes 40% of width)
    int panelWidth = juce::jmin(400, static_cast<int>(getWidth() * 0.4));
    commentsPanel->setBounds(getWidth() - panelWidth, 0, panelWidth, getHeight());

    // Show with animation
    commentsPanel->setVisible(true);
    commentsPanelVisible = true;

    // Bring to front
    commentsPanel->toFront(true);

    repaint();
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
        return;

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

    audioPlayer->setPlaylist(postIds, audioUrls);
}
