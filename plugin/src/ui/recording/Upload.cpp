#include "Upload.h"
#include "../../util/Async.h"
#include "../../util/Colors.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringFormatter.h"
#include "core/PluginProcessor.h"

// ==============================================================================
// Static data: Musical keys (Camelot wheel order is producer-friendly)
const std::array<Upload::MusicalKey, Upload::NUM_KEYS> &Upload::getMusicalKeys() {
  static const std::array<MusicalKey, NUM_KEYS> keys = {
      {{"Not set", "-"},         {"C Major", "C"},         {"C# / Db Major", "C#"},  {"D Major", "D"},
       {"D# / Eb Major", "D#"},  {"E Major", "E"},         {"F Major", "F"},         {"F# / Gb Major", "F#"},
       {"G Major", "G"},         {"G# / Ab Major", "G#"},  {"A Major", "A"},         {"A# / Bb Major", "A#"},
       {"B Major", "B"},         {"C Minor", "Cm"},        {"C# / Db Minor", "C#m"}, {"D Minor", "Dm"},
       {"D# / Eb Minor", "D#m"}, {"E Minor", "Em"},        {"F Minor", "Fm"},        {"F# / Gb Minor", "F#m"},
       {"G Minor", "Gm"},        {"G# / Ab Minor", "G#m"}, {"A Minor", "Am"},        {"A# / Bb Minor", "A#m"},
       {"B Minor", "Bm"}}};
  return keys;
}

// Static data: Genres
const std::array<juce::String, Upload::NUM_GENRES> &Upload::getGenres() {
  static const std::array<juce::String, NUM_GENRES> genres = {{"Electronic", "Hip-Hop / Trap", "House", "Techno",
                                                               "Drum & Bass", "Dubstep", "Pop", "R&B / Soul", "Rock",
                                                               "Lo-Fi", "Ambient", "Other"}};
  return genres;
}

// Static data: Comment audience options
const std::array<Upload::CommentAudienceOption, Upload::NUM_COMMENT_AUDIENCES> &Upload::getCommentAudiences() {
  static const std::array<CommentAudienceOption, NUM_COMMENT_AUDIENCES> audiences = {
      {{"everyone", "Everyone"}, {"followers", "Followers Only"}, {"off", "Off"}}};
  return audiences;
}

// ==============================================================================
Upload::Upload(SidechainAudioProcessor &processor, NetworkClient &network, Sidechain::Stores::AppStore *store)
    : AppStoreComponent(store), audioProcessor(processor), networkClient(network) {
  Log::info("Upload: Initializing upload component");
  setWantsKeyboardFocus(true);
  startTimerHz(30);
  Log::debug("Upload: Timer started at 30Hz, keyboard focus enabled");
  Log::info("Upload: Initialization complete");
}

Upload::~Upload() {
  Log::debug("Upload: Destroying upload component");
  stopTimer();
}

// ==============================================================================
void Upload::subscribeToAppStore() {
  if (!appStore)
    return;

  juce::Component::SafePointer<Upload> safeThis(this);
  storeUnsubscriber = appStore->subscribeToUploads([safeThis](const Sidechain::Stores::UploadState &state) {
    if (!safeThis)
      return;

    auto isUploading = state.isUploading;
    auto progress = state.progress;
    auto errorMsg = state.uploadError;

    juce::MessageManager::callAsync([safeThis, isUploading, progress, errorMsg]() {
      if (!safeThis)
        return;

      if (isUploading) {
        safeThis->uploadState = Upload::UploadState::Uploading;
        safeThis->uploadProgress = static_cast<float>(progress);
        safeThis->errorMessage = "";
      } else if (!errorMsg.isEmpty()) {
        safeThis->uploadState = Upload::UploadState::Error;
        safeThis->errorMessage = errorMsg;
      } else if (progress > 0) {
        safeThis->uploadState = Upload::UploadState::Success;
        safeThis->uploadProgress = 100.0f;
        safeThis->errorMessage = "";
        if (safeThis->onUploadComplete) {
          safeThis->onUploadComplete();
        }
      } else {
        safeThis->uploadState = Upload::UploadState::Editing;
        safeThis->uploadProgress = 0.0f;
        safeThis->errorMessage = "";
      }

      safeThis->repaint();
    });
  });
}

void Upload::onAppStateChanged(const Sidechain::Stores::UploadState & /*state*/) {
  repaint();
}

// ==============================================================================
void Upload::setAudioToUpload(const juce::AudioBuffer<float> &audio, double sampleRate) {
  // Call overloaded version with empty MIDI data
  setAudioToUpload(audio, sampleRate, juce::var());
}

void Upload::setAudioToUpload(const juce::AudioBuffer<float> &audio, double sampleRate, const juce::var &midi) {
  audioBuffer = audio;
  audioSampleRate = sampleRate;
  midiData = midi; // Store MIDI data for upload

  // Get BPM from DAW
  if (audioProcessor.isBPMAvailable()) {
    bpm = audioProcessor.getCurrentBPM();
    bpmFromDAW = true;
  } else {
    bpm = Constants::Audio::DEFAULT_BPM;
    bpmFromDAW = false;
  }

  // Reset form state
  filename = "";
  selectedKeyIndex = 0;
  selectedGenreIndex = 0;
  uploadState = UploadState::Editing;
  uploadProgress = 0.0f;
  errorMessage = "";
  activeField = 0;    // Focus filename field
  includeMidi = true; // Default to including MIDI if available

  // Log MIDI info
  if (!midiData.isVoid() && midiData.hasProperty("events")) {
    auto events = midiData["events"];
    if (events.isArray()) {
      Log::info("Upload::setAudioToUpload: Received " + juce::String(events.size()) + " MIDI events");
    }
  }

  repaint();
}

