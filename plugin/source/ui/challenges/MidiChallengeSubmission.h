#pragma once

#include <JuceHeader.h>
#include "../../models/MidiChallenge.h"
#include "../../util/Colors.h"

class NetworkClient;
class Upload;
class SidechainAudioProcessor;

//==============================================================================
/**
 * MidiChallengeSubmission wraps the Upload component with constraint validation (R.2.2.4.3)
 *
 * Features:
 * - Shows constraint checklist (BPM ✓, Key ✓, etc.)
 * - Validates constraints before submission
 * - Submit button (disabled if constraints not met)
 * - Success confirmation
 * - Reuses existing Upload component for audio/MIDI capture
 */
class MidiChallengeSubmission : public juce::Component
{
public:
    MidiChallengeSubmission(SidechainAudioProcessor& processor, NetworkClient& network);
    ~MidiChallengeSubmission() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

    //==============================================================================
    // Set the challenge to submit to
    void setChallenge(const MIDIChallenge& challenge);

    // Set the audio and MIDI data (from Recording component)
    void setAudioToUpload(const juce::AudioBuffer<float>& audio, double sampleRate, const juce::var& midiData);

    // Reset state
    void reset();

    //==============================================================================
    // Callbacks
    std::function<void()> onBackPressed;
    std::function<void()> onSubmissionComplete;  // Called after successful submission

private:
    //==============================================================================
    SidechainAudioProcessor& audioProcessor;
    NetworkClient& networkClient;
    MIDIChallenge challenge;
    
    // Wrapped Upload component
    std::unique_ptr<Upload> uploadComponent;

    // Audio/MIDI data
    juce::AudioBuffer<float> audioBuffer;
    double audioSampleRate = 44100.0;
    juce::var midiData;

    // Submission state
    enum class SubmissionState
    {
        Editing,      // User is filling in metadata
        Validating,   // Validating constraints
        Submitting,   // Submission in progress
        Success,      // Submission completed
        Error         // Submission failed
    };
    SubmissionState submissionState = SubmissionState::Editing;
    juce::String errorMessage;

    // Constraint validation results
    struct ConstraintCheck
    {
        bool passed = false;
        juce::String message;
    };
    ConstraintCheck bpmCheck;
    ConstraintCheck keyCheck;
    ConstraintCheck scaleCheck;
    ConstraintCheck noteCountCheck;
    ConstraintCheck durationCheck;

    //==============================================================================
    // Drawing methods
    void drawHeader(juce::Graphics& g);
    void drawChallengeInfo(juce::Graphics& g, juce::Rectangle<int>& bounds);
    void drawConstraintChecklist(juce::Graphics& g, juce::Rectangle<int>& bounds);
    void drawConstraintItem(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text, const ConstraintCheck& check);
    void drawSubmitButton(juce::Graphics& g, juce::Rectangle<int>& bounds);
    void drawSuccessState(juce::Graphics& g, juce::Rectangle<int>& bounds);
    void drawErrorState(juce::Graphics& g, juce::Rectangle<int>& bounds);

    //==============================================================================
    // Hit testing helpers
    juce::Rectangle<int> getBackButtonBounds() const;
    juce::Rectangle<int> getSubmitButtonBounds() const;
    juce::Rectangle<int> getContentBounds() const;
    juce::Rectangle<int> getUploadComponentBounds() const;

    //==============================================================================
    // Constraint validation
    void validateConstraints(double bpm, const juce::String& key, const juce::var& midiData, double durationSeconds);
    bool allConstraintsPassed() const;
    int countMIDINotes(const juce::var& midiData) const;
    bool checkMIDIScale(const juce::var& midiData, const juce::String& requiredScale) const;

    //==============================================================================
    // Network operations
    void submitEntry(const juce::String& postId, const juce::String& audioUrl);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiChallengeSubmission)
};
