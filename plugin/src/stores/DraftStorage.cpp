#include "DraftStorage.h"
#include "../util/Json.h"
#include "../util/Log.h"

//==============================================================================
// Draft implementation
//==============================================================================

Draft Draft::fromJson(const juce::var &json) {
  Draft draft;

  if (!Json::isObject(json))
    return draft;

  draft.id = Json::getString(json, "id");
  draft.filename = Json::getString(json, "filename");
  draft.bpm = Json::getDouble(json, "bpm");
  draft.keyIndex = Json::getInt(json, "key_index");
  draft.genreIndex = Json::getInt(json, "genre_index");
  draft.commentAudienceIndex = Json::getInt(json, "comment_audience_index");
  draft.sampleRate = Json::getDouble(json, "sample_rate", 44100.0);
  draft.numSamples = Json::getInt(json, "num_samples");
  draft.numChannels = Json::getInt(json, "num_channels", 2);
  draft.midiData = Json::getObject(json, "midi_data");
  draft.audioFilePath = Json::getString(json, "audio_file_path");

  // Parse timestamps
  juce::String createdAtStr = Json::getString(json, "created_at");
  if (createdAtStr.isNotEmpty())
    draft.createdAt = juce::Time::fromISO8601(createdAtStr);

  juce::String updatedAtStr = Json::getString(json, "updated_at");
  if (updatedAtStr.isNotEmpty())
    draft.updatedAt = juce::Time::fromISO8601(updatedAtStr);

  return draft;
}

juce::var Draft::toJson() const {
  auto *obj = new juce::DynamicObject();

  obj->setProperty("id", id);
  obj->setProperty("filename", filename);
  obj->setProperty("bpm", bpm);
  obj->setProperty("key_index", keyIndex);
  obj->setProperty("genre_index", genreIndex);
  obj->setProperty("comment_audience_index", commentAudienceIndex);
  obj->setProperty("sample_rate", sampleRate);
  obj->setProperty("num_samples", numSamples);
  obj->setProperty("num_channels", numChannels);
  obj->setProperty("midi_data", midiData);
  obj->setProperty("audio_file_path", audioFilePath);
  obj->setProperty("created_at", createdAt.toISO8601(true));
  obj->setProperty("updated_at", updatedAt.toISO8601(true));

  return juce::var(obj);
}

//==============================================================================
// DraftStorage implementation
//==============================================================================

DraftStorage::DraftStorage() {
  // Get platform-specific app data directory
#if JUCE_LINUX
  draftsDir =
      juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".local/share/Sidechain/drafts");
#elif JUCE_MAC
  draftsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Sidechain/drafts");
#else // Windows
  draftsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Sidechain/drafts");
#endif

  ensureDraftsDirectory();
  Log::info("DraftStorage: Initialized with directory: " + draftsDir.getFullPathName());
}

DraftStorage::~DraftStorage() {}

//==============================================================================

void DraftStorage::ensureDraftsDirectory() {
  if (!draftsDir.exists()) {
    auto result = draftsDir.createDirectory();
    if (result.failed()) {
      Log::error("DraftStorage: Failed to create drafts directory: " + result.getErrorMessage());
    } else {
      Log::info("DraftStorage: Created drafts directory");
    }
  }
}

juce::String DraftStorage::generateDraftId() {
  return juce::Uuid().toString();
}

juce::File DraftStorage::getAudioFile(const juce::String &draftId) const {
  return draftsDir.getChildFile(draftId + ".wav");
}

juce::File DraftStorage::getMetadataFile(const juce::String &draftId) const {
  return draftsDir.getChildFile(draftId + ".json");
}

juce::File DraftStorage::getDraftsDirectory() const {
  return draftsDir;
}

//==============================================================================

bool DraftStorage::writeAudioFile(const juce::File &file, const juce::AudioBuffer<float> &buffer, double sampleRate) {
  if (buffer.getNumSamples() == 0) {
    Log::warn("DraftStorage::writeAudioFile: Empty buffer, skipping");
    return false;
  }

  // Create WAV writer
  juce::WavAudioFormat wavFormat;
  std::unique_ptr<juce::OutputStream> fileStream(file.createOutputStream());

  if (fileStream == nullptr) {
    Log::error("DraftStorage::writeAudioFile: Failed to create output stream for " + file.getFullPathName());
    return false;
  }

  auto writerOptions = juce::AudioFormatWriterOptions()
                           .withSampleRate(sampleRate)
                           .withNumChannels(buffer.getNumChannels())
                           .withBitsPerSample(16);
  auto writer = wavFormat.createWriterFor(fileStream, writerOptions);

  if (writer == nullptr) {
    Log::error("DraftStorage::writeAudioFile: Failed to create WAV writer");
    return false;
  }

  // Ownership of fileStream is transferred to writer when successful

  bool success = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

  if (success) {
    Log::debug("DraftStorage::writeAudioFile: Wrote " + juce::String(buffer.getNumSamples()) + " samples to " +
               file.getFileName());
  } else {
    Log::error("DraftStorage::writeAudioFile: Failed to write audio data");
  }

  return success;
}