void Upload::reset() {
  // Cancel any pending timers
  if (progressTimer500ms != 0) {
    Async::cancelDelay(progressTimer500ms);
    progressTimer500ms = 0;
  }
  if (progressTimer1000ms != 0) {
    Async::cancelDelay(progressTimer1000ms);
    progressTimer1000ms = 0;
  }
  if (successDismissTimer != 0) {
    Async::cancelDelay(successDismissTimer);
    successDismissTimer = 0;
  }

  audioBuffer.setSize(0, 0);
  filename = "";
  bpm = 0.0;
  bpmFromDAW = false;
  selectedKeyIndex = 0;
  selectedGenreIndex = 0;
  selectedCommentAudienceIndex = 0; // Reset to "Everyone"
  uploadState = UploadState::Editing;
  uploadProgress = 0.0f;
  errorMessage = "";
  activeField = -1;
  tapTimes.clear();
  midiData = juce::var(); // Clear MIDI data
  includeMidi = true;
  projectFile = juce::File(); // Clear project file
  includeProjectFile = false;
  Log::debug("Upload::reset: All state cleared");

  repaint();
}

// ==============================================================================
void Upload::timerCallback() {
  // Update BPM from DAW if we're still editing and it changes
  if (uploadState == UploadState::Editing && bpmFromDAW && audioProcessor.isBPMAvailable()) {
    double newBpm = audioProcessor.getCurrentBPM();
    if (std::abs(newBpm - bpm) > 0.1) {
      Log::debug("Upload::timerCallback: BPM updated from DAW: " + juce::String(bpm, 1) + " -> " +
                 juce::String(newBpm, 1));
      bpm = newBpm;
      repaint();
    }
  }

  // Animate upload progress (simulate for now)
  if (uploadState == UploadState::Uploading) {
    // In real implementation, this would be updated by network callback
    repaint();
  }
}

// ==============================================================================
void Upload::paint(juce::Graphics &g) {
  // Dark background with subtle gradient
  auto bounds = getLocalBounds();
  g.setGradientFill(
      SidechainColors::backgroundGradient(bounds.getTopLeft().toFloat(), bounds.getBottomLeft().toFloat()));
  g.fillRect(bounds);

  // Draw all sections
  drawHeader(g);
  drawWaveform(g);
  drawFilenameField(g);
  drawBPMField(g);
  drawTapTempoButton(g);
  drawKeyDropdown(g);
  drawDetectKeyButton(g);
  drawGenreDropdown(g);
  drawCommentAudienceDropdown(g);
  drawProjectFileButton(g);

  if (uploadState == UploadState::Uploading) {
    drawProgressBar(g);
  }

  drawButtons(g);
  drawStatus(g);
}

void Upload::resized() {
  Log::debug("Upload::resized: Component resized to " + juce::String(getWidth()) + "x" + juce::String(getHeight()));
  auto bounds = getLocalBounds().reduced(24);
  int rowHeight = 48;
  int fieldSpacing = 16;

  // Header
  headerArea = bounds.removeFromTop(40);
  bounds.removeFromTop(fieldSpacing);

  // Waveform preview
  waveformArea = bounds.removeFromTop(100);
  bounds.removeFromTop(fieldSpacing);

  // Filename field (full width)
  filenameFieldArea = bounds.removeFromTop(rowHeight);
  bounds.removeFromTop(fieldSpacing);

  // BPM field + Tap tempo button (side by side)
  auto bpmRow = bounds.removeFromTop(rowHeight);
  bpmFieldArea = bpmRow.removeFromLeft(bpmRow.getWidth() / 2 - 8);
  bpmRow.removeFromLeft(16);
  tapTempoButtonArea = bpmRow;
  bounds.removeFromTop(fieldSpacing);

  // Key dropdown + Detect button (left side), Genre dropdown (right side)
  auto dropdownRow = bounds.removeFromTop(rowHeight);
  auto leftHalf = dropdownRow.removeFromLeft(dropdownRow.getWidth() / 2 - 8);
  keyDropdownArea = leftHalf.removeFromLeft(leftHalf.getWidth() - 80); // Leave room for detect button
  leftHalf.removeFromLeft(8);
  detectKeyButtonArea = leftHalf; // Remaining space for detect button
  dropdownRow.removeFromLeft(16);
  genreDropdownArea = dropdownRow;
  bounds.removeFromTop(fieldSpacing);

  // Comment audience dropdown - takes half width
  auto commentRow = bounds.removeFromTop(rowHeight);
  commentAudienceArea = commentRow.removeFromLeft(commentRow.getWidth() / 2 - 8);
  bounds.removeFromTop(fieldSpacing);

  // Project file button
  projectFileArea = bounds.removeFromTop(rowHeight);
  bounds.removeFromTop(fieldSpacing);

  // Progress bar (only shown during upload)
  progressBarArea = bounds.removeFromTop(24);
  bounds.removeFromTop(fieldSpacing);

  // Status area
  statusArea = bounds.removeFromTop(24);
  bounds.removeFromTop(fieldSpacing);

  // Buttons at bottom (3 buttons: Cancel, Save Draft, Share)
  auto buttonRow = bounds.removeFromBottom(52);
  int buttonWidth = (buttonRow.getWidth() - 32) / 3; // 3 buttons with 2 gaps
  cancelButtonArea = buttonRow.removeFromLeft(buttonWidth);
  buttonRow.removeFromLeft(16);
  draftButtonArea = buttonRow.removeFromLeft(buttonWidth);
  buttonRow.removeFromLeft(16);
  shareButtonArea = buttonRow;
}

