#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::uploadPost(const juce::var &postData, const juce::File &audioFile) {
  // Extract post metadata if available
  juce::String postId;
  juce::String filename;
  double bpm = 0.0;
  juce::String key;
  juce::String genre;
  double duration = 0.0;

  if (postData.isObject()) {
    const auto *obj = postData.getDynamicObject();
    if (obj) {
      postId = obj->getProperty("id").toString();
      filename = obj->getProperty("filename").toString();
      bpm = obj->getProperty("bpm");
      key = obj->getProperty("key").toString();
      genre = obj->getProperty("genre").toString();
      duration = obj->getProperty("duration");

      if (!postId.isEmpty()) {
        Util::logInfo("AppStore", "Starting upload for post ID: " + postId);
      }
    }
  }

  if (!networkClient) {
    Util::logError("AppStore", "Network client not available");
    UploadState newState = sliceManager.uploads->getState();
    newState.isUploading = false;
    newState.uploadError = "Network client not initialized";
    newState.progress = 0;
    sliceManager.uploads->setState(newState);
    return;
  }

  if (!audioFile.existsAsFile()) {
    Util::logError("AppStore", "Audio file does not exist: " + audioFile.getFullPathName());
    UploadState newState = sliceManager.uploads->getState();
    newState.isUploading = false;
    newState.uploadError = "Audio file not found";
    newState.progress = 0;
    sliceManager.uploads->setState(newState);
    return;
  }

  Util::logInfo("AppStore", "Starting upload - " + audioFile.getFileName());

  // Update state to uploading
  UploadState newState = sliceManager.uploads->getState();
  newState.isUploading = true;
  newState.progress = 10;
  newState.uploadError = "";
  newState.currentFileName = audioFile.getFileName();
  newState.startTime = juce::Time::getCurrentTime().toMilliseconds();
  sliceManager.uploads->setState(newState);

  // Load audio file and upload with metadata
  if (auto reader = std::unique_ptr<juce::AudioFormatReader>(
          juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(audioFile, true), true))) {

    // Create audio buffer from reader
    juce::AudioBuffer<float> audioBuffer(reader->numChannels, static_cast<int>(reader->lengthInSamples));
    reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    // Prepare upload metadata
    NetworkClient::AudioUploadMetadata metadata;
    metadata.filename = filename.isEmpty() ? audioFile.getFileName() : filename;
    metadata.bpm = bpm > 0 ? bpm : 0.0;
    metadata.key = key;
    metadata.genre = genre;
    metadata.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    metadata.sampleRate = static_cast<int>(reader->sampleRate);
    metadata.numChannels = reader->numChannels;
    metadata.daw = NetworkClient::detectDAWName();

    // Upload audio with metadata
    networkClient->uploadAudioWithMetadata(
        audioBuffer, reader->sampleRate, metadata,
        [this, postId](const auto &outcome) {
          if (outcome.isSuccess()) {
            Util::logInfo("AppStore", "Upload successful for post ID: " + postId);
            UploadState newState = sliceManager.uploads->getState();
            newState.isUploading = false;
            newState.progress = 100;
            newState.uploadError = "";
            sliceManager.uploads->setState(newState);
          } else {
            Util::logError("AppStore", "Upload failed: " + outcome.error());
            UploadState newState = sliceManager.uploads->getState();
            newState.isUploading = false;
            newState.progress = 0;
            newState.uploadError = outcome.error();
            sliceManager.uploads->setState(newState);
          }
        });
  } else {
    Util::logError("AppStore", "Failed to read audio file: " + audioFile.getFullPathName());
    UploadState newState = sliceManager.uploads->getState();
    newState.isUploading = false;
    newState.uploadError = "Failed to read audio file";
    newState.progress = 0;
    sliceManager.uploads->setState(newState);
  }
}

void AppStore::cancelUpload() {
  Util::logInfo("AppStore", "Upload cancelled");

  UploadState newState = sliceManager.uploads->getState();
  newState.isUploading = false;
  newState.progress = 0;
  newState.uploadError = "";
  newState.currentFileName = "";
  sliceManager.uploads->setState(newState);
}

} // namespace Stores
} // namespace Sidechain