bool DraftStorage::readAudioFile(const juce::File &file, juce::AudioBuffer<float> &buffer, double &sampleRate) {
  if (!file.existsAsFile()) {
    Log::warn("DraftStorage::readAudioFile: File not found: " + file.getFullPathName());
    return false;
  }

  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

  if (reader == nullptr) {
    Log::error("DraftStorage::readAudioFile: Failed to create reader for " + file.getFullPathName());
    return false;
  }

  sampleRate = reader->sampleRate;
  buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));

  bool success = reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

  if (success) {
    Log::debug("DraftStorage::readAudioFile: Read " + juce::String(buffer.getNumSamples()) + " samples from " +
               file.getFileName());
  } else {
    Log::error("DraftStorage::readAudioFile: Failed to read audio data");
  }

  return success;
}

//==============================================================================

Draft DraftStorage::saveDraft(const Draft &draft, const juce::AudioBuffer<float> &audioBuffer) {
  ensureDraftsDirectory();

  Draft savedDraft = draft;

  // Generate ID if not provided
  if (savedDraft.id.isEmpty()) {
    savedDraft.id = generateDraftId();
    savedDraft.createdAt = juce::Time::getCurrentTime();
  }

  savedDraft.updatedAt = juce::Time::getCurrentTime();

  // Update audio metadata
  savedDraft.numSamples = audioBuffer.getNumSamples();
  savedDraft.numChannels = audioBuffer.getNumChannels();

  // Save audio file
  juce::File audioFile = getAudioFile(savedDraft.id);
  if (!writeAudioFile(audioFile, audioBuffer, savedDraft.sampleRate)) {
    Log::error("DraftStorage::saveDraft: Failed to save audio file");
    return Draft(); // Return empty draft on failure
  }

  savedDraft.audioFilePath = audioFile.getFullPathName();

  // Save metadata
  juce::File metadataFile = getMetadataFile(savedDraft.id);
  juce::String jsonStr = juce::JSON::toString(savedDraft.toJson(), true);

  if (!metadataFile.replaceWithText(jsonStr)) {
    Log::error("DraftStorage::saveDraft: Failed to save metadata file");
    audioFile.deleteFile(); // Clean up audio file
    return Draft();
  }

  Log::info("DraftStorage::saveDraft: Saved draft " + savedDraft.id + " (" + savedDraft.getFormattedDuration() + ")");

  return savedDraft;
}

Draft DraftStorage::loadDraft(const juce::String &draftId, juce::AudioBuffer<float> &audioBuffer) {
  if (draftId.isEmpty()) {
    Log::warn("DraftStorage::loadDraft: Empty draft ID");
    return Draft();
  }

  // Load metadata
  juce::File metadataFile = getMetadataFile(draftId);
  if (!metadataFile.existsAsFile()) {
    Log::warn("DraftStorage::loadDraft: Metadata file not found for " + draftId);
    return Draft();
  }

  juce::String jsonStr = metadataFile.loadFileAsString();
  juce::var json = juce::JSON::parse(jsonStr);

  if (!Json::isObject(json)) {
    Log::error("DraftStorage::loadDraft: Invalid JSON in metadata file");
    return Draft();
  }

  Draft draft = Draft::fromJson(json);

  // Load audio
  juce::File audioFile = getAudioFile(draftId);
  double sampleRate = draft.sampleRate;

  if (!readAudioFile(audioFile, audioBuffer, sampleRate)) {
    Log::error("DraftStorage::loadDraft: Failed to load audio file");
    return Draft();
  }

  draft.sampleRate = sampleRate;

  Log::info("DraftStorage::loadDraft: Loaded draft " + draftId);
  return draft;
}

