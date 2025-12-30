#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"

namespace Sidechain {
namespace Stores {

void AppStore::uploadPost(const PostUploadData &postData, const juce::File &audioFile) {
  if (!networkClient) {
    Util::logError("AppStore", "Network client not available");
    UploadState newState = stateManager.uploads->getState();
    newState.isUploading = false;
    newState.uploadError = "Network client not initialized";
    newState.progress = 0;
    stateManager.uploads->setState(newState);
    return;
  }

  if (!audioFile.existsAsFile()) {
    Util::logError("AppStore", "Audio file does not exist: " + audioFile.getFullPathName());
    UploadState newState = stateManager.uploads->getState();
    newState.isUploading = false;
    newState.uploadError = "Audio file not found";
    newState.progress = 0;
    stateManager.uploads->setState(newState);
    return;
  }

  Util::logInfo("AppStore", "Starting upload - " + audioFile.getFileName());

  // Update state to uploading
  UploadState newState = stateManager.uploads->getState();
  newState.isUploading = true;
  newState.progress = 10;
  newState.uploadError = "";
  newState.currentFileName = audioFile.getFileName();
  newState.startTime = juce::Time::getCurrentTime().toMilliseconds();
  stateManager.uploads->setState(newState);

  // Load audio file and upload with metadata
  if (auto reader = std::unique_ptr<juce::AudioFormatReader>(
          juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(audioFile), true))) {

    // Create audio buffer from reader
    juce::AudioBuffer<float> audioBuffer(static_cast<int>(reader->numChannels),
                                         static_cast<int>(reader->lengthInSamples));
    reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    // Prepare upload metadata from typed PostUploadData
    NetworkClient::AudioUploadMetadata metadata;
    metadata.filename = postData.filename.isEmpty() ? audioFile.getFileName() : postData.filename;
    metadata.bpm = postData.bpm > 0 ? postData.bpm : 0.0;
    metadata.key = postData.key;
    metadata.genre = postData.genre;
    metadata.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    metadata.sampleRate = static_cast<int>(reader->sampleRate);
    metadata.numChannels = static_cast<int>(reader->numChannels);
    metadata.daw = NetworkClient::detectDAWName();

    // Upload audio with metadata
    networkClient->uploadAudioWithMetadata(audioBuffer, reader->sampleRate, metadata,
                                           [this](const Outcome<juce::String> &outcome) {
                                             if (outcome.isOk()) {
                                               Util::logInfo("AppStore", "Upload successful");
                                               UploadState uploadState = stateManager.uploads->getState();
                                               uploadState.isUploading = false;
                                               uploadState.progress = 100;
                                               uploadState.uploadError = "";
                                               stateManager.uploads->setState(uploadState);
                                             } else {
                                               Util::logError("AppStore", "Upload failed: " + outcome.getError());
                                               UploadState uploadState = stateManager.uploads->getState();
                                               uploadState.isUploading = false;
                                               uploadState.progress = 0;
                                               uploadState.uploadError = outcome.getError();
                                               stateManager.uploads->setState(uploadState);
                                             }
                                           });
  } else {
    Util::logError("AppStore", "Failed to read audio file: " + audioFile.getFullPathName());
    UploadState readErrorState = stateManager.uploads->getState();
    readErrorState.isUploading = false;
    readErrorState.uploadError = "Failed to read audio file";
    readErrorState.progress = 0;
    stateManager.uploads->setState(readErrorState);
  }
}

void AppStore::cancelUpload() {
  Util::logInfo("AppStore", "Upload cancelled");

  UploadState newState = stateManager.uploads->getState();
  newState.isUploading = false;
  newState.progress = 0;
  newState.uploadError = "";
  newState.currentFileName = "";
  stateManager.uploads->setState(newState);
}

// ==============================================================================
// Reactive Upload Methods (Phase 7)

rxcpp::observable<AppStore::UploadProgress> AppStore::uploadPostObservable(const PostUploadData &postData,
                                                                           const juce::File &audioFile) {
  return rxcpp::sources::create<UploadProgress>([this, postData, audioFile](auto observer) {
           if (!networkClient) {
             UploadProgress errorProgress;
             errorProgress.error = "Network client not initialized";
             observer.on_next(errorProgress);
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           if (!audioFile.existsAsFile()) {
             UploadProgress errorProgress;
             errorProgress.error = "Audio file not found";
             observer.on_next(errorProgress);
             observer.on_error(std::make_exception_ptr(std::runtime_error("Audio file not found")));
             return;
           }

           Util::logInfo("AppStore", "Starting upload observable - " + audioFile.getFileName());

           // Emit initial progress
           UploadProgress initialProgress;
           initialProgress.progress = 0.1f;
           observer.on_next(initialProgress);

           // Load audio file and upload with metadata
           auto reader = std::unique_ptr<juce::AudioFormatReader>(
               juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(audioFile), true));

           if (!reader) {
             UploadProgress readError;
             readError.error = "Failed to read audio file";
             observer.on_next(readError);
             observer.on_error(std::make_exception_ptr(std::runtime_error("Failed to read audio file")));
             return;
           }

           // Create audio buffer from reader
           juce::AudioBuffer<float> audioBuffer(static_cast<int>(reader->numChannels),
                                                static_cast<int>(reader->lengthInSamples));
           reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

           // Prepare upload metadata from typed PostUploadData
           NetworkClient::AudioUploadMetadata metadata;
           metadata.filename = postData.filename.isEmpty() ? audioFile.getFileName() : postData.filename;
           metadata.bpm = postData.bpm > 0 ? postData.bpm : 0.0;
           metadata.key = postData.key;
           metadata.genre = postData.genre;
           metadata.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
           metadata.sampleRate = static_cast<int>(reader->sampleRate);
           metadata.numChannels = static_cast<int>(reader->numChannels);
           metadata.daw = NetworkClient::detectDAWName();

           // Emit progress update before network call
           UploadProgress uploadingProgress;
           uploadingProgress.progress = 0.5f;
           observer.on_next(uploadingProgress);

           // Upload audio with metadata
           networkClient->uploadAudioWithMetadata(
               audioBuffer, reader->sampleRate, metadata, [observer](const Outcome<juce::String> &outcome) {
                 if (outcome.isOk()) {
                   Util::logInfo("AppStore", "Upload observable completed successfully");
                   UploadProgress successProgress;
                   successProgress.progress = 1.0f;
                   successProgress.isComplete = true;
                   successProgress.postId = outcome.getValue();
                   observer.on_next(successProgress);
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Upload observable failed: " + outcome.getError());
                   UploadProgress failProgress;
                   failProgress.error = outcome.getError();
                   observer.on_next(failProgress);
                   observer.on_error(std::make_exception_ptr(std::runtime_error(outcome.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
