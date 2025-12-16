#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::uploadPost(const juce::var &postData, const juce::File &audioFile) {
  if (!networkClient) {
    Log::error("AppStore: Network client not available");
    updateUploadState([](UploadState &state) {
      state.status = UploadState::Status::Error;
      state.errorMessage = "Network client not initialized";
    });
    return;
  }

  Log::info("AppStore: Starting upload - " + audioFile.getFileName());

  // Update state to uploading
  updateUploadState([](UploadState &state) {
    state.status = UploadState::Status::Uploading;
    state.progress = 10.0f;
    state.errorMessage = "";
    state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  // In a real implementation, would encode audio to MP3/WAV and upload to CDN
  Log::info("AppStore: Upload initiated for " + audioFile.getFullPathName());
}

void AppStore::cancelUpload() {
  Log::info("AppStore: Upload cancelled");

  updateUploadState([](UploadState &state) {
    state.status = UploadState::Status::Idle;
    state.progress = 0.0f;
    state.errorMessage = "";
    state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });
}

} // namespace Stores
} // namespace Sidechain
