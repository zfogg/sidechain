#pragma once

#include <JuceHeader.h>

// Forward declaration
class SidechainAudioProcessor;

//==============================================================================
/**
 * SocialFeed component displays the social feed of loops from other producers
 * 
 * Features:
 * - Scrollable feed of audio posts
 * - Audio preview playback
 * - Emoji reactions (🎵❤️🔥😍🚀💯)
 * - Like/follow buttons
 * - Real-time updates
 */
class SocialFeed : public juce::Component,
                   public juce::Timer,
                   public juce::Button::Listener
{
public:
    SocialFeed(SidechainAudioProcessor& processor);
    ~SocialFeed() override;
    
    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;
    void timerCallback() override;
    
    //==============================================================================
    void refreshFeed();
    void loadMorePosts();
    
private:
    SidechainAudioProcessor& audioProcessor;
    
    // Feed data
    juce::Array<juce::var> feedPosts;
    int currentOffset = 0;
    bool loadingMore = false;
    
    // UI Components
    std::unique_ptr<juce::Viewport> feedViewport;
    std::unique_ptr<juce::Component> feedContainer;
    std::unique_ptr<juce::TextButton> refreshButton;
    std::unique_ptr<juce::Label> feedStatusLabel;
    
    // Individual post components (simplified for now)
    juce::OwnedArray<juce::Component> postComponents;
    
    void createPostComponent(const juce::var& postData, int index);
    void updateFeedDisplay();
    void handleFeedResponse(const juce::var& response);
    
    static constexpr int POST_HEIGHT = 120;
    static constexpr int FEED_REFRESH_INTERVAL_MS = 30000; // 30 seconds
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SocialFeed)
};

//==============================================================================
/**
 * Individual post component within the social feed
 */
class PostComponent : public juce::Component,
                      public juce::Button::Listener
{
public:
    PostComponent(const juce::var& postData, SidechainAudioProcessor& processor);
    ~PostComponent() override;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;
    
private:
    juce::var data;
    SidechainAudioProcessor& audioProcessor;
    
    // UI components for each post
    std::unique_ptr<juce::Label> usernameLabel;
    std::unique_ptr<juce::Label> timestampLabel;
    std::unique_ptr<juce::Label> metadataLabel; // BPM, key, etc.
    std::unique_ptr<juce::TextButton> playButton;
    std::unique_ptr<juce::TextButton> likeButton;
    
    // Emoji reaction buttons
    std::unique_ptr<juce::TextButton> fireButton;    // 🔥
    std::unique_ptr<juce::TextButton> musicButton;   // 🎵
    std::unique_ptr<juce::TextButton> heartButton;   // ❤️
    std::unique_ptr<juce::TextButton> wowButton;     // 😍
    std::unique_ptr<juce::TextButton> hypeButton;    // 🚀
    std::unique_ptr<juce::TextButton> perfectButton; // 💯
    
    // Waveform display
    std::unique_ptr<juce::Component> waveformComponent;
    
    bool isPlaying = false;
    
    void setupEmojiButtons();
    void reactWithEmoji(const juce::String& emoji);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PostComponent)
};