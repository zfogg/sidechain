#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::uploadPost(const juce::var & [[maybe_unused]] postData, const juce::File &audioFile) {
  if (!networkClient) {
    Util::logError("AppStore", "Network client not available");
    sliceManager.getUploadSlice()->dispatch([](UploadState &state) {
      state.isUploading = false;
      state.uploadError = "Network client not initialized";
      state.progress = 0;
    });
    return;
  }

  Util::logInfo("AppStore", "Starting upload - " + audioFile.getFileName());

  // Update state to uploading
  sliceManager.getUploadSlice()->dispatch([](UploadState &state) {
    state.isUploading = true;
    state.progress = 10;
    state.uploadError = "";
    state.currentFileName = "";
    state.startTime = juce::Time::getCurrentTime().toMilliseconds();
  });

  // In a real implementation, would encode audio to MP3/WAV and upload to CDN
  Util::logInfo("AppStore", "Upload initiated for " + audioFile.getFullPathName());
}

void AppStore::cancelUpload() {
  Util::logInfo("AppStore", "Upload cancelled");

  sliceManager.getUploadSlice()->dispatch([](UploadState &state) {
    state.isUploading = false;
    state.progress = 0;
    state.uploadError = "";
    state.currentFileName = "";
  });
}

} // namespace Stores
} // namespace Sidechain
