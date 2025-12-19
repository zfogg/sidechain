//==============================================================================
// AudioClient.cpp - Audio upload operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../../audio/KeyDetector.h"
#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

//==============================================================================
void NetworkClient::uploadAudio(const juce::String &recordingId, const juce::AudioBuffer<float> &audioBuffer,
                                double sampleRate, UploadCallback callback) {
  if (!isAuthenticated()) {
    Log::warn("Cannot upload audio: " + juce::String(Constants::Errors::NOT_AUTHENTICATED));
    if (callback) {
      juce::MessageManager::callAsync(
          [callback]() { callback(Outcome<juce::String>::error(Constants::Errors::NOT_AUTHENTICATED)); });
    }
    return;
  }

  // Upload rate limiting check
  if (uploadRateLimiter) {
    juce::String identifier = currentUserId.isEmpty() ? "anonymous" : currentUserId;
    auto rateLimitStatus = uploadRateLimiter->tryConsume(identifier, 1);

    if (!rateLimitStatus.allowed) {
      int retrySeconds =
          rateLimitStatus.retryAfterSeconds > 0 ? rateLimitStatus.retryAfterSeconds : rateLimitStatus.resetInSeconds;

      juce::String retryMsg = retrySeconds > 0 ? " You can upload again in " + juce::String(retrySeconds) + " seconds."
                                               : " Please try again later.";

      juce::String errorMsg = "Upload limit exceeded." + retryMsg;
      Log::warn("Upload rate limit exceeded for " + identifier + ": " + errorMsg);

      if (callback) {
        juce::MessageManager::callAsync([callback, errorMsg]() { callback(Outcome<juce::String>::error(errorMsg)); });
      }
      return;
    }

    Log::debug("Upload rate limit OK for " + identifier + " - remaining: " + juce::String(rateLimitStatus.remaining) +
               "/" + juce::String(rateLimitStatus.limit));
  }

  // Copy the buffer for the background thread
  juce::AudioBuffer<float> bufferCopy(audioBuffer);

  Async::runVoid([this, recordingId, bufferCopy, sampleRate, callback]() {
    // Encode audio to WAV (server will transcode to MP3)
    auto audioData = encodeAudioToWAV(bufferCopy, sampleRate);

    if (audioData.getSize() == 0) {
      Log::error("Failed to encode audio");
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<juce::String>::error("Failed to encode audio")); });
      }
      return;
    }

    // Calculate duration in seconds
    double durationSecs = static_cast<double>(bufferCopy.getNumSamples()) / sampleRate;

    // Build metadata fields for multipart upload
    // Auto-detect metadata where possible
    std::map<juce::String, juce::String> metadata;
    metadata["recording_id"] = recordingId;

    // Detect key using KeyDetector (if available)
    juce::String detectedKey = "C major"; // Default fallback
    if (KeyDetector::isAvailable()) {
      KeyDetector keyDetector;
      auto key = keyDetector.detectKey(bufferCopy, sampleRate, bufferCopy.getNumChannels());
      if (key.isValid()) {
        detectedKey = key.name;
        Log::info("NetworkClient: Detected key: " + detectedKey);
      } else {
        Log::debug("NetworkClient: Key detection failed, using default");
      }
    } else {
      Log::debug("NetworkClient: KeyDetector not available, using default key");
    }
    metadata["key"] = detectedKey;

    // Detect DAW from host application
    juce::String dawName = NetworkClient::detectDAWName();
    metadata["daw"] = dawName;

    // BPM: Default to 120 if not available (should be passed from processor)
    // In practice, BPM should come from PluginProcessor::getCurrentBPM()
    // For now, use default but log that it should be provided
    double bpm = 120.0;
    metadata["bpm"] = juce::String(bpm, 1);
    Log::debug("NetworkClient: Using default BPM (120). Consider using "
               "uploadAudioWithMetadata with BPM from processor.");

    // Calculate duration_bars from BPM and duration
    double beatsPerSecond = bpm / 60.0;
    double totalBeats = durationSecs * beatsPerSecond;
    int bars = static_cast<int>(totalBeats / 4.0 + 0.5); // Assuming 4/4 time
    metadata["duration_bars"] = juce::String(juce::jmax(1, bars));
    metadata["duration_seconds"] = juce::String(durationSecs, 2);
    metadata["sample_rate"] = juce::String(static_cast<int>(sampleRate));
    metadata["channels"] = juce::String(bufferCopy.getNumChannels());

    // Generate filename
    juce::String fileName = recordingId + ".wav";

    // Upload using multipart form data
    auto result = uploadMultipartData("/api/v1/audio/upload", "audio_file", audioData, fileName, "audio/wav", metadata);

    bool success = result.success;
    juce::String audioUrl;

    if (result.data.isObject()) {
      audioUrl = result.data.getProperty("audio_url", "").toString();
      if (audioUrl.isEmpty())
        audioUrl = result.data.getProperty("url", "").toString();
    }

    if (callback) {
      juce::MessageManager::callAsync([callback, success, audioUrl, result]() {
        if (success)
          callback(Outcome<juce::String>::ok(audioUrl));
        else
          callback(Outcome<juce::String>::error(result.getUserFriendlyError()));
      });
    }

    if (success)
      Log::info("Audio uploaded successfully: " + audioUrl);
    else
      Log::error("Audio upload failed: " + result.getUserFriendlyError());
  });
}