// ==============================================================================
void Upload::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();
  Log::debug("Upload::mouseUp: Mouse clicked at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + "), state: " +
             juce::String(uploadState == UploadState::Editing     ? "Editing"
                          : uploadState == UploadState::Uploading ? "Uploading"
                          : uploadState == UploadState::Success   ? "Success"
                                                                  : "Error"));

  if (uploadState == UploadState::Editing) {
    // Filename field
    if (filenameFieldArea.contains(pos)) {
      Log::info("Upload::mouseUp: Filename field clicked");
      activeField = 0;
      grabKeyboardFocus();
      repaint();
      return;
    }

    // BPM field
    if (bpmFieldArea.contains(pos)) {
      Log::info("Upload::mouseUp: BPM field clicked");
      activeField = 1;
      bpmFromDAW = false; // Manual override
      Log::debug("Upload::mouseUp: BPM manual override enabled");
      grabKeyboardFocus();
      repaint();
      return;
    }

    // Tap tempo
    if (tapTempoButtonArea.contains(pos)) {
      Log::info("Upload::mouseUp: Tap tempo button clicked");
      handleTapTempo();
      return;
    }

    // Key dropdown
    if (keyDropdownArea.contains(pos)) {
      Log::info("Upload::mouseUp: Key dropdown clicked");
      showKeyPicker();
      return;
    }

    // Detect key button
    if (detectKeyButtonArea.contains(pos)) {
      Log::info("Upload::mouseUp: Detect key button clicked");
      detectKey();
      return;
    }

    // Genre dropdown
    if (genreDropdownArea.contains(pos)) {
      Log::info("Upload::mouseUp: Genre dropdown clicked");
      showGenrePicker();
      return;
    }

    // Comment audience dropdown
    if (commentAudienceArea.contains(pos)) {
      Log::info("Upload::mouseUp: Comment audience dropdown clicked");
      showCommentAudiencePicker();
      return;
    }

    // Project file button
    if (projectFileArea.contains(pos)) {
      Log::info("Upload::mouseUp: Project file button clicked");
      selectProjectFile();
      return;
    }

    // Cancel button
    if (cancelButtonArea.contains(pos)) {
      Log::info("Upload::mouseUp: Cancel button clicked");
      cancelUpload();
      return;
    }

    // Save as Draft button
    if (draftButtonArea.contains(pos)) {
      Log::info("Upload::mouseUp: Save as Draft button clicked");
      if (onSaveAsDraft)
        onSaveAsDraft();
      return;
    }

    // Share button
    if (shareButtonArea.contains(pos)) {
      Log::info("Upload::mouseUp: Share button clicked");
      startUpload();
      return;
    }

    // Clicked elsewhere - clear field focus
    Log::debug("Upload::mouseUp: Clicked outside fields, clearing focus");
    activeField = -1;
    repaint();
  } else if (uploadState == UploadState::Success || uploadState == UploadState::Error) {
    // Any click dismisses
    if (uploadState == UploadState::Success && onUploadComplete) {
      Log::info("Upload::mouseUp: Success state clicked, calling onUploadComplete");
      onUploadComplete();
    } else if (uploadState == UploadState::Error) {
      Log::info("Upload::mouseUp: Error state clicked, returning to Editing");
      uploadState = UploadState::Editing;
      repaint();
    }
  }
}

// ==============================================================================
void Upload::drawHeader(juce::Graphics &g) {
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(24.0f).withStyle("Bold")));
  g.drawText("Share Your Loop", headerArea, juce::Justification::centredLeft);

  // Duration badge
  auto durationBadge = headerArea.removeFromRight(80);
  g.setFont(14.0f);
  g.setColour(SidechainColors::textMuted());
  g.drawText(formatDuration(), durationBadge, juce::Justification::centredRight);
}

void Upload::drawWaveform(juce::Graphics &g) {
  // Background
  g.setColour(SidechainColors::waveformBackground());
  g.fillRoundedRectangle(waveformArea.toFloat(), 8.0f);

  if (audioBuffer.getNumSamples() == 0)
    return;

  // Draw waveform
  auto path = generateWaveformPath(waveformArea.reduced(12, 8));
  g.setColour(SidechainColors::waveform());
  g.strokePath(path, juce::PathStrokeType(2.0f));
}

void Upload::drawFilenameField(juce::Graphics &g) {
  drawTextField(g, filenameFieldArea, "Filename", filename, activeField == 0);
}

void Upload::drawBPMField(juce::Graphics &g) {
  juce::String bpmText = bpm > 0 ? juce::String(bpm, 1) : "";
  juce::String label = bpmFromDAW ? "BPM (from DAW)" : "BPM";
  drawTextField(g, bpmFieldArea, label, bpmText, activeField == 1);
}

