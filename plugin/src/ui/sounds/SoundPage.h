#pragma once

#include <JuceHeader.h>
#include "../../models/Sound.h"
#include "../../models/FeedPost.h"

class NetworkClient;

//==============================================================================
/**
 * SoundPage displays a sound's details and all posts using it (Feature #15)
 *
 * Features:
 * - Sound name and creator info
 * - Usage count ("X posts with this sound")
 * - Duration display
 * - Scrollable list of posts using this sound
 * - Play posts directly from the list
 * - Navigate to post or user profile
 */
class SoundPage : public juce::Component,
                  public juce::ScrollBar::Listener
{
public:
    SoundPage();
    ~SoundPage() override;

    //==========================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    //==========================================================================
    // Network client integration
    void setNetworkClient(NetworkClient* client);
    void setCurrentUserId(const juce::String& userId) { currentUserId = userId; }

    // Load sound by ID
    void loadSound(const juce::String& soundId);

    // Load sound for a specific post
    void loadSoundForPost(const juce::String& postId);

    // Refresh data
    void refresh();

    //==========================================================================
    // Callbacks
    std::function<void()> onBackPressed;
    std::function<void(const juce::String& postId)> onPostSelected;
    std::function<void(const juce::String& userId)> onUserSelected;
    std::function<void(const FeedPost& post)> onPlayPost;
    std::function<void(const FeedPost& post)> onPausePost;

    //==========================================================================
    // Playback state (for highlighting currently playing post)
    void setCurrentlyPlayingPost(const juce::String& postId);
    void setPlaybackProgress(float progress);
    void clearPlayingState();

private:
    //==========================================================================
    // Data
    NetworkClient* networkClient = nullptr;
    juce::String currentUserId;
    juce::String soundId;
    Sound sound;
    juce::Array<SoundPost> posts;
    bool isLoading = false;
    juce::String errorMessage;

    // Playback state
    juce::String currentlyPlayingPostId;
    float playbackProgress = 0.0f;

    //==========================================================================
    // UI Components
    juce::ScrollBar scrollBar { true };  // vertical

    // Scroll state
    int scrollOffset = 0;

    // Cached creator avatar
    juce::Image creatorAvatar;

    //==========================================================================
    // Layout constants
    static constexpr int HEADER_HEIGHT = 60;
    static constexpr int SOUND_INFO_HEIGHT = 140;
    static constexpr int POST_CARD_HEIGHT = 80;
    static constexpr int PADDING = 16;

    //==========================================================================
    // Colors
    struct Colors
    {
        static inline juce::Colour background { 0xff1a1a1e };
        static inline juce::Colour cardBg { 0xff252529 };
        static inline juce::Colour cardBgHover { 0xff2d2d32 };
        static inline juce::Colour textPrimary { 0xffffffff };
        static inline juce::Colour textSecondary { 0xffa0a0a0 };
        static inline juce::Colour accent { 0xff00d4ff };
        static inline juce::Colour accentHover { 0xff00b8e0 };
        static inline juce::Colour playButton { 0xff00d4ff };
        static inline juce::Colour soundIcon { 0xffff6b6b };  // Coral for sound icon
        static inline juce::Colour trendingBadge { 0xffffd93d };  // Gold for trending
    };

    //==========================================================================
    // Drawing methods
    void drawHeader(juce::Graphics& g);
    void drawSoundInfo(juce::Graphics& g, juce::Rectangle<int>& bounds);
    void drawPostCard(juce::Graphics& g, juce::Rectangle<int> bounds, const SoundPost& post, int index);
    void drawLoadingState(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawErrorState(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds);

    //==========================================================================
    // Hit testing helpers
    juce::Rectangle<int> getBackButtonBounds() const;
    juce::Rectangle<int> getCreatorBounds() const;
    juce::Rectangle<int> getContentBounds() const;
    juce::Rectangle<int> getPostCardBounds(int index) const;
    juce::Rectangle<int> getPostPlayButtonBounds(int index) const;
    juce::Rectangle<int> getPostUserBounds(int index) const;

    //==========================================================================
    // Network operations
    void fetchSound();
    void fetchSoundPosts();

    //==========================================================================
    // Helper methods
    int calculateContentHeight() const;
    void updateScrollBounds();
    void loadCreatorAvatar();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundPage)
};
