#pragma once

#include <JuceHeader.h>

// Forward declaration
class SidechainAudioProcessor;

//==============================================================================
/**
 * AudioSnippetRecorder provides UI for recording short audio snippets (max 30 seconds)
 * to send in messages.
 *
 * Features:
 * - Record button (toggle or hold-to-record)
 * - Max duration: 30 seconds
 * - Timer display during recording
 * - Waveform visualization while recording
 * - Cancel button to discard recording
 * - Preview and send functionality
 */
class AudioSnippetRecorder : public juce::Component,
                              public juce::Timer
{
public:
    AudioSnippetRecorder(SidechainAudioProcessor& processor);
    ~AudioSnippetRecorder() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // Timer callback for UI updates
    void timerCallback() override;

    //==============================================================================
    // Callback when recording is complete and ready to send
    std::function<void(const juce::AudioBuffer<float>&, double sampleRate)> onRecordingComplete;

    // Callback when user cancels recording
    std::function<void()> onRecordingCancelled;

    // Check if currently recording
    bool isRecording() const { return currentState == State::Recording; }

    // Get current recording duration
    double getRecordingDuration() const;

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

    // Recording start time
    juce::Time recordingStartTime;
    static constexpr double MAX_DURATION_SECONDS = 30.0;

    // Cached recording data for preview
    juce::AudioBuffer<float> recordedAudio;
    double recordedSampleRate = 44100.0;

    // UI areas
    juce::Rectangle<int> recordButtonArea;
    juce::Rectangle<int> timerArea;
    juce::Rectangle<int> waveformArea;
    juce::Rectangle<int> cancelButtonArea;
    juce::Rectangle<int> sendButtonArea;

    //==============================================================================
    // Drawing helpers
    void drawIdleState(juce::Graphics& g);
    void drawRecordingState(juce::Graphics& g);
    void drawPreviewState(juce::Graphics& g);
    void drawRecordButton(juce::Graphics& g, bool isRecording);
    void drawTimer(juce::Graphics& g);
    void drawWaveform(juce::Graphics& g);
    void drawCancelButton(juce::Graphics& g);
    void drawSendButton(juce::Graphics& g);

    // Generate waveform path from audio buffer
    juce::Path generateWaveformPath(const juce::AudioBuffer<float>& buffer,
                                    juce::Rectangle<int> bounds);

    // Helper to format time as MM:SS
    juce::String formatTime(double seconds);

    //==============================================================================
    // Button actions
    void startRecording();
    void stopRecording();
    void cancelRecording();
    void sendRecording();

    // Check if recording has reached max duration
    bool hasReachedMaxDuration() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSnippetRecorder)
};
