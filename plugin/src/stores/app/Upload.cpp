#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::uploadPost(const juce::var &postData, const juce::File &audioFile) {
  // Extract post metadata if available
  juce::String postId;
  if (postData.isObject()) {
    const auto *obj = postData.getDynamicObject();
    if (obj) {
      postId = obj->getProperty("id").toString();
      if (!postId.isEmpty()) {
        Util::logInfo("AppStore", "Starting upload for post ID: " + postId);
      }
    }
  }

  if (!networkClient) {
    Util::logError("AppStore", "Network client not available");
    UploadState newState = sliceManager.getUploadSlice()->getState();
    newState.isUploading = false;
    newState.uploadError = "Network client not initialized";
    newState.progress = 0;
    sliceManager.getUploadSlice()->setState(newState);
    return;
  }

  Util::logInfo("AppStore", "Starting upload - " + audioFile.getFileName());

  // Update state to uploading
  UploadState newState = sliceManager.getUploadSlice()->getState();
  newState.isUploading = true;
  newState.progress = 10;
  newState.uploadError = "";
  newState.currentFileName = "";
  newState.startTime = juce::Time::getCurrentTime().toMilliseconds();
  sliceManager.getUploadSlice()->setState(newState);

  // In a real implementation, would encode audio to MP3/WAV and upload to CDN
  Util::logInfo("AppStore", "Upload initiated for " + audioFile.getFullPathName());
}

void AppStore::cancelUpload() {
  Util::logInfo("AppStore", "Upload cancelled");

  UploadState newState = sliceManager.getUploadSlice()->getState();
  newState.isUploading = false;
  newState.progress = 0;
  newState.uploadError = "";
  newState.currentFileName = "";
  sliceManager.getUploadSlice()->setState(newState);
}

} // namespace Stores
} // namespace Sidechain
