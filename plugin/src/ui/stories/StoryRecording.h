#pragma once

#include "../../audio/BufferAudioPlayer.h"
#include "../../audio/MIDICapture.h"
#include "../../audio/Microphone.h"
#include "../../ui/animations/Easing.h"
#include "../../ui/animations/TransitionAnimation.h"
#include "PianoRoll.h"
#include <JuceHeader.h>

class SidechainAudioProcessor;

//==============================================================================
/**
 * StoryRecording provides UI for recording short music clips (stories)
 * with MIDI visualization support (7.5.3.1.1)
 *
 * Features:
 * - Record button with 60-second countdown
 * - MIDI activity indicator (shows when MIDI is being captured)
 * - Waveform preview during recording
 * - Duration display with countdown (max 60 seconds)
 * - Auto-stop at 60 seconds
 * - Minimum 5 seconds required before stop enabled
 *
 * Stories are short music clips (5-60 seconds) that expire after 24 hours.
 * They can include MIDI data for piano roll visualization.
 */
class StoryRecording : public juce::Component, public juce::Timer {
public:
  StoryRecording(SidechainAudioProcessor &processor);
  ~StoryRecording() override;

  //==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;

  //==============================================================================
  // Timer callback for UI updates
  void timerCallback() override;

  //==============================================================================
  // Callbacks

  // Called when recording is complete and ready for upload
  // Provides audio buffer, MIDI data, and optional metadata
  std::function<void(const juce::AudioBuffer<float> &, const juce::var &midiData, int bpm, const juce::String &key,
                     const juce::StringArray &genres)>
      onRecordingComplete;

  // Called when user wants to discard recording
  std::function<void()> onRecordingDiscarded;

  // Called when user cancels (goes back)
  std::function<void()> onCancel;

  //==============================================================================
  // Configuration
  static constexpr double MIN_DURATION_SECONDS = 5.0;
  static constexpr double MAX_DURATION_SECONDS = 60.0;

  //==============================================================================
  // Recording source selection
  enum class RecordingSource {
    DAW,       // Record from DAW (default)
    Microphone // Record from system microphone
  };

  void setRecordingSource(RecordingSource source);
  RecordingSource getRecordingSource() const {
    return currentRecordingSource;
  }

private:
  //==============================================================================
  SidechainAudioProcessor &audioProcessor;

  // Recording source
  RecordingSource currentRecordingSource = RecordingSource::DAW;
  Microphone microphone;

  // Recording state
  enum class State {
    Idle,      // Ready to record
    Recording, // Actively recording
    Preview    // Recording complete, showing preview
  };
  State currentState = State::Idle;

  // MIDI capture
  MIDICapture midiCapture;
  bool hasMIDIActivity = false;
  bool lastMIDIActivityState = false; // For detecting transitions
  int lastMIDIEventCount = 0;

  // Cached recording data for preview
  juce::AudioBuffer<float> recordedAudio;
  double recordedSampleRate = 44100.0;
  double recordingStartTime = 0.0;
  double currentRecordingDuration = 0.0;

  // Preview playback using BufferAudioPlayer
  std::unique_ptr<BufferAudioPlayer> bufferAudioPlayer;
  bool isPreviewPlaying = false;
  double previewPlaybackPosition = 0.0;

  // MIDI visualization for preview
  std::unique_ptr<PianoRoll> pianoRollPreview;

  // Metadata (optional)
  int storyBPM = 0;
  juce::String storyKey;
  juce::StringArray storyGenres;

  // Animation state
  std::shared_ptr<Sidechain::UI::Animations::TransitionAnimation<float>> recordingDotAnimation;
  float recordingDotOpacity = 0.0f;

  std::shared_ptr<Sidechain::UI::Animations::TransitionAnimation<float>> midiActivityAnimation;
  float midiActivityOpacity = 0.0f;

  // Helper to start/stop the looping animations
  void startRecordingDotAnimation();
  void stopRecordingDotAnimation();
  void triggerMIDIActivityAnimation();

  // UI areas (calculated in resized())
  juce::Rectangle<int> headerArea;
  juce::Rectangle<int> recordButtonArea;
  juce::Rectangle<int> timeDisplayArea;
  juce::Rectangle<int> countdownArea;
  juce::Rectangle<int> midiIndicatorArea;
  juce::Rectangle<int> waveformArea;
  juce::Rectangle<int> pianoRollArea;
  juce::Rectangle<int> playbackControlsArea;
  juce::Rectangle<int> metadataArea;
  juce::Rectangle<int> actionButtonsArea;
  juce::Rectangle<int> cancelButtonArea;
  juce::Rectangle<int> sourceToggleArea;

  //==============================================================================
  // Drawing helpers
  void drawHeader(juce::Graphics &g);
  void drawRecordButton(juce::Graphics &g);
  void drawTimeDisplay(juce::Graphics &g);
  void drawCountdownRing(juce::Graphics &g);
  void drawMIDIIndicator(juce::Graphics &g);
  void drawWaveformPreview(juce::Graphics &g);
  void drawPlaybackControls(juce::Graphics &g);
  void drawMetadataInput(juce::Graphics &g);
  void drawActionButtons(juce::Graphics &g);
  void drawSourceToggle(juce::Graphics &g);
  void drawIdleState(juce::Graphics &g);
  void drawRecordingState(juce::Graphics &g);
  void drawPreviewState(juce::Graphics &g);

  // Helper to format time as MM:SS
  juce::String formatTime(double seconds);

  //==============================================================================
  // Button actions
  void startRecording();
  void stopRecording();
  void discardRecording();
  void confirmRecording();

  // Check if stop is allowed (minimum duration reached)
  bool canStopRecording() const;

  // Preview playback controls
  void togglePreviewPlayback();
  void stopPreviewPlayback();

  // Get buffer audio player for mixing in processor (called from audio thread)
  BufferAudioPlayer *getBufferAudioPlayer() {
    return bufferAudioPlayer.get();
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StoryRecording)
};