void Upload::drawTapTempoButton(juce::Graphics &g) {
  bool isHovered = tapTempoButtonArea.contains(getMouseXYRelative());
  auto bgColor = isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface();

  g.setColour(bgColor);
  g.fillRoundedRectangle(tapTempoButtonArea.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(tapTempoButtonArea.toFloat(), 8.0f, 1.0f);

  // Text
  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  g.drawText("Tap Tempo", tapTempoButtonArea, juce::Justification::centred);
}

void Upload::drawKeyDropdown(juce::Graphics &g) {
  auto &keys = getMusicalKeys();
  juce::String value =
      selectedKeyIndex < static_cast<int>(keys.size()) ? keys[static_cast<size_t>(selectedKeyIndex)].name : "Not set";
  bool isHovered = keyDropdownArea.contains(getMouseXYRelative());
  drawDropdown(g, keyDropdownArea, "Key", value, isHovered);
}

void Upload::drawDetectKeyButton(juce::Graphics &g) {
  bool isHovered = detectKeyButtonArea.contains(getMouseXYRelative());
  bool isEnabled = KeyDetector::isAvailable() && audioBuffer.getNumSamples() > 0 && !isDetectingKey;

  auto bgColor = isEnabled ? (isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface())
                           : SidechainColors::backgroundLight();

  g.setColour(bgColor);
  g.fillRoundedRectangle(detectKeyButtonArea.toFloat(), 8.0f);

  // Border
  auto borderColor = isEnabled ? SidechainColors::border() : SidechainColors::borderSubtle();
  g.setColour(borderColor);
  g.drawRoundedRectangle(detectKeyButtonArea.toFloat(), 8.0f, 1.0f);

  // Text
  g.setColour(isEnabled ? SidechainColors::textPrimary() : SidechainColors::textMuted());
  g.setFont(12.0f);

  juce::String buttonText = isDetectingKey ? "..." : "Detect";
  g.drawText(buttonText, detectKeyButtonArea, juce::Justification::centred);
}

void Upload::drawGenreDropdown(juce::Graphics &g) {
  auto &genres = getGenres();
  juce::String value = selectedGenreIndex < static_cast<int>(genres.size())
                           ? genres[static_cast<size_t>(selectedGenreIndex)]
                           : "Electronic";
  bool isHovered = genreDropdownArea.contains(getMouseXYRelative());
  drawDropdown(g, genreDropdownArea, "Genre", value, isHovered);
}

void Upload::drawCommentAudienceDropdown(juce::Graphics &g) {
  auto &audiences = getCommentAudiences();
  juce::String value = selectedCommentAudienceIndex < static_cast<int>(audiences.size())
                           ? audiences[static_cast<size_t>(selectedCommentAudienceIndex)].displayName
                           : "Everyone";
  bool isHovered = commentAudienceArea.contains(getMouseXYRelative());
  drawDropdown(g, commentAudienceArea, "Comments", value, isHovered);
}

void Upload::drawProjectFileButton(juce::Graphics &g) {
  bool isHovered = projectFileArea.contains(getMouseXYRelative());
  auto bgColor = isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface();

  g.setColour(bgColor);
  g.fillRoundedRectangle(projectFileArea.toFloat(), 8.0f);

  // Border - use accent color if file is selected
  auto borderColor = includeProjectFile ? SidechainColors::primary() : SidechainColors::border();
  g.setColour(borderColor);
  g.drawRoundedRectangle(projectFileArea.toFloat(), 8.0f, includeProjectFile ? 2.0f : 1.0f);

  auto innerBounds = projectFileArea.reduced(16, 0);

  // Label
  g.setColour(SidechainColors::textMuted());
  g.setFont(11.0f);
  auto labelBounds = innerBounds.removeFromTop(20).withTrimmedTop(6);
  g.drawText("Project File (Optional)", labelBounds, juce::Justification::centredLeft);

  // Value or placeholder
  g.setFont(14.0f);
  auto valueBounds = innerBounds.withTrimmedBottom(8);

  if (includeProjectFile && projectFile.existsAsFile()) {
    // Show filename and file size
    juce::String projectFilename = projectFile.getFileName();
    if (projectFilename.length() > 35)
      projectFilename = projectFilename.substring(0, 32) + "...";

    int64 sizeKB = projectFile.getSize() / 1024;
    juce::String sizeStr =
        sizeKB > 1024 ? juce::String(static_cast<double>(sizeKB) / 1024.0, 1) + " MB" : juce::String(sizeKB) + " KB";

    g.setColour(SidechainColors::textPrimary());
    g.drawText(projectFilename, valueBounds.removeFromLeft(valueBounds.getWidth() - 80),
               juce::Justification::centredLeft);

    g.setColour(SidechainColors::textMuted());
    g.setFont(11.0f);
    g.drawText(sizeStr, valueBounds, juce::Justification::centredRight);
  } else {
    g.setColour(SidechainColors::textMuted());
    g.drawText("Click to attach DAW project (.als, .flp, .logicx, ...)", valueBounds, juce::Justification::centredLeft);
  }

  // Show clear button if file is selected
  if (includeProjectFile) {
    auto clearBounds = projectFileArea.removeFromRight(40);
    g.setColour(SidechainColors::textMuted());
    g.setFont(14.0f);
    g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x9C\x95")), clearBounds,
               juce::Justification::centred); // X symbol
  }
}

void Upload::drawProgressBar(juce::Graphics &g) {
  // Background
  g.setColour(SidechainColors::backgroundLight());
  g.fillRoundedRectangle(progressBarArea.toFloat(), 4.0f);

  // Progress fill
  if (uploadProgress > 0.0f) {
    auto fillWidth = static_cast<float>(progressBarArea.getWidth()) * uploadProgress;
    auto fillRect = progressBarArea.withWidth(static_cast<int>(fillWidth));
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(fillRect.toFloat(), 4.0f);
  }
}

void Upload::drawButtons(juce::Graphics &g) {
  bool cancelHovered = cancelButtonArea.contains(getMouseXYRelative());
  bool draftHovered = draftButtonArea.contains(getMouseXYRelative());
  bool shareHovered = shareButtonArea.contains(getMouseXYRelative());
  bool canShare = !filename.isEmpty() && audioBuffer.getNumSamples() > 0;
  bool canSaveDraft = audioBuffer.getNumSamples() > 0; // Can save draft even without title

  if (uploadState == UploadState::Uploading) {
    // Show cancel only during upload
    drawButton(g, cancelButtonArea, "Cancel", SidechainColors::buttonSecondary(), cancelHovered, true);
    // Draft button disabled during upload
    drawButton(g, draftButtonArea, "Save Draft", SidechainColors::surface(), false, false);
    // Share button disabled during upload
    drawButton(g, shareButtonArea, "Uploading...", SidechainColors::primary().darker(0.2f), false, false);
  } else {
    drawButton(g, cancelButtonArea, "Cancel", SidechainColors::buttonSecondary(), cancelHovered, true);
    drawButton(g, draftButtonArea, "Save Draft", SidechainColors::surface(), draftHovered, canSaveDraft);
    drawButton(g, shareButtonArea, "Share Loop", SidechainColors::primary(), shareHovered, canShare);
  }
}

