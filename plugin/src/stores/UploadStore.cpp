#include "UploadStore.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
UploadStore::UploadStore(NetworkClient *client) : networkClient(client) {
  Log::info("UploadStore: Initializing");
  setState(UploadState{});
}

//==============================================================================
void UploadStore::startUpload(const juce::String &filename, const juce::String &genre, const juce::String &key,
                              double bpm, const juce::AudioBuffer<float> &audioData, double sampleRate) {
  if (networkClient == nullptr) {
    Log::error("UploadStore: Network client not available");
    failUpload("Network client not initialized");
    return;
  }

  Log::info("UploadStore: Starting upload - " + filename);

  // Cache upload data
  uploadFilename = filename;
  uploadGenre = genre;
  uploadKey = key;
  uploadBpm = bpm;

  // Update state to uploading
  updateStatus(UploadState::Status::Uploading);

  // Simulate progress updates
  updateProgress(10.0f);

  // In a real implementation, would encode audio to MP3/WAV and upload to CDN
  Log::info("UploadStore: Upload initiated for " + juce::String(audioData.getNumSamples()) + " samples at " +
            juce::String(sampleRate, 0) + "Hz");
}

//==============================================================================
void UploadStore::updateProgress(float progressPercent) {
  auto state = getState();
  state.progress = juce::jlimit(0.0f, 100.0f, progressPercent);
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);

  Log::debug("UploadStore: Progress " + juce::String(state.progress, 1) + "%");
}

//==============================================================================
void UploadStore::completeUpload(const juce::String &postId) {
  Log::info("UploadStore: Upload complete - post ID: " + postId);

  auto state = getState();
  state.status = UploadState::Status::Success;
  state.progress = 100.0f;
  state.uploadedPostId = postId;
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

//==============================================================================
void UploadStore::failUpload(const juce::String &error) {
  Log::error("UploadStore: Upload failed - " + error);

  auto state = getState();
  state.status = UploadState::Status::Error;
  state.errorMessage = error;
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

//==============================================================================
void UploadStore::cancelUpload() {
  Log::info("UploadStore: Upload cancelled");

  auto state = getState();
  state.status = UploadState::Status::Idle;
  state.progress = 0.0f;
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

//==============================================================================
void UploadStore::reset() {
  Log::debug("UploadStore: Resetting");

  auto state = getState();
  state.status = UploadState::Status::Idle;
  state.progress = 0.0f;
  state.errorMessage = "";
  state.uploadedPostId = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);

  uploadFilename = "";
  uploadGenre = "";
  uploadKey = "";
  uploadBpm = 0.0;
}

//==============================================================================
void UploadStore::updateStatus(UploadState::Status newStatus) {
  auto state = getState();
  state.status = newStatus;
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);
}

} // namespace Stores
} // namespace Sidechain
