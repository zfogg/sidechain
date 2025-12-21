#include "MidiChallengeSubmission.h"
#include "../../network/NetworkClient.h"
#include "../../util/Async.h"
#include "../../util/Json.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../recording/Upload.h"
#include "core/PluginProcessor.h"

// ==============================================================================
MidiChallengeSubmission::MidiChallengeSubmission(SidechainAudioProcessor &processor, NetworkClient &network,
                                                 Sidechain::Stores::AppStore *store)
    : AppStoreComponent(
          store,
          [store](auto cb) { return store ? store->subscribeToChallenges(cb) : std::function<void()>([]() {}); }),
      audioProcessor(processor), networkClient(network) {
  Log::info("MidiChallengeSubmission: Initializing");

  // Create wrapped Upload component
  uploadComponent = std::make_unique<Upload>(processor, network, store);
  uploadComponent->onUploadCompleteWithPostId = [this](const juce::String &postId) {
    // After upload completes, submit to challenge
    Log::info("MidiChallengeSubmission: Upload complete with post ID: " + postId);

    if (!postId.isEmpty() && !challenge.id.isEmpty()) {
      // Submit the post to the challenge
      networkClient.submitMIDIChallengeEntry(
          challenge.id, "", postId, midiData, "",
          [this](const auto &outcome) {
            if (outcome.isSuccess()) {
              Log::info("MidiChallengeSubmission: Challenge submission successful");
              submissionState = SubmissionState::Success;
            } else {
              Log::error("MidiChallengeSubmission: Challenge submission failed: " + outcome.getError());
              submissionState = SubmissionState::Error;
              errorMessage = outcome.getError();
            }
            repaint();
          });
    } else {
      Log::warn("MidiChallengeSubmission: Missing post ID or challenge ID");
      submissionState = SubmissionState::Error;
      errorMessage = "Could not submit to challenge - missing post ID";
      repaint();
    }
  };
  uploadComponent->onCancel = [this]() {
    if (onBackPressed)
      onBackPressed();
  };
  addAndMakeVisible(uploadComponent.get());
}

MidiChallengeSubmission::~MidiChallengeSubmission() {
  Log::debug("MidiChallengeSubmission: Destroying");
}

// ==============================================================================
void MidiChallengeSubmission::setChallenge(const Sidechain::MIDIChallenge &ch) {
  challenge = ch;
  reset();
  repaint();
}

void MidiChallengeSubmission::setAudioToUpload(const juce::AudioBuffer<float> &audio, double sampleRate,
                                               const juce::var &midi) {
  audioBuffer = audio;
  audioSampleRate = sampleRate;
  midiData = midi;

  // Pass to upload component
  if (uploadComponent)
    uploadComponent->setAudioToUpload(audio, sampleRate, midi);

  // Validate constraints with current values
  double duration = audio.getNumSamples() / sampleRate;
  double bpm = audioProcessor.isBPMAvailable() ? audioProcessor.getCurrentBPM() : 0.0;
  juce::String key = ""; // Will be set from upload form
  validateConstraints(bpm, key, midi, duration);

  repaint();
}

void MidiChallengeSubmission::reset() {
  submissionState = SubmissionState::Editing;
  errorMessage = "";
  bpmCheck = ConstraintCheck();
  keyCheck = ConstraintCheck();
  scaleCheck = ConstraintCheck();
  noteCountCheck = ConstraintCheck();
  durationCheck = ConstraintCheck();
}

// ==============================================================================
// AppStoreComponent virtual methods

void MidiChallengeSubmission::onAppStateChanged(const Sidechain::Stores::ChallengeState &state) {
  // Update UI when challenge state changes in the store
  Log::debug("MidiChallengeSubmission: Handling challenge state change");

  // If the current challenge was updated in the store, refresh its data
  if (!challenge.id.isEmpty() && !state.challenges.empty()) {
    for (const auto &ch : state.challenges) {
      if (ch && ch->id == challenge.id) {
        challenge = *ch; // Dereference shared_ptr
        repaint();
        break;
      }
    }
  }
}

