#pragma once

#include "../../audio/KeyDetector.h"
#include "../../network/NetworkClient.h"
#include "../../stores/AppStore.h"
#include "../../ui/common/AppStoreComponent.h"
#include <JuceHeader.h>
#include <memory>

// Forward declarations
class SidechainAudioProcessor;

// ==============================================================================
/**
 * Upload provides the UI for sharing a recorded loop.
 *
 * Features:
 * - Filename input (required) - display name for the audio file
 * - BPM display (auto-detected from DAW via AudioPlayHead)
 * - Musical key dropdown (24 keys + "Not set")
 * - Genre dropdown
 * - Waveform preview of the recording
 * - Upload progress indicator
 * - Cancel/Share buttons
 *
 * Design: Dark theme matching the plugin aesthetic, producer-friendly
 */
class Upload : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::UploadState>, public juce::Timer {
public:
  // ==========================================================================
  // Musical key options
  static constexpr int NUM_KEYS = 25; // 24 keys + "Not set"

  struct MusicalKey {
    juce::String name;
    juce::String shortName;
  };

  static const std::array<MusicalKey, NUM_KEYS> &getMusicalKeys();

  // ==========================================================================
  // Genre options
  static constexpr int NUM_GENRES = 12;
  static const std::array<juce::String, NUM_GENRES> &getGenres();

  // ==========================================================================
  // Comment audience options
  static constexpr int NUM_COMMENT_AUDIENCES = 3;
  struct CommentAudienceOption {
    juce::String value;       // Backend value: "everyone", "followers", "off"
    juce::String displayName; // UI display name
  };
  static const std::array<CommentAudienceOption, NUM_COMMENT_AUDIENCES> &getCommentAudiences();

  // ==========================================================================
  Upload(SidechainAudioProcessor &processor, NetworkClient &network, Sidechain::Stores::AppStore *store = nullptr);
  ~Upload() override;

  // ==========================================================================
  // Set the audio to upload (called when user confirms recording)
  void setAudioToUpload(const juce::AudioBuffer<float> &audio, double sampleRate);

  // Set the audio and MIDI data to upload
  void setAudioToUpload(const juce::AudioBuffer<float> &audio, double sampleRate, const juce::var &midiData);

  // Clear state and prepare for new upload
  void reset();

  // ==========================================================================
  // Draft support

  /** Get current form data for draft saving */
  juce::String getFilename() const {
    return filename;
  }
  double getBpm() const {
    return bpm;
  }
  int getKeyIndex() const {
    return selectedKeyIndex;
  }
  int getGenreIndex() const {
    return selectedGenreIndex;
  }
  int getCommentAudienceIndex() const {
    return selectedCommentAudienceIndex;
  }
  const juce::var &getMidiData() const {
    return midiData;
  }
  const juce::AudioBuffer<float> &getAudioBuffer() const {
    return audioBuffer;
  }
  double getSampleRate() const {
    return audioSampleRate;
  }

  /** Set form data from a draft */
  void loadFromDraft(const juce::String &draftFilename, double draftBpm, int keyIdx, int genreIdx, int commentIdx);

  // ==========================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void timerCallback() override;
  bool keyPressed(const juce::KeyPress &key) override;
  void focusGained(FocusChangeType cause) override;

  // ==========================================================================
  // Callbacks
  std::function<void()> onUploadComplete; // Called after successful upload
  std::function<void()> onCancel;         // Called when user cancels
  std::function<void()> onSaveAsDraft;    // Called when user saves as draft

protected:
  void onAppStateChanged(const Sidechain::Stores::UploadState &state) override;
  void subscribeToAppStore() override;

private:
  // ==========================================================================
  SidechainAudioProcessor &audioProcessor;
  NetworkClient &networkClient;

  // Audio data to upload
  juce::AudioBuffer<float> audioBuffer;
  double audioSampleRate = 44100.0;

