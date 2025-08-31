#include "SocialFeed.h"
#include "PluginProcessor.h"

//==============================================================================
SocialFeed::SocialFeed(SidechainAudioProcessor& processor)
    : audioProcessor(processor)
{
    // Create feed viewport for scrolling
    feedViewport = std::make_unique<juce::Viewport>();
    feedContainer = std::make_unique<juce::Component>();
    feedViewport->setViewedComponent(feedContainer.get(), false);
    addAndMakeVisible(feedViewport.get());
    
    // Refresh button
    refreshButton = std::make_unique<juce::TextButton>("🔄 Refresh Feed");
    refreshButton->addListener(this);
    addAndMakeVisible(refreshButton.get());
    
    // Status label
    feedStatusLabel = std::make_unique<juce::Label>("feedStatus", "Loading feed...");
    feedStatusLabel->setJustificationType(juce::Justification::centred);
    feedStatusLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(feedStatusLabel.get());
    
    // Start timer for periodic feed updates
    startTimer(FEED_REFRESH_INTERVAL_MS);
    
    // Load initial feed if authenticated
    if (audioProcessor.isAuthenticated())
    {
        refreshFeed();
    }
}

SocialFeed::~SocialFeed()
{
    stopTimer();
}

//==============================================================================
void SocialFeed::paint(juce::Graphics& g)
{
    // Dark background for feed
    g.fillAll(juce::Colour::fromRGB(32, 32, 36));
    
    // Border
    g.setColour(juce::Colour::fromRGB(64, 64, 68));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 4.0f, 1.0f);
    
    // If not authenticated, show placeholder
    if (!audioProcessor.isAuthenticated())
    {
        g.setColour(juce::Colours::lightgrey);
        g.setFont(16.0f);
        g.drawText("Connect your account to see the social feed", 
                  getLocalBounds(), juce::Justification::centred);
    }
}

void SocialFeed::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(8, 8);
    
    // Top controls
    auto controlsArea = bounds.removeFromTop(40);
    refreshButton->setBounds(controlsArea.removeFromLeft(120));
    controlsArea.removeFromLeft(10);
    feedStatusLabel->setBounds(controlsArea);
    
    bounds.removeFromTop(10);
    
    // Feed viewport takes remaining space
    feedViewport->setBounds(bounds);
    
    // Update container size based on number of posts
    int containerHeight = feedPosts.size() * (POST_HEIGHT + 10);
    feedContainer->setSize(bounds.getWidth() - 20, juce::jmax(containerHeight, bounds.getHeight()));
}

void SocialFeed::buttonClicked(juce::Button* button)
{
    if (button == refreshButton.get())
    {
        refreshFeed();
    }
}

void SocialFeed::timerCallback()
{
    // Auto-refresh feed if authenticated
    if (audioProcessor.isAuthenticated() && !loadingMore)
    {
        refreshFeed();
    }
}

//==============================================================================
void SocialFeed::refreshFeed()
{
    if (!audioProcessor.isAuthenticated())
    {
        feedStatusLabel->setText("Not authenticated", juce::dontSendNotification);
        return;
    }
    
    feedStatusLabel->setText("Refreshing feed...", juce::dontSendNotification);
    refreshButton->setEnabled(false);
    
    // Get global feed from backend
    audioProcessor.getNetworkClient().getGlobalFeed(20, 0, [this](const juce::var& response) {
        handleFeedResponse(response);
        
        refreshButton->setEnabled(true);
        feedStatusLabel->setText("Last updated: " + juce::Time::getCurrentTime().toString(false, true), juce::dontSendNotification);
    });
}

void SocialFeed::loadMorePosts()
{
    if (loadingMore || !audioProcessor.isAuthenticated())
        return;
        
    loadingMore = true;
    feedStatusLabel->setText("Loading more posts...", juce::dontSendNotification);
    
    audioProcessor.getNetworkClient().getGlobalFeed(20, feedPosts.size(), [this](const juce::var& response) {
        handleFeedResponse(response);
        loadingMore = false;
    });
}