//==============================================================================
void NetworkClient::uploadAudioWithMetadata(const juce::AudioBuffer<float> &audioBuffer, double sampleRate,
                                            const AudioUploadMetadata &metadata, UploadCallback callback) {
  if (!isAuthenticated()) {
    Log::warn("Cannot upload audio: " + juce::String(Constants::Errors::NOT_AUTHENTICATED));
    if (callback) {
      juce::MessageManager::callAsync(
          [callback]() { callback(Outcome<juce::String>::error(Constants::Errors::NOT_AUTHENTICATED)); });
    }
    return;
  }

  // Upload rate limiting check
  if (uploadRateLimiter) {
    juce::String identifier = currentUserId.isEmpty() ? "anonymous" : currentUserId;
    auto rateLimitStatus = uploadRateLimiter->tryConsume(identifier, 1);

    if (!rateLimitStatus.allowed) {
      int retrySeconds =
          rateLimitStatus.retryAfterSeconds > 0 ? rateLimitStatus.retryAfterSeconds : rateLimitStatus.resetInSeconds;

      juce::String retryMsg = retrySeconds > 0 ? " You can upload again in " + juce::String(retrySeconds) + " seconds."
                                               : " Please try again later.";

      juce::String errorMsg = "Upload limit exceeded." + retryMsg;
      Log::warn("Upload rate limit exceeded for " + identifier + ": " + errorMsg);

      if (callback) {
        juce::MessageManager::callAsync([callback, errorMsg]() { callback(Outcome<juce::String>::error(errorMsg)); });
      }
      return;
    }

    Log::debug("Upload rate limit OK for " + identifier + " - remaining: " + juce::String(rateLimitStatus.remaining) +
               "/" + juce::String(rateLimitStatus.limit));
  }

  // Copy the buffer and metadata for the background thread
  juce::AudioBuffer<float> bufferCopy(audioBuffer);
  AudioUploadMetadata metadataCopy = metadata;

  // Detect DAW if not provided (before lambda capture)
  if (metadataCopy.daw.isEmpty()) {
    metadataCopy.daw = NetworkClient::detectDAWName();
  }

  Async::runVoid([this, bufferCopy, sampleRate, metadataCopy, callback]() {
    // Encode audio to WAV (server will transcode to MP3)
    auto audioData = encodeAudioToWAV(bufferCopy, sampleRate);

    if (audioData.getSize() == 0) {
      Log::error("Failed to encode audio");
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<juce::String>::error("Failed to encode audio")); });
      }
      return;
    }

    // Generate unique recording ID
    juce::String recordingId = juce::Uuid().toString();

    // Calculate duration
    double durationSecs = static_cast<double>(bufferCopy.getNumSamples()) / sampleRate;

    // Build metadata fields for multipart upload
    std::map<juce::String, juce::String> fields;
    fields["recording_id"] = recordingId;
    fields["filename"] = metadataCopy.filename;

    if (metadataCopy.bpm > 0)
      fields["bpm"] = juce::String(metadataCopy.bpm, 1);

    if (metadataCopy.key.isNotEmpty())
      fields["key"] = metadataCopy.key;

    if (metadataCopy.genre.isNotEmpty())
      fields["genre"] = metadataCopy.genre;

    fields["duration_seconds"] = juce::String(durationSecs, 2);
    fields["sample_rate"] = juce::String(static_cast<int>(sampleRate));
    fields["channels"] = juce::String(bufferCopy.getNumChannels());

    // Add DAW to fields (already detected before lambda if needed)
    if (metadataCopy.daw.isNotEmpty()) {
      fields["daw"] = metadataCopy.daw;
    }

    // Calculate approximate bar count if BPM is known
    if (metadataCopy.bpm > 0) {
      double beatsPerSecond = metadataCopy.bpm / 60.0;
      double totalBeats = durationSecs * beatsPerSecond;
      int bars = static_cast<int>(totalBeats / 4.0 + 0.5); // Assuming 4/4 time
      fields["duration_bars"] = juce::String(juce::jmax(1, bars));
    }

    // Include MIDI data if available (R.3.3 Cross-DAW MIDI Collaboration)
    if (metadataCopy.includeMidi && !metadataCopy.midiData.isVoid()) {
      // Serialize MIDI data as JSON string for multipart field
      juce::String midiJson = juce::JSON::toString(metadataCopy.midiData, true);
      if (midiJson.isNotEmpty() && midiJson != "null") {
        fields["midi_data"] = midiJson;
        Log::debug("Including MIDI data in upload: " + juce::String(midiJson.length()) + " chars");
      }
    }

    // Include comment audience setting
    if (metadataCopy.commentAudience.isNotEmpty()) {
      fields["comment_audience"] = metadataCopy.commentAudience;
    }

    // Generate safe filename for upload
    juce::String safeFilename = metadataCopy.filename.replaceCharacters(" /\\:*?\"<>|", "-----------");
    juce::String fileName = safeFilename + "-" + recordingId.substring(0, 8) + ".wav";

    // Upload using multipart form data
    auto result = uploadMultipartData("/api/v1/audio/upload", "audio_file", audioData, fileName, "audio/wav", fields);

    bool success = result.success;
    juce::String audioUrl;
    juce::String audioPostId;

    // Check for error messages in response body (even if HTTP status is 200)
    if (result.data.isObject()) {
      // Check if response contains an error
      if (result.data.hasProperty("error")) {
        juce::String errorMsg = result.data.getProperty("error", "").toString();
        juce::String message = result.data.getProperty("message", "").toString();
        if (errorMsg.isNotEmpty() || message.isNotEmpty()) {
          success = false;
          Log::warn("Upload response contains error: " + (errorMsg.isNotEmpty() ? errorMsg : message));
        }
      }

      audioUrl = result.data.getProperty("audio_url", "").toString();
      if (audioUrl.isEmpty())
        audioUrl = result.data.getProperty("url", "").toString();
      audioPostId = result.data.getProperty("id", "").toString();
      if (audioPostId.isEmpty())
        audioPostId = result.data.getProperty("post_id", "").toString();
    }

    // If success is true but audioUrl is empty, treat as failure
    if (success && audioUrl.isEmpty()) {
      Log::warn("Upload reported success but audioUrl is empty, treating as failure");
      success = false;
    }

    // If audio upload succeeded and project file should be included, upload it
    if (success && metadataCopy.includeProjectFile && metadataCopy.projectFile.existsAsFile()) {
      Log::info("Audio upload succeeded, now uploading project file: " + metadataCopy.projectFile.getFileName());

      // Read project file data
      juce::MemoryBlock projectData;
      if (metadataCopy.projectFile.loadFileAsData(projectData)) {
        // Detect DAW type from extension
        juce::String extension = metadataCopy.projectFile.getFileExtension().toLowerCase();
        juce::String dawType = "other";
        if (extension == ".als" || extension == ".alp")
          dawType = "ableton";
        else if (extension == ".flp")
          dawType = "fl_studio";
        else if (extension == ".logic" || extension == ".logicx")
          dawType = "logic";
        else if (extension == ".ptx" || extension == ".ptf")
          dawType = "pro_tools";
        else if (extension == ".cpr")
          dawType = "cubase";
        else if (extension == ".song")
          dawType = "studio_one";
        else if (extension == ".rpp")
          dawType = "reaper";
        else if (extension == ".bwproject")
          dawType = "bitwig";

        // Upload project file to CDN
        std::map<juce::String, juce::String> projectFields;
        auto projectUploadResult =
            uploadMultipartData("/api/v1/upload/project", "project_file", projectData,
                                metadataCopy.projectFile.getFileName(), "application/octet-stream", projectFields);

        if (projectUploadResult.success) {
          juce::String fileUrl;
          if (projectUploadResult.data.isObject()) {
            fileUrl = projectUploadResult.data.getProperty("url", "").toString();
            if (fileUrl.isEmpty())
              fileUrl = projectUploadResult.data.getProperty("file_url", "").toString();
          }

          if (fileUrl.isNotEmpty()) {
            // Create project file record linked to audio post
            juce::var recordData = juce::var(new juce::DynamicObject());
            recordData.getDynamicObject()->setProperty("filename", metadataCopy.projectFile.getFileName());
            recordData.getDynamicObject()->setProperty("file_url", fileUrl);
            recordData.getDynamicObject()->setProperty("file_size",
                                                       static_cast<int64>(metadataCopy.projectFile.getSize()));
            recordData.getDynamicObject()->setProperty("daw_type", dawType);
            recordData.getDynamicObject()->setProperty("is_public", true);
            if (audioPostId.isNotEmpty())
              recordData.getDynamicObject()->setProperty("audio_post_id", audioPostId);

            auto recordResult = makeRequestWithRetry(buildApiPath("/project-files"), "POST", recordData, true);
            if (recordResult.success)
              Log::info("Project file record created successfully");
            else
              Log::warn("Project file record creation failed: " + recordResult.getUserFriendlyError());
          }
        } else {
          Log::warn("Project file upload failed: " + projectUploadResult.getUserFriendlyError());
        }
      } else {
        Log::warn("Failed to read project file data");
      }
    }

    if (callback) {
      juce::MessageManager::callAsync([callback, success, audioUrl, result]() {
        if (success)
          callback(Outcome<juce::String>::ok(audioUrl));
        else
          callback(Outcome<juce::String>::error(result.getUserFriendlyError()));
      });
    }

    if (success)
      Log::info("Audio with metadata uploaded successfully: " + audioUrl);
    else
      Log::error("Audio upload failed: " + result.getUserFriendlyError());
  });
}