juce::Array<Draft> DraftStorage::getAllDrafts() {
  juce::Array<Draft> drafts;

  if (!draftsDir.exists())
    return drafts;

  // Find all .json metadata files
  juce::Array<juce::File> metadataFiles;
  draftsDir.findChildFiles(metadataFiles, juce::File::findFiles, false, "*.json");

  for (const auto &file : metadataFiles) {
    // Skip auto-recovery file
    if (file.getFileNameWithoutExtension() == AUTO_RECOVERY_ID)
      continue;

    juce::String jsonStr = file.loadFileAsString();
    juce::var json = juce::JSON::parse(jsonStr);

    if (Json::isObject(json)) {
      Draft draft = Draft::fromJson(json);
      if (draft.id.isNotEmpty())
        drafts.add(draft);
    }
  }

  // Sort by updatedAt (newest first)
  std::sort(drafts.begin(), drafts.end(), [](const Draft &a, const Draft &b) { return a.updatedAt > b.updatedAt; });

  Log::debug("DraftStorage::getAllDrafts: Found " + juce::String(drafts.size()) + " drafts");
  return drafts;
}

bool DraftStorage::deleteDraft(const juce::String &draftId) {
  if (draftId.isEmpty())
    return false;

  bool success = true;

  // Delete audio file
  juce::File audioFile = getAudioFile(draftId);
  if (audioFile.existsAsFile()) {
    if (!audioFile.deleteFile()) {
      Log::warn("DraftStorage::deleteDraft: Failed to delete audio file");
      success = false;
    }
  }

  // Delete metadata file
  juce::File metadataFile = getMetadataFile(draftId);
  if (metadataFile.existsAsFile()) {
    if (!metadataFile.deleteFile()) {
      Log::warn("DraftStorage::deleteDraft: Failed to delete metadata file");
      success = false;
    }
  }

  if (success)
    Log::info("DraftStorage::deleteDraft: Deleted draft " + draftId);

  return success;
}

int DraftStorage::getDraftCount() {
  return getAllDrafts().size();
}

//==============================================================================
// Auto-recovery

void DraftStorage::saveAutoRecoveryDraft(const Draft &draft, const juce::AudioBuffer<float> &audioBuffer) {
  Draft recoveryDraft = draft;
  recoveryDraft.id = AUTO_RECOVERY_ID;

  // Use internal save logic but don't update timestamps
  ensureDraftsDirectory();

  recoveryDraft.numSamples = audioBuffer.getNumSamples();
  recoveryDraft.numChannels = audioBuffer.getNumChannels();
  recoveryDraft.updatedAt = juce::Time::getCurrentTime();

  juce::File audioFile = getAudioFile(AUTO_RECOVERY_ID);
  if (!writeAudioFile(audioFile, audioBuffer, recoveryDraft.sampleRate)) {
    Log::error("DraftStorage::saveAutoRecoveryDraft: Failed to save audio");
    return;
  }

  recoveryDraft.audioFilePath = audioFile.getFullPathName();

  juce::File metadataFile = getMetadataFile(AUTO_RECOVERY_ID);
  juce::String jsonStr = juce::JSON::toString(recoveryDraft.toJson(), true);

  if (!metadataFile.replaceWithText(jsonStr)) {
    Log::error("DraftStorage::saveAutoRecoveryDraft: Failed to save metadata");
    return;
  }

  Log::debug("DraftStorage::saveAutoRecoveryDraft: Saved auto-recovery draft");
}

bool DraftStorage::hasAutoRecoveryDraft() {
  return getMetadataFile(AUTO_RECOVERY_ID).existsAsFile() && getAudioFile(AUTO_RECOVERY_ID).existsAsFile();
}

Draft DraftStorage::loadAutoRecoveryDraft(juce::AudioBuffer<float> &audioBuffer) {
  Draft draft = loadDraft(AUTO_RECOVERY_ID, audioBuffer);

  // Clear the ID so it becomes a new draft when saved
  if (draft.hasAudio()) {
    draft.id = "";
    Log::info("DraftStorage::loadAutoRecoveryDraft: Loaded auto-recovery draft");
  }

  return draft;
}

void DraftStorage::clearAutoRecoveryDraft() {
  getAudioFile(AUTO_RECOVERY_ID).deleteFile();
  getMetadataFile(AUTO_RECOVERY_ID).deleteFile();
  Log::debug("DraftStorage::clearAutoRecoveryDraft: Cleared auto-recovery draft");
}

juce::int64 DraftStorage::getTotalStorageUsed() {
  juce::int64 totalSize = 0;

  if (!draftsDir.exists())
    return totalSize;

  juce::Array<juce::File> files;
  draftsDir.findChildFiles(files, juce::File::findFiles, false);

  for (const auto &file : files) {
    totalSize += file.getSize();
  }

  return totalSize;
}
