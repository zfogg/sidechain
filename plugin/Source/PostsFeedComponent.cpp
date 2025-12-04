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
}

PostsFeedComponent::~PostsFeedComponent()
{
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

    updateScrollBounds();
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
    auto contentBounds = getFeedContentBounds();

    // Clip to content area
    g.saveState();
    g.reduceClipRegion(contentBounds);

    int yOffset = contentBounds.getY() - static_cast<int>(scrollPosition);
    int cardWidth = contentBounds.getWidth() - 40; // Padding

    for (int i = 0; i < posts.size(); ++i)
    {
        auto cardBounds = juce::Rectangle<int>(
            contentBounds.getX() + 20,
            yOffset + i * (POST_CARD_HEIGHT + POST_CARD_SPACING),
            cardWidth,
            POST_CARD_HEIGHT
        );

        // Only draw visible cards
        if (cardBounds.getBottom() > contentBounds.getY() && cardBounds.getY() < contentBounds.getBottom())
        {
            drawPostCard(g, posts[i], cardBounds);
        }
    }

    g.restoreState();

    // Loading more indicator at bottom
    if (feedDataManager.isFetching() && feedDataManager.hasMorePosts())
    {
        auto loadingBounds = contentBounds.removeFromBottom(40);
        g.setColour(juce::Colours::grey);
        g.setFont(14.0f);
        g.drawText("Loading more...", loadingBounds, juce::Justification::centred);
    }
}

void PostsFeedComponent::drawPostCard(juce::Graphics& g, const FeedPost& post, juce::Rectangle<int> bounds)
{
    // Card background
    g.setColour(juce::Colour::fromRGB(40, 40, 40));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    // Card border
    g.setColour(juce::Colour::fromRGB(60, 60, 60));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

    // Left section: avatar + username
    auto avatarBounds = juce::Rectangle<int>(bounds.getX() + 15, bounds.getCentreY() - 25, 50, 50);

    // Avatar placeholder
    g.setColour(juce::Colour::fromRGB(70, 70, 70));
    g.fillEllipse(avatarBounds.toFloat());

    // User initial
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    juce::String initial = post.username.isEmpty() ? "?" : post.username.substring(0, 1).toUpperCase();
    g.drawText(initial, avatarBounds, juce::Justification::centred);

    // Username and time
    int textX = avatarBounds.getRight() + 15;
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(post.username.isEmpty() ? "Unknown" : post.username,
               textX, bounds.getY() + 15, 150, 20, juce::Justification::centredLeft);

    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText(post.timeAgo, textX, bounds.getY() + 35, 150, 18, juce::Justification::centredLeft);

    // Center section: waveform placeholder
    auto waveformBounds = juce::Rectangle<int>(textX + 160, bounds.getY() + 20, bounds.getWidth() - textX - 280, bounds.getHeight() - 40);

    g.setColour(juce::Colour::fromRGB(50, 50, 50));
    g.fillRoundedRectangle(waveformBounds.toFloat(), 4.0f);

    // Waveform bars placeholder
    g.setColour(juce::Colour::fromRGB(0, 180, 220));
    int barWidth = 3;
    int barSpacing = 2;
    int numBars = waveformBounds.getWidth() / (barWidth + barSpacing);

    for (int i = 0; i < numBars; ++i)
    {
        int barHeight = 5 + (std::hash<int>{}(post.id.hashCode() + i) % 25);
        int barX = waveformBounds.getX() + i * (barWidth + barSpacing);
        int barY = waveformBounds.getCentreY() - barHeight / 2;
        g.fillRect(barX, barY, barWidth, barHeight);
    }

    // Right section: metadata badges
    int badgeX = bounds.getRight() - 110;
    int badgeY = bounds.getY() + 20;

    // BPM badge
    if (post.bpm > 0)
    {
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.fillRoundedRectangle(static_cast<float>(badgeX), static_cast<float>(badgeY), 50.0f, 22.0f, 4.0f);
        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        g.drawText(juce::String(post.bpm) + " BPM", badgeX, badgeY, 50, 22, juce::Justification::centred);
        badgeY += 28;
    }

    // Key badge
    if (post.key.isNotEmpty())
    {
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.fillRoundedRectangle(static_cast<float>(badgeX), static_cast<float>(badgeY), 50.0f, 22.0f, 4.0f);
        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        g.drawText(post.key, badgeX, badgeY, 50, 22, juce::Justification::centred);
        badgeY += 28;
    }

    // Like count
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText(juce::String(post.likeCount) + " likes", badgeX - 10, bounds.getBottom() - 30, 70, 20, juce::Justification::centredLeft);
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
                posts.addArray(response.posts);
                updateScrollBounds();
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