void Upload::drawStatus(juce::Graphics &g) {
  if (uploadState == UploadState::Error && !errorMessage.isEmpty()) {
    g.setColour(SidechainColors::error());
    g.setFont(14.0f);
    g.drawText(errorMessage, statusArea, juce::Justification::centred);
  } else if (uploadState == UploadState::Success) {
    // Success icon and title
    g.setColour(SidechainColors::success());
    g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
    g.drawText(juce::CharPointer_UTF8("\xE2\x9C\x93 Loop shared!"), statusArea,
               juce::Justification::centred); // checkmark

    // Show post details below
    auto detailsArea = statusArea.translated(0, 24);
    g.setColour(SidechainColors::textSecondary());
    g.setFont(12.0f);

    juce::String details = "\"" + lastUploadedFilename + "\"";
    if (!lastUploadedGenre.isEmpty())
      details += " · " + lastUploadedGenre;
    if (lastUploadedBpm > 0)
      details += " · " + StringFormatter::formatBPM(lastUploadedBpm);

    g.drawText(details, detailsArea, juce::Justification::centred);
  } else if (uploadState == UploadState::Uploading) {
    g.setColour(SidechainColors::primary());
    g.setFont(14.0f);
    g.drawText("Uploading... " + StringFormatter::formatPercentage(uploadProgress), statusArea,
               juce::Justification::centred);
  } else if (filename.isEmpty() && activeField != 0) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(12.0f);
    g.drawText("Give your loop a filename to share", statusArea, juce::Justification::centred);
  }
}

// ==============================================================================
void Upload::drawTextField(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &label,
                           const juce::String &value, bool isActive, bool isEditable) {
  // Draw state based on isEditable (read-only vs editable mode)
  if (!isEditable) {
    g.setColour(juce::Colours::grey.withAlpha(0.5f)); // Dimmed for read-only
  }
  // Background
  auto bgColor = isActive ? SidechainColors::surfaceHover() : SidechainColors::surface();
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Border
  auto borderColor = isActive ? SidechainColors::borderActive() : SidechainColors::border();
  g.setColour(borderColor);
  g.drawRoundedRectangle(bounds.toFloat(), 8.0f, isActive ? 2.0f : 1.0f);

  auto innerBounds = bounds.reduced(16, 0);

  // Label (top-left, smaller)
  g.setColour(SidechainColors::textMuted());
  g.setFont(11.0f);
  auto labelBounds = innerBounds.removeFromTop(20).withTrimmedTop(6);
  g.drawText(label, labelBounds, juce::Justification::centredLeft);

  // Value
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  auto valueBounds = innerBounds.withTrimmedBottom(8);

  if (value.isEmpty() && isActive) {
    g.setColour(SidechainColors::textMuted());
    g.drawText("Enter " + label.toLowerCase() + "...", valueBounds, juce::Justification::centredLeft);
  } else {
    g.drawText(value + (isActive ? "|" : ""), valueBounds, juce::Justification::centredLeft);
  }
}

void Upload::drawDropdown(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &label,
                          const juce::String &value, bool isHovered) {
  auto bgColor = isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface();
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

  auto innerBounds = bounds.reduced(16, 0);

  // Label
  g.setColour(SidechainColors::textMuted());
  g.setFont(11.0f);
  auto labelBounds = innerBounds.removeFromTop(20).withTrimmedTop(6);
  g.drawText(label, labelBounds, juce::Justification::centredLeft);

  // Value
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  auto valueBounds = innerBounds.withTrimmedBottom(8);
  g.drawText(value, valueBounds, juce::Justification::centredLeft);

  // Dropdown arrow
  auto arrowArea = bounds.removeFromRight(40);
  g.setColour(SidechainColors::textMuted());
  juce::Path arrow;
  float cx = static_cast<float>(arrowArea.getCentreX());
  float cy = static_cast<float>(arrowArea.getCentreY());
  arrow.addTriangle(cx - 6, cy - 3, cx + 6, cy - 3, cx, cy + 4);
  g.fillPath(arrow);
}

void Upload::drawButton(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
                        bool isHovered, bool isEnabled) {
  auto color = isEnabled ? (isHovered ? bgColor.brighter(0.15f) : bgColor) : bgColor.withAlpha(0.5f);
  g.setColour(color);
  g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

  g.setColour(isEnabled ? SidechainColors::textPrimary() : SidechainColors::textPrimary().withAlpha(0.5f));
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
  g.drawText(text, bounds, juce::Justification::centred);
}

juce::Path Upload::generateWaveformPath(juce::Rectangle<int> bounds) {
  juce::Path path;

  if (audioBuffer.getNumSamples() == 0)
    return path;

  int numSamples = audioBuffer.getNumSamples();
  int width = bounds.getWidth();
  float height = static_cast<float>(bounds.getHeight());
  float centerY = static_cast<float>(bounds.getCentreY());

  path.startNewSubPath(static_cast<float>(bounds.getX()), centerY);

  for (int x = 0; x < width; ++x) {
    int startSample = x * numSamples / width;
    int endSample = juce::jmin((x + 1) * numSamples / width, numSamples);

    float peak = 0.0f;
    for (int i = startSample; i < endSample; ++i) {
      for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch) {
        peak = juce::jmax(peak, std::abs(audioBuffer.getSample(ch, i)));
      }
    }

    float y = centerY - (peak * height * 0.45f);
    path.lineTo(static_cast<float>(bounds.getX() + x), y);
  }

  return path;
}

juce::String Upload::formatDuration() const {
  if (audioBuffer.getNumSamples() == 0 || audioSampleRate <= 0)
    return "0:00";

  double seconds = static_cast<double>(audioBuffer.getNumSamples()) / audioSampleRate;
  return StringFormatter::formatDuration(seconds);
}

// ==============================================================================
void Upload::handleTapTempo() {
  double now = juce::Time::getMillisecondCounterHiRes();

  // Reset if more than 2 seconds since last tap
  if (now - lastTapTime > 2000.0) {
    tapTimes.clear();
  }

  tapTimes.push_back(now);
  lastTapTime = now;

  // Need at least 2 taps to calculate BPM
  if (tapTimes.size() >= 2) {
    // Calculate average interval
    double totalInterval = 0.0;
    for (size_t i = 1; i < tapTimes.size(); ++i) {
      totalInterval += tapTimes[i] - tapTimes[i - 1];
    }
    double avgInterval = totalInterval / static_cast<double>(tapTimes.size() - 1);

    // Convert to BPM (ms to beats per minute)
    if (avgInterval > 0) {
      bpm = 60000.0 / avgInterval;
      bpmFromDAW = false;
      repaint();
    }
  }

  // Keep only last 8 taps
  if (tapTimes.size() > 8) {
    tapTimes.erase(tapTimes.begin());
  }
}