  // Upload state
  enum class UploadState {
    Editing,   // User is filling in metadata
    Uploading, // Upload in progress
    Success,   // Upload completed
    Error      // Upload failed
  };
  UploadState uploadState = UploadState::Editing;
  float uploadProgress = 0.0f;
  juce::String errorMessage;

  // Timer IDs for progress simulation (to allow cancellation)
  int progressTimer500ms = 0;
  int progressTimer1000ms = 0;
  int successDismissTimer = 0;

  // Form data
  juce::String filename; // Display filename for the audio file
  double bpm = 0.0;
  bool bpmFromDAW = false;
  int selectedKeyIndex = 0;             // 0 = "Not set"
  int selectedGenreIndex = 0;           // 0 = first genre
  int selectedCommentAudienceIndex = 0; // 0 = "Everyone"

  // MIDI data
  juce::var midiData;      // Captured MIDI events from recording
  bool includeMidi = true; // Whether to include MIDI in upload

  // Project file data
  juce::File projectFile;          // Selected project file (optional)
  bool includeProjectFile = false; // Whether to include project file in upload

  // UI state
  int activeField = -1; // -1 = none, 0 = title, 1 = bpm
  int hoveredButton = -1;

  // Tap tempo state
  std::vector<double> tapTimes;
  double lastTapTime = 0.0;

  // Last uploaded post info (for success preview)
  juce::String lastUploadedFilename;
  juce::String lastUploadedGenre;
  double lastUploadedBpm = 0.0;
  juce::String lastUploadedUrl;

  // UI areas (calculated in resized)
  juce::Rectangle<int> headerArea;
  juce::Rectangle<int> waveformArea;
  juce::Rectangle<int> filenameFieldArea;
  juce::Rectangle<int> bpmFieldArea;
  juce::Rectangle<int> tapTempoButtonArea;
  juce::Rectangle<int> keyDropdownArea;
  juce::Rectangle<int> detectKeyButtonArea;
  juce::Rectangle<int> genreDropdownArea;
  juce::Rectangle<int> commentAudienceArea;
  juce::Rectangle<int> projectFileArea; // Project File Exchange
  juce::Rectangle<int> progressBarArea;
  juce::Rectangle<int> draftButtonArea;
  juce::Rectangle<int> cancelButtonArea;
  juce::Rectangle<int> shareButtonArea;
  juce::Rectangle<int> statusArea;

  // ==========================================================================
  // Drawing helpers
  void drawHeader(juce::Graphics &g);
  void drawWaveform(juce::Graphics &g);
  void drawFilenameField(juce::Graphics &g);
  void drawBPMField(juce::Graphics &g);
  void drawTapTempoButton(juce::Graphics &g);
  void drawKeyDropdown(juce::Graphics &g);
  void drawDetectKeyButton(juce::Graphics &g);
  void drawGenreDropdown(juce::Graphics &g);
  void drawCommentAudienceDropdown(juce::Graphics &g);
  void drawProjectFileButton(juce::Graphics &g); // Project File Exchange
  void drawProgressBar(juce::Graphics &g);
  void drawButtons(juce::Graphics &g);
  void drawStatus(juce::Graphics &g);

  void drawTextField(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &label,
                     const juce::String &value, bool isActive, bool isEditable = true);
  void drawDropdown(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &label,
                    const juce::String &value, bool isHovered);
  void drawButton(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
                  bool isHovered, bool isEnabled = true);

  juce::Path generateWaveformPath(juce::Rectangle<int> bounds);
  juce::String formatDuration() const;

  // ==========================================================================
  // Actions
  void handleTapTempo();
  void detectKey();
  void showKeyPicker();
  void showGenrePicker();
  void showCommentAudiencePicker();
  void selectProjectFile(); // Project File Exchange
  void cancelUpload();
  void startUpload();

  // Key detection
  KeyDetector keyDetector;
  bool isDetectingKey = false;
  juce::String keyDetectionStatus;

  // Keyboard input handling
  void handleKeyPress(const juce::KeyPress &key);
  void handleTextInput(juce::juce_wchar character);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Upload)
};
