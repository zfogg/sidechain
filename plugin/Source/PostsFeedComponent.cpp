#include "PostsFeedComponent.h"
#include "NetworkClient.h"

//==============================================================================
PostsFeedComponent::PostsFeedComponent()
{
    setSize(1000, 800);

    // Add scroll bar
    addAndMakeVisible(scrollBar);
    scrollBar.addListener(this);
    scrollBar.setColour(juce::ScrollBar::thumbColourId, juce::Colour::fromRGB(80, 80, 80));
    scrollBar.setColour(juce::ScrollBar::trackColourId, juce::Colour::fromRGB(40, 40, 40));

    // Enable keyboard focus for shortcuts
    setWantsKeyboardFocus(true);
    addKeyListener(this);
}

PostsFeedComponent::~PostsFeedComponent()
{
    removeKeyListener(this);
    scrollBar.removeListener(this);
}

//==============================================================================
void PostsFeedComponent::setUserInfo(const juce::String& user, const juce::String& userEmail, const juce::String& picUrl)
{
    username = user;
    email = userEmail;
    profilePicUrl = picUrl;
    repaint();
}

void PostsFeedComponent::setNetworkClient(NetworkClient* client)
{
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
    g.fillAll(juce::Colour::fromRGB(25, 25, 25));

    // Top navigation bar
    drawTopBar(g);

    // Feed type tabs
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

void PostsFeedComponent::drawTopBar(juce::Graphics& g)
{
    auto topBarBounds = getLocalBounds().withHeight(TOP_BAR_HEIGHT);

    // Top bar background
    g.setColour(juce::Colour::fromRGB(35, 35, 35));
    g.fillRect(topBarBounds);

    // App title
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("Sidechain", topBarBounds.withX(20).withWidth(200), juce::Justification::centredLeft);

    // Profile section (right side)
    auto profileBounds = topBarBounds.withX(getWidth() - 200).withWidth(180);

    // Small profile pic
    auto smallPicBounds = juce::Rectangle<int>(profileBounds.getX() + 10, profileBounds.getCentreY() - 20, 40, 40);
    drawCircularProfilePic(g, smallPicBounds, true);

    // Username
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(username, smallPicBounds.withX(smallPicBounds.getRight() + 10).withWidth(100), juce::Justification::centredLeft);

    // Border
    g.setColour(juce::Colour::fromRGB(60, 60, 60));
    g.drawLine(0.0f, static_cast<float>(topBarBounds.getBottom()), static_cast<float>(getWidth()), static_cast<float>(topBarBounds.getBottom()), 1.0f);
}

void PostsFeedComponent::drawFeedTabs(juce::Graphics& g)
{
    auto tabsBounds = getLocalBounds().withY(TOP_BAR_HEIGHT).withHeight(FEED_TABS_HEIGHT);

    // Tabs background
    g.setColour(juce::Colour::fromRGB(30, 30, 30));
    g.fillRect(tabsBounds);

    // Timeline tab
    auto timelineTab = getTimelineTabBounds();
    bool isTimelineActive = (currentFeedType == FeedDataManager::FeedType::Timeline);

    if (isTimelineActive)
    {
        g.setColour(juce::Colour::fromRGB(0, 212, 255));
        g.fillRoundedRectangle(timelineTab.reduced(5).toFloat(), 4.0f);
        g.setColour(juce::Colours::white);
    }
    else
    {
        g.setColour(juce::Colours::grey);
    }
    g.setFont(14.0f);
    g.drawText("Following", timelineTab, juce::Justification::centred);

    // Global tab
    auto globalTab = getGlobalTabBounds();
    bool isGlobalActive = (currentFeedType == FeedDataManager::FeedType::Global);

    if (isGlobalActive)
    {
        g.setColour(juce::Colour::fromRGB(0, 212, 255));
        g.fillRoundedRectangle(globalTab.reduced(5).toFloat(), 4.0f);
        g.setColour(juce::Colours::white);
    }
    else
    {
        g.setColour(juce::Colours::grey);
    }
    g.drawText("Discover", globalTab, juce::Justification::centred);

    // Refresh button
    auto refreshBtn = getRefreshButtonBounds();
    g.setColour(feedDataManager.isFetching() ? juce::Colours::grey : juce::Colours::lightgrey);
    g.setFont(18.0f);
    g.drawText("Refresh", refreshBtn, juce::Justification::centred);

    // Bottom border
    g.setColour(juce::Colour::fromRGB(50, 50, 50));
    g.drawLine(0.0f, static_cast<float>(tabsBounds.getBottom()), static_cast<float>(getWidth()), static_cast<float>(tabsBounds.getBottom()), 1.0f);
}

void PostsFeedComponent::drawLoadingState(juce::Graphics& g)
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(300, 150);

    // Loading spinner placeholder (animated dots)
    g.setColour(juce::Colour::fromRGB(0, 212, 255));
    g.setFont(32.0f);
    g.drawText("...", centerBounds.withHeight(50), juce::Justification::centred);

    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("Loading feed...", centerBounds.withY(centerBounds.getY() + 60).withHeight(30), juce::Justification::centred);

    g.setColour(juce::Colours::grey);
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
    g.setColour(juce::Colour::fromRGB(100, 100, 100));
    g.setFont(48.0f);
    g.drawText("~", centerBounds.withHeight(80), juce::Justification::centred);

    // Main message
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText(title, centerBounds.withY(centerBounds.getY() + 100).withHeight(40), juce::Justification::centred);

    // Subtitle
    g.setColour(juce::Colours::lightgrey);
    g.setFont(16.0f);
    g.drawText(subtitle1, centerBounds.withY(centerBounds.getY() + 150).withHeight(30), juce::Justification::centred);
    g.drawText(subtitle2, centerBounds.withY(centerBounds.getY() + 180).withHeight(30), juce::Justification::centred);

    // Action button
    auto actionBtn = getRecordButtonBounds();
    g.setColour(juce::Colour::fromRGB(0, 212, 255));
    g.fillRoundedRectangle(actionBtn.toFloat(), 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Start Recording", actionBtn, juce::Justification::centred);
}

void PostsFeedComponent::drawErrorState(juce::Graphics& g)
{
    auto contentBounds = getFeedContentBounds();
    auto centerBounds = contentBounds.withSizeKeepingCentre(400, 250);

    // Error icon
    g.setColour(juce::Colour::fromRGB(255, 100, 100));
    g.setFont(48.0f);
    g.drawText("!", centerBounds.withHeight(80), juce::Justification::centred);

    // Error message
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("Couldn't Load Feed", centerBounds.withY(centerBounds.getY() + 90).withHeight(35), juce::Justification::centred);

    // Error details
    g.setColour(juce::Colours::lightgrey);
    g.setFont(14.0f);
    juce::String displayError = errorMessage.isEmpty() ? "Network error. Please check your connection." : errorMessage;
    g.drawFittedText(displayError, centerBounds.withY(centerBounds.getY() + 130).withHeight(40), juce::Justification::centred, 2);

    // Retry button
    auto retryBtn = getRetryButtonBounds();
    g.setColour(juce::Colour::fromRGB(0, 212, 255));
    g.fillRoundedRectangle(retryBtn.toFloat(), 8.0f);
    g.setColour(juce::Colours::white);
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
            g.setColour(juce::Colours::grey);
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

    card->onLikeToggled = [this](const FeedPost& post, bool liked) {
        DBG("Like toggled for post: " + post.id + " -> " + (liked ? "liked" : "unliked"));
        // Optimistic UI update - card handles its own state
        // TODO: Call backend API
    };

    card->onUserClicked = [this](const FeedPost& post) {
        DBG("User clicked: " + post.username);
        // TODO: Navigate to user profile
    };

    card->onCommentClicked = [this](const FeedPost& post) {
        DBG("Comments clicked for post: " + post.id);
        // TODO: Open comments panel
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

void PostsFeedComponent::drawCircularProfilePic(juce::Graphics& g, juce::Rectangle<int> bounds, bool small)
{
    // Create circular clipping path
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());

    juce::Graphics::ScopedSaveState saveState(g);
    g.reduceClipRegion(circlePath);

    if (profilePicUrl.isEmpty())
    {
        // Default profile picture placeholder
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.fillEllipse(bounds.toFloat());

        // Add initials
        g.setColour(juce::Colour::fromRGB(120, 120, 120));
        g.setFont(small ? 14.0f : 28.0f);
        juce::String initials = username.substring(0, 2).toUpperCase();
        g.drawText(initials, bounds, juce::Justification::centred);
    }
    else
    {
        // Load and draw actual profile picture
        juce::File imageFile(profilePicUrl);
        if (imageFile.existsAsFile())
        {
            juce::Image profileImage = juce::ImageFileFormat::loadFrom(imageFile);
            if (profileImage.isValid())
            {
                auto scaledImage = profileImage.rescaled(bounds.getWidth(), bounds.getHeight(), juce::Graphics::highResamplingQuality);
                g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
            }
            else
            {
                g.setColour(juce::Colour::fromRGB(0, 150, 255));
                g.fillEllipse(bounds.toFloat());
                g.setColour(juce::Colours::white);
                g.setFont(small ? 14.0f : 28.0f);
                g.drawText(username.substring(0, 1).toUpperCase(), bounds, juce::Justification::centred);
            }
        }
        else
        {
            g.setColour(juce::Colour::fromRGB(0, 150, 255));
            g.fillEllipse(bounds.toFloat());
            g.setColour(juce::Colours::white);
            g.setFont(small ? 14.0f : 28.0f);
            g.drawText(username.substring(0, 1).toUpperCase(), bounds, juce::Justification::centred);
        }
    }

    // Border
    g.setColour(juce::Colour::fromRGB(200, 200, 200));
    g.drawEllipse(bounds.toFloat(), small ? 1.0f : 2.0f);
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

    // Check feed tabs
    if (getTimelineTabBounds().contains(pos))
    {
        switchFeedType(FeedDataManager::FeedType::Timeline);
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

    // Check profile area in top bar
    auto topBarBounds = getLocalBounds().withHeight(TOP_BAR_HEIGHT);
    auto profileBounds = topBarBounds.withX(getWidth() - 200).withWidth(180);

    if (profileBounds.contains(pos))
    {
        if (onGoToProfile)
            onGoToProfile();
        return;
    }
}

//==============================================================================
juce::Rectangle<int> PostsFeedComponent::getTimelineTabBounds() const
{
    return juce::Rectangle<int>(20, TOP_BAR_HEIGHT + 10, 100, 30);
}

juce::Rectangle<int> PostsFeedComponent::getGlobalTabBounds() const
{
    return juce::Rectangle<int>(130, TOP_BAR_HEIGHT + 10, 100, 30);
}

juce::Rectangle<int> PostsFeedComponent::getRefreshButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 100, TOP_BAR_HEIGHT + 10, 80, 30);
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
    return getLocalBounds().withTrimmedTop(TOP_BAR_HEIGHT + FEED_TABS_HEIGHT);
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

    return false;
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