// ==============================================================================
void MidiChallengeSubmission::paint(juce::Graphics &g) {
  // Background
  g.fillAll(SidechainColors::background());

  // Header
  drawHeader(g);

  // Content area
  auto contentBounds = getContentBounds();

  // Challenge info
  drawChallengeInfo(g, contentBounds);

  // Constraint checklist
  drawConstraintChecklist(g, contentBounds);

  // Upload component (will draw itself)
  // We'll position it in resized

  // Submit button
  if (submissionState == SubmissionState::Editing || submissionState == SubmissionState::Validating) {
    drawSubmitButton(g, contentBounds);
  } else if (submissionState == SubmissionState::Success) {
    drawSuccessState(g, contentBounds);
  } else if (submissionState == SubmissionState::Error) {
    drawErrorState(g, contentBounds);
  }
}

void MidiChallengeSubmission::resized() {
  auto contentBounds = getContentBounds();

  // Calculate heights
  int headerHeight = 60;
  int challengeInfoHeight = 120;
  int checklistHeight = 200;
  int submitButtonHeight = 50;

  int uploadHeight =
      getHeight() - headerHeight - challengeInfoHeight - checklistHeight - submitButtonHeight - 40; // padding

  // Position upload component
  if (uploadComponent) {
    auto uploadBounds = contentBounds.removeFromTop(uploadHeight);
    uploadBounds.translate(0, headerHeight + challengeInfoHeight + checklistHeight + 20);
    uploadComponent->setBounds(uploadBounds);
  }
}

void MidiChallengeSubmission::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onBackPressed)
      onBackPressed();
    return;
  }

  // Submit button - the upload component will handle submission once upload completes
  if (submissionState == SubmissionState::Editing && allConstraintsPassed() && getSubmitButtonBounds().contains(pos)) {
    Log::info("MidiChallengeSubmission: Submit button clicked");
    submissionState = SubmissionState::Validating;
    repaint();

    // Validate one more time
    double duration = audioBuffer.getNumSamples() / audioSampleRate;
    double bpm = audioProcessor.isBPMAvailable() ? audioProcessor.getCurrentBPM() : 0.0;
    // Get key from upload component - would need to expose this
    juce::String key = "";
    validateConstraints(bpm, key, midiData, duration);

    if (allConstraintsPassed()) {
      // Trigger upload, which will submit to challenge via the onUploadCompleteWithPostId callback
      Log::info("MidiChallengeSubmission: All constraints passed, uploading...");
    }
  }
}

// ==============================================================================
void MidiChallengeSubmission::drawHeader(juce::Graphics &g) {
  auto bounds = juce::Rectangle<int>(0, 0, getWidth(), 60);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRect(bounds);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("Submit to Challenge", bounds.removeFromLeft(getWidth() - 100), juce::Justification::centredLeft);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(SidechainColors::textPrimary());
  g.setFont(16.0f);
  g.drawText("←", backBounds, juce::Justification::centred);
}

void MidiChallengeSubmission::drawChallengeInfo(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto infoBounds = bounds.removeFromTop(120).reduced(16, 8);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(infoBounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(infoBounds.toFloat(), 8.0f, 1.0f);

  auto contentBounds = infoBounds.reduced(12, 12);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)).boldened());
  auto titleBounds = contentBounds.removeFromTop(24);
  g.drawText(challenge.title, titleBounds, juce::Justification::centredLeft);

  // Description
  if (challenge.description.isNotEmpty()) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(12.0f);
    auto descBounds = contentBounds.removeFromTop(50);
    g.drawText(challenge.description, descBounds, juce::Justification::topLeft, true);
  }
}