void Upload::detectKey() {
  Log::info("Upload::detectKey: Starting key detection");

  if (!KeyDetector::isAvailable()) {
    Log::warn("Upload::detectKey: Key detection not available");
    keyDetectionStatus = "Key detection not available";
    repaint();
    return;
  }

  if (audioBuffer.getNumSamples() == 0) {
    Log::warn("Upload::detectKey: No audio to analyze");
    keyDetectionStatus = "No audio to analyze";
    repaint();
    return;
  }

  if (isDetectingKey) {
    Log::debug("Upload::detectKey: Key detection already in progress");
    return;
  }

  isDetectingKey = true;
  keyDetectionStatus = "Analyzing...";
  Log::debug("Upload::detectKey: Starting analysis - samples: " + juce::String(audioBuffer.getNumSamples()) +
             ", sampleRate: " + juce::String(audioSampleRate, 1) + "Hz");
  repaint();

  // Run detection on background thread to avoid UI blocking
  Async::runVoid([this]() {
    Log::debug("Upload::detectKey: Running key detection on background thread");
    auto detectedKey = keyDetector.detectKey(audioBuffer, audioSampleRate, audioBuffer.getNumChannels());

    Log::debug("Upload::detectKey: Detection complete - valid: " + juce::String(detectedKey.isValid() ? "yes" : "no") +
               (detectedKey.isValid() ? (", name: " + detectedKey.name + ", Camelot: " + detectedKey.camelot +
                                         ", confidence: " + juce::String(detectedKey.confidence, 2))
                                      : ""));

    // Map detected key to our key index
    int keyIndex = 0; // "Not set"
    if (detectedKey.isValid()) {
      // Try to find matching key in our list
      auto &keys = getMusicalKeys();
      for (size_t i = 1; i < keys.size(); ++i) {
        // Match by short name (Am, F#, etc.)
        if (keys[i].shortName.equalsIgnoreCase(detectedKey.shortName)) {
          keyIndex = static_cast<int>(i);
          Log::debug("Upload::detectKey: Matched key by shortName: " + keys[i].name);
          break;
        }
        // Also try matching by standard name prefix
        if (detectedKey.name.containsIgnoreCase(keys[i].shortName.replace("m", " Minor").replace("# ", "# /"))) {
          keyIndex = static_cast<int>(i);
          Log::debug("Upload::detectKey: Matched key by name prefix: " + keys[i].name);
          break;
        }
      }
    }

    // Update UI on message thread
    juce::MessageManager::callAsync([this, keyIndex, detectedKey]() {
      isDetectingKey = false;

      if (detectedKey.isValid()) {
        selectedKeyIndex = keyIndex;
        keyDetectionStatus = "Detected: " + detectedKey.name;
        if (detectedKey.confidence > 0.0f) {
          keyDetectionStatus += " (" + StringFormatter::formatConfidence(detectedKey.confidence) + " confidence)";
        }
        Log::info("Upload::detectKey: Key detected: " + detectedKey.name + " (Camelot: " + detectedKey.camelot +
                  "), confidence: " + juce::String(detectedKey.confidence, 2) +
                  ", mapped to index: " + juce::String(keyIndex));
      } else {
        keyDetectionStatus = "Could not detect key";
        Log::warn("Upload::detectKey: Could not detect key");
      }
      repaint();

      // Clear status after 3 seconds
      juce::Timer::callAfterDelay(3000, [this]() {
        keyDetectionStatus = "";
        repaint();
      });
    });
  });
}

void Upload::showKeyPicker() {
  Log::debug("Upload::showKeyPicker: Showing key picker menu");
  juce::PopupMenu menu;
  auto &keys = getMusicalKeys();

  for (size_t i = 0; i < keys.size(); ++i) {
    menu.addItem(static_cast<int>(i) + 1, keys[i].name, true, static_cast<int>(i) == selectedKeyIndex);
  }

  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                         keyDropdownArea.translated(getScreenX(), getScreenY())),
                     [this](int result) {
                       if (result > 0) {
                         auto &keysInner = getMusicalKeys();
                         int newIndex = result - 1;
                         Log::info(
                             "Upload::showKeyPicker: Key selected: " + keysInner[static_cast<size_t>(newIndex)].name +
                             " (index: " + juce::String(newIndex) + ")");
                         selectedKeyIndex = newIndex;
                         repaint();
                       }
                     });
}

void Upload::showGenrePicker() {
  Log::debug("Upload::showGenrePicker: Showing genre picker menu");
  juce::PopupMenu menu;
  auto &genres = getGenres();

  for (size_t i = 0; i < genres.size(); ++i) {
    menu.addItem(static_cast<int>(i) + 1, genres[i], true, static_cast<int>(i) == selectedGenreIndex);
  }

  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                         genreDropdownArea.translated(getScreenX(), getScreenY())),
                     [this](int result) {
                       if (result > 0) {
                         auto &genresInner = getGenres();
                         int newIndex = result - 1;
                         Log::info(
                             "Upload::showGenrePicker: Genre selected: " + genresInner[static_cast<size_t>(newIndex)] +
                             " (index: " + juce::String(newIndex) + ")");
                         selectedGenreIndex = newIndex;
                         repaint();
                       }
                     });
}

void Upload::showCommentAudiencePicker() {
  Log::debug("Upload::showCommentAudiencePicker: Showing comment audience "
             "picker menu");
  juce::PopupMenu menu;
  auto &audiences = getCommentAudiences();

  for (size_t i = 0; i < audiences.size(); ++i) {
    menu.addItem(static_cast<int>(i) + 1, audiences[i].displayName, true,
                 static_cast<int>(i) == selectedCommentAudienceIndex);
  }

  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                         commentAudienceArea.translated(getScreenX(), getScreenY())),
                     [this](int result) {
                       if (result > 0) {
                         auto &audiencesInner = getCommentAudiences();
                         int newIndex = result - 1;
                         Log::info("Upload::showCommentAudiencePicker: Comment audience selected: " +
                                   audiencesInner[static_cast<size_t>(newIndex)].displayName + " (" +
                                   audiencesInner[static_cast<size_t>(newIndex)].value + ")");
                         selectedCommentAudienceIndex = newIndex;
                         repaint();
                       }
                     });
}