void SocialFeed::handleFeedResponse(const juce::var& response)
{
    if (!response.isObject())
        return;
        
    auto activities = response.getProperty("activities", juce::var());
    
    if (activities.isArray())
    {
        // Clear old posts for refresh, or append for load more
        if (currentOffset == 0)
        {
            feedPosts.clear();
            postComponents.clear();
        }
        
        // Add new posts
        auto* activitiesArray = activities.getArray();
        for (int i = 0; i < activitiesArray->size(); ++i)
        {
            feedPosts.add(activitiesArray->getReference(i));
        }
        
        updateFeedDisplay();
        currentOffset = feedPosts.size();
    }
}

void SocialFeed::updateFeedDisplay()
{
    // Clear existing post components
    postComponents.clear();
    
    // Create components for each post
    for (int i = 0; i < feedPosts.size(); ++i)
    {
        createPostComponent(feedPosts[i], i);
    }
    
    resized(); // Update layout
}

void SocialFeed::createPostComponent(const juce::var& postData, int index)
{
    auto* postComp = new PostComponent(postData, audioProcessor);
    postComponents.add(postComp);
    feedContainer->addAndMakeVisible(postComp);
    
    // Position the post component
    int yPos = index * (POST_HEIGHT + 10);
    postComp->setBounds(10, yPos, feedContainer->getWidth() - 20, POST_HEIGHT);
}

//==============================================================================
// PostComponent implementation
//==============================================================================

PostComponent::PostComponent(const juce::var& postData, SidechainAudioProcessor& processor)
    : data(postData), audioProcessor(processor)
{
    // Username and timestamp
    auto username = data.getProperty("actor", "Unknown Producer").toString();
    usernameLabel = std::make_unique<juce::Label>("username", username);
    usernameLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    usernameLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(usernameLabel.get());
    
    timestampLabel = std::make_unique<juce::Label>("timestamp", "2 hours ago");
    timestampLabel->setFont(juce::Font(11.0f));
    timestampLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(timestampLabel.get());
    
    // Metadata (BPM, key, etc.)
    auto bpm = data.getProperty("bpm", 120);
    auto key = data.getProperty("key", "C major").toString();
    juce::String metadata = juce::String(bpm) + " BPM • " + key;
    
    metadataLabel = std::make_unique<juce::Label>("metadata", metadata);
    metadataLabel->setFont(juce::Font(11.0f));
    metadataLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(metadataLabel.get());
    
    // Play button
    playButton = std::make_unique<juce::TextButton>("▶️");
    playButton->addListener(this);
    addAndMakeVisible(playButton.get());
    
    // Like button
    likeButton = std::make_unique<juce::TextButton>("🤍");
    likeButton->addListener(this);
    addAndMakeVisible(likeButton.get());
    
    setupEmojiButtons();
}

PostComponent::~PostComponent()
{
}

void PostComponent::paint(juce::Graphics& g)
{
    // Post background
    g.fillAll(juce::Colour::fromRGB(40, 40, 44));
    
    // Border
    g.setColour(juce::Colour::fromRGB(60, 60, 64));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 6.0f, 1.0f);
    
    // Waveform area (simplified visualization)
    auto waveformArea = juce::Rectangle<int>(120, 40, 200, 40);
    g.setColour(juce::Colour::fromRGB(0, 212, 255));
    
    // Draw simple waveform bars
    for (int i = 0; i < 20; ++i)
    {
        int barHeight = 5 + (i % 4) * 8; // Simple pattern
        int x = waveformArea.getX() + i * 10;
        int y = waveformArea.getCentreY() - barHeight / 2;
        g.fillRect(x, y, 2, barHeight);
    }
}

void PostComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(12, 8);
    
    // Top row: username and timestamp
    auto topRow = bounds.removeFromTop(20);
    usernameLabel->setBounds(topRow.removeFromLeft(200));
    timestampLabel->setBounds(topRow);
    
    bounds.removeFromTop(5);
    
    // Middle row: play button, waveform, metadata
    auto middleRow = bounds.removeFromTop(40);
    playButton->setBounds(middleRow.removeFromLeft(60));
    middleRow.removeFromLeft(10);
    
    // Waveform space (rendered in paint)
    middleRow.removeFromLeft(200);
    middleRow.removeFromLeft(10);
    
    metadataLabel->setBounds(middleRow);
    
    bounds.removeFromTop(8);
    
    // Bottom row: emoji reactions
    auto bottomRow = bounds.removeFromTop(30);
    
    likeButton->setBounds(bottomRow.removeFromLeft(40));
    bottomRow.removeFromLeft(10);
    
    // Emoji buttons
    fireButton->setBounds(bottomRow.removeFromLeft(35));
    musicButton->setBounds(bottomRow.removeFromLeft(35));
    heartButton->setBounds(bottomRow.removeFromLeft(35));
    wowButton->setBounds(bottomRow.removeFromLeft(35));
    hypeButton->setBounds(bottomRow.removeFromLeft(35));
    perfectButton->setBounds(bottomRow.removeFromLeft(35));
}

void PostComponent::buttonClicked(juce::Button* button)
{
    auto activityId = data.getProperty("id", "").toString();
    
    if (button == playButton.get())
    {
        // Toggle play/pause
        isPlaying = !isPlaying;
        playButton->setButtonText(isPlaying ? "⏸️" : "▶️");
        
        if (isPlaying)
        {
            // TODO: Start audio preview playback
            DBG("Playing audio preview for: " + activityId);
        }
        else
        {
            // TODO: Stop audio preview
            DBG("Stopped audio preview");
        }
    }
    else if (button == likeButton.get())
    {
        likeButton->setButtonText("❤️");
        audioProcessor.getNetworkClient().likePost(activityId);
    }
    else if (button == fireButton.get())
    {
        reactWithEmoji("🔥");
    }
    else if (button == musicButton.get())
    {
        reactWithEmoji("🎵");
    }
    else if (button == heartButton.get())
    {
        reactWithEmoji("❤️");
    }
    else if (button == wowButton.get())
    {
        reactWithEmoji("😍");
    }
    else if (button == hypeButton.get())
    {
        reactWithEmoji("🚀");
    }
    else if (button == perfectButton.get())
    {
        reactWithEmoji("💯");
    }
}

void PostComponent::setupEmojiButtons()
{
    // Create emoji reaction buttons
    fireButton = std::make_unique<juce::TextButton>("🔥");
    fireButton->addListener(this);
    addAndMakeVisible(fireButton.get());
    
    musicButton = std::make_unique<juce::TextButton>("🎵");
    musicButton->addListener(this);
    addAndMakeVisible(musicButton.get());
    
    heartButton = std::make_unique<juce::TextButton>("❤️");
    heartButton->addListener(this);
    addAndMakeVisible(heartButton.get());
    
    wowButton = std::make_unique<juce::TextButton>("😍");
    wowButton->addListener(this);
    addAndMakeVisible(wowButton.get());
    
    hypeButton = std::make_unique<juce::TextButton>("🚀");
    hypeButton->addListener(this);
    addAndMakeVisible(hypeButton.get());
    
    perfectButton = std::make_unique<juce::TextButton>("💯");
    perfectButton->addListener(this);
    addAndMakeVisible(perfectButton.get());
    
    // Style emoji buttons
    auto buttons = {fireButton.get(), musicButton.get(), heartButton.get(), 
                   wowButton.get(), hypeButton.get(), perfectButton.get()};
    
    for (auto* btn : buttons)
    {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }
}

void PostComponent::reactWithEmoji(const juce::String& emoji)
{
    auto activityId = data.getProperty("id", "").toString();
    
    if (!activityId.isEmpty())
    {
        audioProcessor.getNetworkClient().likePost(activityId, emoji);
        DBG("Reacted with " + emoji + " to post: " + activityId);
    }
}