void MidiChallengeSubmission::drawConstraintChecklist(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  auto checklistBounds = bounds.removeFromTop(200).reduced(16, 8);

  // Background
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(checklistBounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(checklistBounds.toFloat(), 8.0f, 1.0f);

  auto contentBounds = checklistBounds.reduced(12, 12);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
  auto titleBounds = contentBounds.removeFromTop(24);
  g.drawText("Constraint Checklist", titleBounds, juce::Justification::centredLeft);

  contentBounds.removeFromTop(8);

  // Draw each constraint check
  int lineHeight = 24;
  int y = contentBounds.getY();

  // BPM
  if (challenge.constraints.bpmMin > 0 || challenge.constraints.bpmMax > 0) {
    juce::String text = "BPM: ";
    if (challenge.constraints.bpmMin > 0 && challenge.constraints.bpmMax > 0)
      text += juce::String(challenge.constraints.bpmMin) + "-" + juce::String(challenge.constraints.bpmMax);
    else if (challenge.constraints.bpmMin > 0)
      text += "≥" + juce::String(challenge.constraints.bpmMin);
    else
      text += "≤" + juce::String(challenge.constraints.bpmMax);

    drawConstraintItem(g, juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth(), lineHeight), text,
                       bpmCheck);
    y += lineHeight + 4;
  }

  // Key
  if (challenge.constraints.key.isNotEmpty()) {
    drawConstraintItem(g, juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth(), lineHeight),
                       "Key: " + challenge.constraints.key, keyCheck);
    y += lineHeight + 4;
  }

  // Scale
  if (challenge.constraints.scale.isNotEmpty()) {
    drawConstraintItem(g, juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth(), lineHeight),
                       "Scale: " + challenge.constraints.scale, scaleCheck);
    y += lineHeight + 4;
  }

  // Note count
  if (challenge.constraints.noteCountMin > 0 || challenge.constraints.noteCountMax > 0) {
    juce::String text = "Note Count: ";
    if (challenge.constraints.noteCountMin > 0 && challenge.constraints.noteCountMax > 0)
      text += juce::String(challenge.constraints.noteCountMin) + "-" + juce::String(challenge.constraints.noteCountMax);
    else if (challenge.constraints.noteCountMin > 0)
      text += "≥" + juce::String(challenge.constraints.noteCountMin);
    else
      text += "≤" + juce::String(challenge.constraints.noteCountMax);

    drawConstraintItem(g, juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth(), lineHeight), text,
                       noteCountCheck);
    y += lineHeight + 4;
  }

  // Duration
  if (challenge.constraints.durationMin > 0 || challenge.constraints.durationMax > 0) {
    juce::String text = "Duration: ";
    if (challenge.constraints.durationMin > 0 && challenge.constraints.durationMax > 0)
      text += juce::String(challenge.constraints.durationMin, 1) + "-" +
              juce::String(challenge.constraints.durationMax, 1) + "s";
    else if (challenge.constraints.durationMin > 0)
      text += "≥" + juce::String(challenge.constraints.durationMin, 1) + "s";
    else
      text += "≤" + juce::String(challenge.constraints.durationMax, 1) + "s";

    drawConstraintItem(g, juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth(), lineHeight), text,
                       durationCheck);
  }
}

void MidiChallengeSubmission::drawConstraintItem(juce::Graphics &g, juce::Rectangle<int> bounds,
                                                 const juce::String &text, const ConstraintCheck &check) {
  // Checkmark or X
  auto iconBounds = bounds.removeFromLeft(24);
  g.setColour(check.passed ? SidechainColors::success() : SidechainColors::error());
  g.setFont(14.0f);
  g.drawText(check.passed ? "[OK]" : "[X]", iconBounds, juce::Justification::centred);

  // Text
  g.setColour(SidechainColors::textPrimary());
  g.setFont(13.0f);
  g.drawText(text, bounds.removeFromLeft(bounds.getWidth() - 200), juce::Justification::centredLeft);

  // Message
  if (check.message.isNotEmpty()) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(11.0f);
    g.drawText(check.message, bounds, juce::Justification::centredRight);
  }
}