void Upload::selectProjectFile() {
  Log::debug("Upload::selectProjectFile: Opening file chooser");

  // If file is already selected, clicking clears it
  if (includeProjectFile) {
    Log::info("Upload::selectProjectFile: Clearing selected project file");
    projectFile = juce::File();
    includeProjectFile = false;
    repaint();
    return;
  }

  // Open file chooser - use shared_ptr to allow copy in lambda
  auto chooser = std::make_shared<juce::FileChooser>(
      "Select Project File", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
      "*.als;*.alp;*.flp;*.logic;*.logicx;*.ptx;*.ptf;*.cpr;*.song;*.rpp;*."
      "bwproject");

  chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                       [this, chooser](const juce::FileChooser &fc) {
                         auto result = fc.getResult();

                         if (result.existsAsFile()) {
                           // Check file size (50MB max)
                           const int64 maxSize = 50 * 1024 * 1024;
                           if (result.getSize() > maxSize) {
                             Log::warn("Upload::selectProjectFile: File too large (max 50MB)");
                             juce::AlertWindow::showMessageBoxAsync(
                                 juce::MessageBoxIconType::WarningIcon, "File Too Large",
                                 "Project file must be under 50MB.\n\nYour file: " +
                                     juce::String(static_cast<double>(result.getSize()) / (1024.0 * 1024.0), 1) +
                                     " MB");
                             return;
                           }

                           projectFile = result;
                           includeProjectFile = true;
                           Log::info("Upload::selectProjectFile: Selected file: " + projectFile.getFileName() + " (" +
                                     juce::String(projectFile.getSize() / 1024) + " KB)");
                           repaint();
                         }
                       });
}

void Upload::cancelUpload() {
  Log::info("Upload::cancelUpload: Upload cancelled by user");

  // Cancel any pending progress timers
  if (progressTimer500ms != 0) {
    Async::cancelDelay(progressTimer500ms);
    progressTimer500ms = 0;
  }
  if (progressTimer1000ms != 0) {
    Async::cancelDelay(progressTimer1000ms);
    progressTimer1000ms = 0;
  }
  if (successDismissTimer != 0) {
    Async::cancelDelay(successDismissTimer);
    successDismissTimer = 0;
  }

  // Reset upload state if currently uploading
  if (uploadState == UploadState::Uploading) {
    uploadState = UploadState::Editing;
    uploadProgress = 0.0f;
    errorMessage = "";
    Log::debug("Upload::cancelUpload: Reset upload state to Editing");
    repaint();
  }

  // Call the cancel callback (which navigates back to recording)
  if (onCancel) {
    Log::debug("Upload::cancelUpload: Calling onCancel callback");
    onCancel();
  } else {
    Log::warn("Upload::cancelUpload: onCancel callback not set");
  }
}

void Upload::startUpload() {
  Log::info("Upload::startUpload: Starting upload process");

  if (filename.isEmpty()) {
    Log::warn("Upload::startUpload: Validation failed - filename is empty");
    errorMessage = "Please enter a filename";
    uploadState = UploadState::Error;
    repaint();
    return;
  }

  // Validate filename - no directory separators allowed
  if (filename.containsChar('/') || filename.containsChar('\\')) {
    Log::warn("Upload::startUpload: Validation failed - filename contains "
              "directory path");
    errorMessage = "Filename cannot contain directory paths";
    uploadState = UploadState::Error;
    repaint();
    return;
  }

  if (filename.length() > 255) {
    Log::warn("Upload::startUpload: Validation failed - filename too long");
    errorMessage = "Filename too long (max 255 characters)";
    uploadState = UploadState::Error;
    repaint();
    return;
  }

  if (audioBuffer.getNumSamples() == 0) {
    Log::warn("Upload::startUpload: Validation failed - no audio to upload");
    errorMessage = "No audio to upload";
    uploadState = UploadState::Error;
    repaint();
    return;
  }

  uploadState = UploadState::Uploading;
  uploadProgress = 0.1f; // Show initial progress
  errorMessage = "";
  Log::debug("Upload::startUpload: State changed to Uploading, progress: 10%");
  repaint();

  // Validate store is available
  jassert(appStore != nullptr);
  if (!appStore) {
    uploadState = UploadState::Error;
    errorMessage = "App store not initialized";
    repaint();
    return;
  }

  Log::info("Upload::startUpload: Delegating to AppStore");

  // Extract key and genre strings for store
  auto &keys = getMusicalKeys();
  auto keyStr = selectedKeyIndex > 0 && selectedKeyIndex < static_cast<int>(keys.size())
                    ? keys[static_cast<size_t>(selectedKeyIndex)].shortName
                    : "";
  auto &genres = getGenres();
  auto genreStr =
      selectedGenreIndex < static_cast<int>(genres.size()) ? genres[static_cast<size_t>(selectedGenreIndex)] : "";

  // Create post data object and call appStore->uploadPost which will handle the network request
  // The store will notify us via subscription with progress/success/error updates
  juce::DynamicObject::Ptr postData(new juce::DynamicObject());
  postData->setProperty("filename", filename);
  postData->setProperty("genre", genreStr);
  postData->setProperty("key", keyStr);
  postData->setProperty("bpm", bpm);

  // Create actual audio file from audioBuffer and write to temporary location
  // Use system temp directory to store the WAV file before upload
  juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
  juce::String audioFileName = filename.replaceCharacters(" ", "_") + ".wav";
  juce::File audioFile = tempDir.getChildFile(audioFileName);

  // Write audio buffer to WAV file
  if (audioBuffer.getNumSamples() > 0) {
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outStream(audioFile.createOutputStream());

    if (outStream) {
      juce::AudioFormatWriter::AudioFormatWriterOptions options;
      options.sampleRate = audioSampleRate;
      options.numChannels = static_cast<unsigned int>(audioBuffer.getNumChannels());
      options.bitsPerSample = 16;

      std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outStream.get(), options));

      if (writer) {
        outStream.release(); // Release ownership to writer
        writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples());
        writer->flush();

        Log::debug("Upload::startUpload: Audio file written to " + audioFile.getFullPathName());
      } else {
        Log::error("Upload::startUpload: Failed to create WAV writer");
        uploadState = UploadState::Error;
        errorMessage = "Failed to encode audio";
        repaint();
        return;
      }
    } else {
      Log::error("Upload::startUpload: Failed to create output stream for " + audioFile.getFullPathName());
      uploadState = UploadState::Error;
      errorMessage = "Failed to write audio file";
      repaint();
      return;
    }
  } else {
    Log::error("Upload::startUpload: Audio buffer is empty");
    uploadState = UploadState::Error;
    errorMessage = "No audio data to upload";
    repaint();
    return;
  }

  appStore->uploadPost(postData.get(), audioFile);

  // Store upload details for success preview
  lastUploadedFilename = filename;
  lastUploadedGenre = genreStr;
  lastUploadedBpm = bpm;

  // Auto-dismiss success after 3 seconds (longer to show success preview)
  // This will be called by store subscription when uploadState changes to Success
  // TODO: Phase 2 - Implement actual upload in UploadStore with NetworkClient integration
}

