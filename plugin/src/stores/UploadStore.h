#pragma once

#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * UploadState - Immutable state for audio upload operations
 */
struct UploadState {
  enum class Status { Idle, Editing, Uploading, Success, Error };

  Status status = Status::Idle;
  float progress = 0.0f;
  juce::String errorMessage;
  juce::String uploadedPostId; // Set after successful upload
  int64_t lastUpdated = 0;
};

/**
 * UploadStore - Reactive store for managing audio loop uploads
 *
 * Features:
 * - Track upload progress (0-100%)
 * - Handle upload completion and errors
 * - Manage upload state (Idle, Editing, Uploading, Success, Error)
 * - Optimistic updates and error recovery
 *
 * Usage:
 * ```cpp
 * auto uploadStore = std::make_shared<UploadStore>(networkClient);
 * uploadStore->subscribe([this](const UploadState& state) {
 *   updateUploadUI(state.status, state.progress);
 * });
 * uploadStore->uploadLoop(audioData, metadata);
 * ```
 */
class UploadStore : public Store<UploadState> {
public:
  explicit UploadStore(NetworkClient *client);
  ~UploadStore() override = default;

  //==============================================================================
  // Upload Operations
  void startUpload(const juce::String &filename, const juce::String &genre, const juce::String &key, double bpm,
                   const juce::AudioBuffer<float> &audioData, double sampleRate);

  void updateProgress(float progressPercent);

  void completeUpload(const juce::String &postId);

  void failUpload(const juce::String &error);

  void cancelUpload();

  void reset();

  //==============================================================================
  // Current State Access
  UploadState::Status getStatus() const {
    return getState().status;
  }

  float getProgress() const {
    return getState().progress;
  }

  juce::String getError() const {
    return getState().errorMessage;
  }

  juce::String getUploadedPostId() const {
    return getState().uploadedPostId;
  }

  bool isUploading() const {
    const auto &state = getState();
    return state.status == UploadState::Status::Uploading;
  }

protected:
  //==============================================================================
  // Helper methods
  void updateStatus(UploadState::Status newStatus);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Upload data (cached during upload process)
  juce::String uploadFilename;
  juce::String uploadGenre;
  juce::String uploadKey;
  double uploadBpm = 0.0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UploadStore)
};

} // namespace Stores
} // namespace Sidechain
