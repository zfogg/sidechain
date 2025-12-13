#pragma once

#include <JuceHeader.h>
#include "StoriesFeed.h"
#include "PianoRoll.h"
#include "../../audio/HttpAudioPlayer.h"

class NetworkClient;

//==============================================================================
/**
 * StoryViewer displays stories in a full-screen viewer (7.5.4.2.1)
 *
 * Features:
 * - Full-screen story view (Snapchat-style)
 * - Audio playback with waveform
 * - MIDI visualization (piano roll)
 * - User info header (avatar, name)
 * - Progress bar showing story duration
 * - Swipe/tap navigation between stories
 */
class StoryViewer : public juce::Component,
                              public juce::Timer
{
public:
    StoryViewer();
    ~StoryViewer() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    //==============================================================================
    // Timer callback for playback progress
    void timerCallback() override;

    //==============================================================================
    // Network client for marking stories as viewed
    void setNetworkClient(NetworkClient* client) { networkClient = client; }
    void setCurrentUserId(const juce::String& userId) { currentUserId = userId; }

    // Set story data to display
    void setStories(const std::vector<StoryData>& stories, int startIndex = 0);

    // Navigation
    void showNextStory();
    void showPreviousStory();
    void closeViewer();

    // Playback control
    void togglePlayPause();
    bool isPlaying() const { return playing; }

    //==============================================================================
    // Callbacks

    // Called when viewer should close
    std::function<void()> onClose;

    // Called when navigating to next user's stories
    std::function<void(const juce::String& nextUserId)> onNextUser;

    // Called when user wants to see viewers list
    std::function<void(const juce::String& storyId)> onViewersClicked;

    // Called when user wants to share story
    std::function<void(const juce::String& storyId)> onShareClicked;

    // Called when user wants to download MIDI from story
    std::function<void(const StoryData& story)> onDownloadMIDIClicked;

    // Called when user wants to remix the story (R.3.2 Remix Chains)
    // Parameters: storyId, remixType ("audio", "midi", or "both")
    std::function<void(const juce::String& storyId, const juce::String& remixType)> onRemixClicked;

    // Called when user wants to delete their own story
    std::function<void(const juce::String& storyId)> onDeleteClicked;

private:
    //==============================================================================
    NetworkClient* networkClient = nullptr;
    juce::String currentUserId;

    // Stories data
    std::vector<StoryData> stories;
    int currentStoryIndex = 0;

    // Audio player
    std::unique_ptr<HttpAudioPlayer> audioPlayer;
    bool playing = false;
    double playbackPosition = 0.0;
    double storyDuration = 0.0;

    // Piano roll component for MIDI visualization
    std::unique_ptr<PianoRoll> pianoRoll;

    // Waveform data
    juce::Path currentWaveformPath;
    bool waveformLoaded = false;

    // Swipe detection
    juce::Point<int> dragStartPoint;
    bool isDragging = false;
    static constexpr int SWIPE_THRESHOLD = 50;

    // Progress bar segments (for multiple stories from same user)
    struct ProgressSegment
    {
        float progress = 0.0f;
        bool completed = false;
    };
    std::vector<ProgressSegment> progressSegments;

    // UI areas
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> progressBarArea;
    juce::Rectangle<int> contentArea;
    juce::Rectangle<int> closeButtonArea;
    juce::Rectangle<int> leftTapArea;
    juce::Rectangle<int> rightTapArea;
    juce::Rectangle<int> viewersButtonArea;
    juce::Rectangle<int> shareButtonArea;
    juce::Rectangle<int> deleteButtonArea;  // Delete button for own stories
    juce::Rectangle<int> midiButtonArea;
    juce::Rectangle<int> remixButtonArea;  // R.3.2 Remix Chains

    //==============================================================================
    // Drawing helpers
    void drawHeader(juce::Graphics& g);
    void drawProgressBar(juce::Graphics& g);
    void drawStoryContent(juce::Graphics& g);
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawViewCount(juce::Graphics& g);
    void drawExpirationTime(juce::Graphics& g);
    void drawPlayPauseOverlay(juce::Graphics& g);
    void drawExpiredMessage(juce::Graphics& g);
    void drawViewersButton(juce::Graphics& g);
    void drawShareButton(juce::Graphics& g);
    void drawDeleteButton(juce::Graphics& g);  // Delete button for own stories
    void drawMIDIButton(juce::Graphics& g);
    void drawRemixButton(juce::Graphics& g);  // R.3.2 Remix Chains

    // Story management
    void loadCurrentStory();
    void markStoryAsViewed();
    void updateProgress();
    void onStoryComplete();

    // Get current story data
    const StoryData* getCurrentStory() const;

    // Share story (copy link to clipboard)
    void handleShareStory(const juce::String& storyId);

    // Download MIDI from story (R.3.3.5.5)
    void handleDownloadMIDI(const StoryData& story);

    // Delete story (for story owner)
    void handleDeleteStory(const juce::String& storyId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StoryViewer)
};