bool Upload::keyPressed(const juce::KeyPress &key) {
  if (activeField < 0)
    return false;

  // Handle special keys
  if (key == juce::KeyPress::escapeKey) {
    Log::debug("Upload::keyPressed: Escape key pressed, clearing field focus");
    activeField = -1;
    repaint();
    return true;
  }

  if (key == juce::KeyPress::tabKey) {
    int newField = (activeField + 1) % 2;
    Log::debug("Upload::keyPressed: Tab key pressed, switching field: " + juce::String(activeField) + " -> " +
               juce::String(newField));
    activeField = newField;
    repaint();
    return true;
  }

  if (key == juce::KeyPress::returnKey) {
    Log::debug("Upload::keyPressed: Return key pressed, clearing field focus");
    activeField = -1;
    repaint();
    return true;
  }

  if (activeField == 0) // Filename field
  {
    if (key == juce::KeyPress::backspaceKey) {
      if (!filename.isEmpty()) {
        filename = filename.dropLastCharacters(1);
        Log::debug("Upload::keyPressed: Backspace in filename field, new length: " + juce::String(filename.length()));
        repaint();
      }
      return true;
    }

    // Handle text input - get the character from the key
    juce::juce_wchar character = key.getTextCharacter();
    if (character >= 32 && character < 127) // Printable ASCII
    {
      // Block directory separators in filename
      if (character == '/' || character == '\\') {
        Log::debug("Upload::keyPressed: Directory separator blocked in filename");
        return true;
      }

      if (filename.length() < 255) // Max filename length
      {
        filename += juce::String::charToString(character);
        Log::debug("Upload::keyPressed: Character added to filename, new length: " + juce::String(filename.length()));
        repaint();
      } else {
        Log::debug("Upload::keyPressed: Filename max length (255) reached");
      }
      return true;
    }
  } else if (activeField == 1) // BPM field
  {
    if (key == juce::KeyPress::backspaceKey) {
      juce::String bpmStr = bpm > 0 ? juce::String(bpm, 1) : "";
      if (!bpmStr.isEmpty()) {
        bpmStr = bpmStr.dropLastCharacters(1);
        bpm = bpmStr.isEmpty() ? 0.0 : bpmStr.getDoubleValue();
        bpmFromDAW = false;
        Log::debug("Upload::keyPressed: Backspace in BPM field, new BPM: " + juce::String(bpm, 1));
        repaint();
      }
      return true;
    }

    // Handle numeric input
    juce::juce_wchar character = key.getTextCharacter();
    if ((character >= '0' && character <= '9') || character == '.') {
      juce::String bpmStr = bpm > 0 ? juce::String(bpm, 1) : "";
      bpmStr += juce::String::charToString(character);
      double newBpm = bpmStr.getDoubleValue();
      if (newBpm <= Constants::Audio::MAX_BPM) {
        bpm = newBpm;
        bpmFromDAW = false;
        Log::debug("Upload::keyPressed: BPM updated: " + juce::String(bpm, 1));
        repaint();
      } else {
        Log::debug("Upload::keyPressed: BPM exceeds max (" + juce::String(Constants::Audio::MAX_BPM) + ")");
      }
      return true;
    }
  }

  return false;
}

void Upload::focusGained(FocusChangeType /*cause*/) {
  // When component gains focus, activate filename field if nothing is active
  if (activeField < 0) {
    Log::debug("Upload::focusGained: Component gained focus, activating "
               "filename field");
    activeField = 0;
    repaint();
  }
}

void Upload::loadFromDraft(const juce::String &draftFilename, double draftBpm, int keyIdx, int genreIdx,
                           int commentIdx) {
  Log::info("Upload::loadFromDraft: Loading draft data - filename: \"" + draftFilename + "\", BPM: " +
            juce::String(draftBpm, 1) + ", key: " + juce::String(keyIdx) + ", genre: " + juce::String(genreIdx));

  filename = draftFilename;
  bpm = draftBpm;
  bpmFromDAW = false; // Draft BPM is manual
  selectedKeyIndex = keyIdx;
  selectedGenreIndex = genreIdx;
  selectedCommentAudienceIndex = commentIdx;

  // Ensure state is editing
  uploadState = UploadState::Editing;
  uploadProgress = 0.0f;
  errorMessage = "";
  activeField = -1;

  repaint();
}