void MidiChallengeSubmission::drawSubmitButton(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  // Use provided bounds for responsive button layout
  // Center the button within the available space
  auto buttonBounds = bounds.withSizeKeepingCentre(120, 40);
  bool isHovered = buttonBounds.contains(getMouseXYRelative());
  bool isEnabled = allConstraintsPassed() && submissionState == SubmissionState::Editing;

  auto bgColor = isEnabled ? (isHovered ? SidechainColors::coralPink().brighter(0.2f) : SidechainColors::coralPink())
                           : SidechainColors::backgroundLight();

  g.setColour(bgColor);
  g.fillRoundedRectangle(buttonBounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(buttonBounds.toFloat(), 8.0f, 1.0f);

  // Text
  g.setColour(isEnabled ? SidechainColors::textPrimary() : SidechainColors::textMuted());
  g.setFont(16.0f);
  juce::String buttonText = submissionState == SubmissionState::Validating ? "Validating..." : "Submit Entry";
  g.drawText(buttonText, buttonBounds, juce::Justification::centred);
}

void MidiChallengeSubmission::drawSuccessState(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  g.setColour(SidechainColors::textPrimary());
  g.setFont(18.0f);
  g.drawText("Entry submitted successfully!", bounds, juce::Justification::centred);
}

void MidiChallengeSubmission::drawErrorState(juce::Graphics &g, juce::Rectangle<int> &bounds) {
  g.setColour(SidechainColors::error());
  g.setFont(14.0f);
  g.drawText(errorMessage, bounds, juce::Justification::centred);
}

// ==============================================================================
juce::Rectangle<int> MidiChallengeSubmission::getBackButtonBounds() const {
  return juce::Rectangle<int>(16, 0, 50, 60);
}

juce::Rectangle<int> MidiChallengeSubmission::getSubmitButtonBounds() const {
  auto contentBounds = getContentBounds();
  return contentBounds.removeFromBottom(50).reduced(16, 8);
}

juce::Rectangle<int> MidiChallengeSubmission::getContentBounds() const {
  return juce::Rectangle<int>(0, 60, getWidth(), getHeight() - 60);
}

juce::Rectangle<int> MidiChallengeSubmission::getUploadComponentBounds() const {
  return getContentBounds();
}

// ==============================================================================
void MidiChallengeSubmission::validateConstraints(double bpm, const juce::String &key, const juce::var &midi,
                                                  double durationSeconds) {
  // Reset all checks
  bpmCheck = ConstraintCheck();
  keyCheck = ConstraintCheck();
  scaleCheck = ConstraintCheck();
  noteCountCheck = ConstraintCheck();
  durationCheck = ConstraintCheck();

  // BPM check
  if (challenge.constraints.bpmMin > 0 || challenge.constraints.bpmMax > 0) {
    if (bpm <= 0) {
      bpmCheck.passed = false;
      bpmCheck.message = "BPM not set";
    } else if (challenge.constraints.bpmMin > 0 && bpm < challenge.constraints.bpmMin) {
      bpmCheck.passed = false;
      bpmCheck.message = "Too slow";
    } else if (challenge.constraints.bpmMax > 0 && bpm > challenge.constraints.bpmMax) {
      bpmCheck.passed = false;
      bpmCheck.message = "Too fast";
    } else {
      bpmCheck.passed = true;
    }
  } else {
    bpmCheck.passed = true; // No constraint
  }

  // Key check
  if (challenge.constraints.key.isNotEmpty()) {
    if (key.isEmpty()) {
      keyCheck.passed = false;
      keyCheck.message = "Key not set";
    } else {
      // Simple key matching (could be improved)
      juce::String normalizedKey = key.toUpperCase().trim();
      juce::String normalizedRequired = challenge.constraints.key.toUpperCase().trim();
      keyCheck.passed = normalizedKey == normalizedRequired || normalizedKey.startsWith(normalizedRequired);
      if (!keyCheck.passed)
        keyCheck.message = "Doesn't match";
    }
  } else {
    keyCheck.passed = true; // No constraint
  }

  // Scale check (simplified - would need proper scale validation)
  if (challenge.constraints.scale.isNotEmpty()) {
    scaleCheck.passed = checkMIDIScale(midi, challenge.constraints.scale);
    if (!scaleCheck.passed)
      scaleCheck.message = "Notes outside scale";
  } else {
    scaleCheck.passed = true; // No constraint
  }

  // Note count check
  if (challenge.constraints.noteCountMin > 0 || challenge.constraints.noteCountMax > 0) {
    int noteCount = countMIDINotes(midi);
    if (challenge.constraints.noteCountMin > 0 && noteCount < challenge.constraints.noteCountMin) {
      noteCountCheck.passed = false;
      noteCountCheck.message = "Too few notes";
    } else if (challenge.constraints.noteCountMax > 0 && noteCount > challenge.constraints.noteCountMax) {
      noteCountCheck.passed = false;
      noteCountCheck.message = "Too many notes";
    } else {
      noteCountCheck.passed = true;
    }
  } else {
    noteCountCheck.passed = true; // No constraint
  }

  // Duration check
  if (challenge.constraints.durationMin > 0 || challenge.constraints.durationMax > 0) {
    if (durationSeconds <= 0) {
      durationCheck.passed = false;
      durationCheck.message = "Duration unknown";
    } else if (challenge.constraints.durationMin > 0 && durationSeconds < challenge.constraints.durationMin) {
      durationCheck.passed = false;
      durationCheck.message = "Too short";
    } else if (challenge.constraints.durationMax > 0 && durationSeconds > challenge.constraints.durationMax) {
      durationCheck.passed = false;
      durationCheck.message = "Too long";
    } else {
      durationCheck.passed = true;
    }
  } else {
    durationCheck.passed = true; // No constraint
  }
}

bool MidiChallengeSubmission::allConstraintsPassed() const {
  return bpmCheck.passed && keyCheck.passed && scaleCheck.passed && noteCountCheck.passed && durationCheck.passed;
}

int MidiChallengeSubmission::countMIDINotes(const juce::var &midi) const {
  if (midi.isVoid() || !midi.hasProperty("events"))
    return 0;

  auto events = midi["events"];
  if (!events.isArray())
    return 0;

  int noteCount = 0;
  for (int i = 0; i < events.size(); ++i) {
    auto event = events[i];
    if (event.hasProperty("type")) {
      juce::String type = Json::getString(event, "type");
      if (type == "note_on" || type == "noteOn")
        noteCount++;
    }
  }
  return noteCount;
}

bool MidiChallengeSubmission::checkMIDIScale(const juce::var &midi, const juce::String &requiredScale) const {
  // Validate MIDI against the required scale
  if (midi.isVoid() || !midi.hasProperty("events")) {
    return false;
  }

  auto events = midi.getProperty("events", juce::var());
  if (!events.isArray()) {
    return false;
  }

  // Define scale note mappings (C=0, C#=1, D=2, etc.)
  // Common scales: Major, Minor, Pentatonic, Blues, Dorian, Phrygian, etc.
  std::map<juce::String, std::set<int>> scaleNotes;
  scaleNotes["Major"] = {0, 2, 4, 5, 7, 9, 11};    // Ionian
  scaleNotes["Minor"] = {0, 2, 3, 5, 7, 8, 10};    // Aeolian
  scaleNotes["Pentatonic"] = {0, 2, 4, 7, 9};      // Major Pentatonic
  scaleNotes["Blues"] = {0, 3, 5, 6, 7, 10};       // Blues
  scaleNotes["Dorian"] = {0, 2, 3, 5, 7, 9, 10};   // Dorian
  scaleNotes["Phrygian"] = {0, 1, 3, 5, 7, 8, 10}; // Phrygian

  // Get the allowed notes for this scale (if scale is recognized)
  auto it = scaleNotes.find(requiredScale);
  if (it == scaleNotes.end()) {
    Log::warn("MidiChallengeSubmission: Unknown scale: " + requiredScale);
    return true; // Unknown scale - accept all notes
  }

  auto &allowedNotes = it->second;

  // Check each MIDI note event
  for (int i = 0; i < events.size(); ++i) {
    auto event = events[i];
    if (event.hasProperty("type")) {
      juce::String type = Json::getString(event, "type");
      if (type == "note_on" || type == "noteOn") {
        if (event.hasProperty("note")) {
          int midiNote = static_cast<int>(event.getProperty("note", juce::var(0)));
          int noteInScale = midiNote % 12; // Normalize to 0-11
          if (allowedNotes.find(noteInScale) == allowedNotes.end()) {
            Log::warn("MidiChallengeSubmission: MIDI note " + juce::String(midiNote) + " (scale degree " +
                      juce::String(noteInScale) + ") is not in scale " + requiredScale);
            return false;
          }
        }
      }
    }
  }

  Log::info("MidiChallengeSubmission: All MIDI notes are valid for scale " + requiredScale);
  return true;
}

void MidiChallengeSubmission::submitEntry(const juce::String &postId, const juce::String &audioUrl) {
  submissionState = SubmissionState::Submitting;
  repaint();

  juce::String midiPatternId = "";
  if (!midiData.isVoid() && midiData.hasProperty("events")) {
    // TODO: Upload MIDI pattern first, get ID
  }

  networkClient.submitMIDIChallengeEntry(challenge.id, audioUrl, postId, midiData, midiPatternId,
                                         [this](Outcome<juce::var> result) {
                                           juce::MessageManager::callAsync([this, result]() {
                                             if (result.isOk()) {
                                               submissionState = SubmissionState::Success;
                                               if (onSubmissionComplete)
                                                 onSubmissionComplete();
                                             } else {
                                               submissionState = SubmissionState::Error;
                                               errorMessage = "Submission failed: " + result.getError();
                                             }
                                             repaint();
                                           });
                                         });
}
