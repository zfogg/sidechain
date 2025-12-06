#pragma once

#include <JuceHeader.h>
#include "../../network/NetworkClient.h"
#include "../../audio/KeyDetector.h"

// Forward declarations
class SidechainAudioProcessor;

//==============================================================================
/**
 * Upload provides the UI for sharing a recorded loop.
 *
 * Features:
 * - Title input (required)
 * - BPM display (auto-detected from DAW via AudioPlayHead)
 * - Musical key dropdown (24 keys + "Not set")
 * - Genre dropdown
 * - Waveform preview of the recording
 * - Upload progress indicator
 * - Cancel/Share buttons
 *
 * Design: Dark theme matching the plugin aesthetic, producer-friendly
 */
class Upload : public juce::Component,
                        public juce::Timer
{
public:
    //==========================================================================
    // Musical key options
    static constexpr int NUM_KEYS = 25; // 24 keys + "Not set"

    struct MusicalKey
    {
        juce::String name;
        juce::String shortName;
    };

    static const std::array<MusicalKey, NUM_KEYS>& getMusicalKeys();

    //==========================================================================
    // Genre options
    static constexpr int NUM_GENRES = 12;
    static const std::array<juce::String, NUM_GENRES>& getGenres();

    //==========================================================================
    Upload(SidechainAudioProcessor& processor, NetworkClient& network);
    ~Upload() override;

    //==========================================================================
    // Set the audio to upload (called when user confirms recording)
    void setAudioToUpload(const juce::AudioBuffer<float>& audio, double sampleRate);

    // Clear state and prepare for new upload
    void reset();

    //==========================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType cause) override;

    //==========================================================================
    // Callbacks
    std::function<void()> onUploadComplete;  // Called after successful upload
    std::function<void()> onCancel;          // Called when user cancels

private:
    //==========================================================================
    SidechainAudioProcessor& audioProcessor;
    NetworkClient& networkClient;

    // Audio data to upload
    juce::AudioBuffer<float> audioBuffer;
    double audioSampleRate = 44100.0;

    // Upload state
    enum class UploadState
    {
        Editing,    // User is filling in metadata
        Uploading,  // Upload in progress
        Success,    // Upload completed
        Error       // Upload failed
    };
    UploadState uploadState = UploadState::Editing;
    float uploadProgress = 0.0f;
    juce::String errorMessage;
    
    // Timer IDs for progress simulation (to allow cancellation)
    int progressTimer500ms = 0;
    int progressTimer1000ms = 0;
    int successDismissTimer = 0;

    // Form data
    juce::String title;
    double bpm = 0.0;
    bool bpmFromDAW = false;
    int selectedKeyIndex = 0;      // 0 = "Not set"
    int selectedGenreIndex = 0;    // 0 = first genre

    // UI state
    int activeField = -1;  // -1 = none, 0 = title, 1 = bpm
    int hoveredButton = -1;

    // Tap tempo state
    std::vector<double> tapTimes;
    double lastTapTime = 0.0;

    // Last uploaded post info (for success preview)
    juce::String lastUploadedTitle;
    juce::String lastUploadedGenre;
    double lastUploadedBpm = 0.0;
    juce::String lastUploadedUrl;

    // UI areas (calculated in resized())
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> waveformArea;
    juce::Rectangle<int> titleFieldArea;
    juce::Rectangle<int> bpmFieldArea;
    juce::Rectangle<int> tapTempoButtonArea;
    juce::Rectangle<int> keyDropdownArea;
    juce::Rectangle<int> detectKeyButtonArea;
    juce::Rectangle<int> genreDropdownArea;
    juce::Rectangle<int> progressBarArea;
    juce::Rectangle<int> cancelButtonArea;
    juce::Rectangle<int> shareButtonArea;
    juce::Rectangle<int> statusArea;

    //==========================================================================
    // Drawing helpers
    void drawHeader(juce::Graphics& g);
    void drawWaveform(juce::Graphics& g);
    void drawTitleField(juce::Graphics& g);
    void drawBPMField(juce::Graphics& g);
    void drawTapTempoButton(juce::Graphics& g);
    void drawKeyDropdown(juce::Graphics& g);
    void drawDetectKeyButton(juce::Graphics& g);
    void drawGenreDropdown(juce::Graphics& g);
    void drawProgressBar(juce::Graphics& g);
    void drawButtons(juce::Graphics& g);
    void drawStatus(juce::Graphics& g);

    void drawTextField(juce::Graphics& g, juce::Rectangle<int> bounds,
                      const juce::String& label, const juce::String& value,
                      bool isActive, bool isEditable = true);
    void drawDropdown(juce::Graphics& g, juce::Rectangle<int> bounds,
                     const juce::String& label, const juce::String& value,
                     bool isHovered);
    void drawButton(juce::Graphics& g, juce::Rectangle<int> bounds,
                   const juce::String& text, juce::Colour bgColor,
                   bool isHovered, bool isEnabled = true);

    juce::Path generateWaveformPath(juce::Rectangle<int> bounds);
    juce::String formatDuration() const;

    //==========================================================================
    // Actions
    void handleTapTempo();
    void detectKey();
    void showKeyPicker();
    void showGenrePicker();
    void cancelUpload();
    void startUpload();

    // Key detection
    KeyDetector keyDetector;
    bool isDetectingKey = false;
    juce::String keyDetectionStatus;

    // Keyboard input handling
    void handleKeyPress(const juce::KeyPress& key);
    void handleTextInput(juce::juce_wchar character);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Upload)
};
