#pragma once

#include <JuceHeader.h>
#include "../../util/Animation.h"

// Forward declaration to avoid circular includes
class SidechainAudioProcessor;

//==============================================================================
/**
 * Recording provides the UI for audio recording from the DAW.
 *
 * Features:
 * - Record/Stop button with visual state
 * - Recording indicator (red dot animation)
 * - Time elapsed display (MM:SS)
 * - Level meters (stereo peak + RMS)
 * - Progress bar (0-60 seconds)
 * - Waveform preview after recording
 *
 * Uses juce::Timer to poll recording state from the processor
 * at ~30fps for smooth UI updates.
 */
class Recording : public juce::Component,
                           public juce::Timer
{
public:
    Recording(SidechainAudioProcessor& processor);
    ~Recording() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

    //==============================================================================
    // Timer callback for UI updates
    void timerCallback() override;

    //==============================================================================
    // Callback when recording is complete and ready for upload
    std::function<void(const juce::AudioBuffer<float>&)> onRecordingComplete;

    // Callback when user wants to discard recording
    std::function<void()> onRecordingDiscarded;

private:
    //==============================================================================
    SidechainAudioProcessor& audioProcessor;

    // Recording state
    enum class State
    {
        Idle,       // Ready to record
        Recording,  // Actively recording
        Preview     // Recording complete, showing preview
    };
    State currentState = State::Idle;

    // Cached recording data for preview
    juce::AudioBuffer<float> recordedAudio;
    double recordedSampleRate = 44100.0;

    // Animation state
    Animation recordingDotAnimation{2000, Animation::Easing::EaseInOut};  // 2 second ping-pong

    // UI areas (calculated in resized())
    juce::Rectangle<int> recordButtonArea;
    juce::Rectangle<int> timeDisplayArea;
    juce::Rectangle<int> levelMeterArea;
    juce::Rectangle<int> progressBarArea;
    juce::Rectangle<int> waveformArea;
    juce::Rectangle<int> actionButtonsArea;

    //==============================================================================
    // Drawing helpers
    void drawRecordButton(juce::Graphics& g);
    void drawTimeDisplay(juce::Graphics& g);
    void drawLevelMeters(juce::Graphics& g);
    void drawSingleMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                         float peak, float rms, const juce::String& label);
    void drawProgressBar(juce::Graphics& g);
    void drawWaveformPreview(juce::Graphics& g);
    void drawActionButtons(juce::Graphics& g);
    void drawIdleState(juce::Graphics& g);
    void drawRecordingState(juce::Graphics& g);
    void drawPreviewState(juce::Graphics& g);

    // Helper to format time as MM:SS
    juce::String formatTime(double seconds);

    // Generate waveform path from audio buffer
    juce::Path generateWaveformPath(const juce::AudioBuffer<float>& buffer,
                                    juce::Rectangle<int> bounds);

    //==============================================================================
    // Button actions
    void startRecording();
    void stopRecording();
    void discardRecording();
    void confirmRecording();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Recording)
};
