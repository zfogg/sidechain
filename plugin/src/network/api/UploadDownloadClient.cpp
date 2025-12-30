// ==============================================================================
// UploadDownloadClient.cpp - Upload and download operations (MIDI, project
// files) Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::getPostDownloadInfo(const juce::String &postId, DownloadInfoCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<DownloadInfo>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/posts/" + postId + "/download";
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "POST", nlohmann::json(), true);
    Log::debug("Get download info response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        if (result.success && result.data.is_object()) {
          DownloadInfo info;
          info.downloadUrl = juce::String(result.data.value("download_url", ""));
          info.filename = juce::String(result.data.value("filename", ""));
          info.metadata = result.data.value("metadata", nlohmann::json());
          info.downloadCount = result.data.value("download_count", 0);
          callback(Outcome<DownloadInfo>::ok(info));
        } else {
          auto outcome = requestResultToOutcome(result);
          callback(Outcome<DownloadInfo>::error(outcome.getError()));
        }
      });
    }
  });
}

void NetworkClient::downloadFile(const juce::String &url, const juce::File &targetFile,
                                 DownloadProgressCallback progressCallback, ResponseCallback callback) {
  // Create parent directory if it doesn't exist
  if (!targetFile.getParentDirectory().createDirectory()) {
    if (callback) {
      juce::MessageManager::callAsync([callback, targetFile]() {
        callback(Outcome<nlohmann::json>::error("Failed to create directory: " +
                                                targetFile.getParentDirectory().getFullPathName()));
      });
    }
    return;
  }

  Async::runVoid([url, targetFile, progressCallback, callback]() {
    juce::URL downloadUrl(url);
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS)
                       .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS);

    bool success = false;
    juce::String errorMessage = "Download failed";

    if (auto stream = downloadUrl.createInputStream(options)) {
      // Get total size if available (for progress)
      int64 totalBytes = stream->getTotalLength();
      int64 bytesRead = 0;
      const int bufferSize = 32768; // 32KB buffer for better throughput
      juce::HeapBlock<char> buffer(bufferSize);

      // Create output file
      juce::FileOutputStream output(targetFile);
      if (output.openedOk()) {
        // Throttle progress updates to avoid flooding message queue
        // Update at most every 100ms or 2% progress change
        float lastReportedProgress = 0.0f;
        auto lastProgressTime = juce::Time::getCurrentTime();
        const auto minProgressInterval = juce::RelativeTime::milliseconds(100);
        const float minProgressDelta = 0.02f;

        // Read and write in chunks
        bool writeError = false;
        int bytesReadThisChunk;
        while ((bytesReadThisChunk = stream->read(buffer.getData(), bufferSize)) > 0) {
          if (!output.write(buffer.getData(), static_cast<size_t>(bytesReadThisChunk))) {
            writeError = true;
            errorMessage = "Failed to write to file: " + targetFile.getFullPathName();
            break;
          }
          bytesRead += bytesReadThisChunk;

          // Throttled progress reporting
          if (progressCallback && totalBytes > 0) {
            float progress = static_cast<float>(bytesRead) / static_cast<float>(totalBytes);
            auto now = juce::Time::getCurrentTime();
            bool shouldReport = (progress - lastReportedProgress >= minProgressDelta) ||
                                (now - lastProgressTime >= minProgressInterval) || (progress >= 1.0f);

            if (shouldReport) {
              lastReportedProgress = progress;
              lastProgressTime = now;
              juce::MessageManager::callAsync([progressCallback, progress]() { progressCallback(progress); });
            }
          }
        }

        if (!writeError) {
          output.flush(); // Flush before proceeding
          success = (bytesRead > 0);
          if (!success)
            errorMessage = "No data received from server";
        } else {
          errorMessage = "Failed to write file: " + targetFile.getFullPathName();
        }
      } else {
        errorMessage = "Failed to create output file: " + targetFile.getFullPathName();
      }
    } else {
      errorMessage = "Failed to connect to: " + url;
    }

    if (callback) {
      juce::MessageManager::callAsync([callback, success, targetFile, errorMessage]() {
        if (success) {
          Log::info("File downloaded successfully to: " + targetFile.getFullPathName());
          callback(Outcome<nlohmann::json>::ok(nlohmann::json()));
        } else {
          Log::error(errorMessage);
          callback(Outcome<nlohmann::json>::error(errorMessage));
        }
      });
    }
  });
}

// ==============================================================================
void NetworkClient::downloadMIDI(const juce::String &midiId, const juce::File &targetFile, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  // Build the MIDI file download URL
  juce::String midiUrl = config.baseUrl + "/api/v1/midi/" + midiId + "/file";

  Async::runVoid([this, midiUrl, targetFile, callback]() {
    juce::URL downloadUrl(midiUrl);

    // Add auth header
    juce::StringPairArray headers;
    headers.set("Authorization", getAuthHeader());

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS)
                       .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS)
                       .withExtraHeaders(headers.getDescription());

    bool success = false;

    if (auto stream = downloadUrl.createInputStream(options)) {
      // Create output file
      juce::FileOutputStream output(targetFile);
      if (output.openedOk()) {
        // Read all data
        juce::MemoryBlock data;
        stream->readIntoMemoryBlock(data);

        if (data.getSize() > 0) {
          if (output.write(data.getData(), data.getSize())) {
            output.flush(); // Flush after write
            success = true;
          }
        }
      }
    }

    if (callback) {
      juce::String midiUrlCopy = midiUrl;
      juce::MessageManager::callAsync([callback, success, targetFile, midiUrlCopy]() {
        if (success) {
          Log::info("MIDI downloaded successfully to: " + targetFile.getFullPathName());
          callback(Outcome<nlohmann::json>::ok(nlohmann::json()));
        } else {
          Log::error("Failed to download MIDI from: " + midiUrlCopy);
          callback(Outcome<nlohmann::json>::error("MIDI download failed"));
        }
      });
    }
  });
}

// ==============================================================================
void NetworkClient::uploadMIDI(const nlohmann::json &midiData, const juce::String &name,
                               const juce::String &description, bool isPublic, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, midiData, name, description, isPublic, callback]() {
    // Build request body
    nlohmann::json requestBody;

    // Extract events from midiData
    if (midiData.is_object() && midiData.contains("events"))
      requestBody["events"] = midiData["events"];
    else
      requestBody["events"] = midiData; // Assume midiData is the events array

    // Extract or set tempo
    if (midiData.is_object() && midiData.contains("tempo"))
      requestBody["tempo"] = midiData["tempo"];
    else
      requestBody["tempo"] = 120;

    // Extract or set time signature
    if (midiData.is_object() && midiData.contains("time_signature"))
      requestBody["time_signature"] = midiData["time_signature"];
    else
      requestBody["time_signature"] = nlohmann::json::array({4, 4});

    // Extract or calculate total_time
    if (midiData.is_object() && midiData.contains("total_time"))
      requestBody["total_time"] = midiData["total_time"];

    // Optional fields
    if (name.isNotEmpty())
      requestBody["name"] = name.toStdString();
    if (description.isNotEmpty())
      requestBody["description"] = description.toStdString();
    requestBody["is_public"] = isPublic;

    auto result = makeRequestWithRetry(buildApiPath("/midi"), "POST", requestBody, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        if (result.success) {
          callback(Outcome<nlohmann::json>::ok(result.data));
        } else {
          callback(Outcome<nlohmann::json>::error(result.errorMessage));
        }
      });
    }
  });
}

// ==============================================================================
// Project file operations

void NetworkClient::downloadProjectFile(const juce::String &projectFileId, const juce::File &targetFile,
                                        DownloadProgressCallback progressCallback, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  // Use the download endpoint which redirects to the CDN
  juce::String downloadUrl = config.baseUrl + "/api/v1/project-files/" + projectFileId + "/download";

  Async::runVoid([this, downloadUrl, targetFile, progressCallback, callback]() {
    juce::URL url(downloadUrl);

    // Create parent directory if needed
    if (!targetFile.getParentDirectory().createDirectory()) {
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<nlohmann::json>::error("Failed to create directory")); });
      }
      return;
    }

    // Set up connection with auth
    juce::StringPairArray headers;
    headers.set("Authorization", getAuthHeader());

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(config.timeoutMs)
                       .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS)
                       .withExtraHeaders(headers.getDescription());

    // Download file
    std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));

    if (stream == nullptr) {
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<nlohmann::json>::error("Failed to connect to server")); });
      }
      return;
    }

    // Write to file
    juce::FileOutputStream output(targetFile);
    if (!output.openedOk()) {
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<nlohmann::json>::error("Failed to create output file")); });
      }
      return;
    }

    // Stream data to file (with progress if available)
    const int bufferSize = 8192;
    juce::HeapBlock<char> buffer(bufferSize);
    int64 totalBytes = stream->getTotalLength();
    int64 bytesRead = 0;
    bool writeError = false;

    while (!stream->isExhausted()) {
      int numRead = stream->read(buffer, bufferSize);
      if (numRead > 0) {
        if (!output.write(buffer, static_cast<size_t>(numRead))) {
          writeError = true;
          break;
        }
        bytesRead += numRead;

        // Report progress
        if (progressCallback && totalBytes > 0) {
          float progress = static_cast<float>(bytesRead) / static_cast<float>(totalBytes);
          juce::MessageManager::callAsync([progressCallback, progress]() { progressCallback(progress); });
        }
      }
    }

    if (writeError) {
      output.flush(); // Best effort flush
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<nlohmann::json>::error("Failed to write file")); });
      }
      return;
    }

    if (callback) {
      juce::MessageManager::callAsync([callback]() { callback(Outcome<nlohmann::json>::ok(nlohmann::json())); });
    }
  });
}

void NetworkClient::uploadProjectFile(const juce::File &projectFile, const juce::String &audioPostId,
                                      const juce::String &description, bool isPublic,
                                      DownloadProgressCallback progressCallback, UploadCallback callback) {
  if (!isAuthenticated()) {
    if (callback) {
      juce::MessageManager::callAsync(
          [callback]() { callback(Outcome<juce::String>::error(Constants::Errors::NOT_AUTHENTICATED)); });
    }
    return;
  }

  if (!projectFile.existsAsFile()) {
    if (callback) {
      juce::MessageManager::callAsync(
          [callback]() { callback(Outcome<juce::String>::error("Project file does not exist")); });
    }
    return;
  }

  // Check file size (50MB max)
  const int64 maxSize = 50 * 1024 * 1024;
  if (projectFile.getSize() > maxSize) {
    if (callback) {
      juce::MessageManager::callAsync(
          [callback]() { callback(Outcome<juce::String>::error("Project file too large (max 50MB)")); });
    }
    return;
  }

  // Detect DAW type from file extension
  juce::String extension = projectFile.getFileExtension().toLowerCase();
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

  juce::String filename = projectFile.getFileName();
  int64 fileSize = projectFile.getSize();

  Async::runVoid([this, projectFile, audioPostId, description, isPublic, dawType, filename, fileSize, progressCallback,
                  callback]() {
    // Read file data
    juce::MemoryBlock fileData;
    if (!projectFile.loadFileAsData(fileData)) {
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<juce::String>::error("Failed to read project file")); });
      }
      return;
    }

    // Upload file to CDN first
    std::map<juce::String, juce::String> fields;
    // No additional fields needed for CDN upload

    auto uploadResult = uploadMultipartData("/api/v1/upload/project", // CDN upload endpoint
                                            "project_file", fileData, filename, "application/octet-stream", fields);

    if (!uploadResult.success) {
      if (callback) {
        juce::MessageManager::callAsync([callback, uploadResult]() {
          callback(Outcome<juce::String>::error(uploadResult.getUserFriendlyError()));
        });
      }
      return;
    }

    // Get CDN URL from response
    juce::String fileUrl;
    if (uploadResult.data.is_object()) {
      if (uploadResult.data.contains("url"))
        fileUrl = juce::String(uploadResult.data.value("url", ""));
      if (fileUrl.isEmpty() && uploadResult.data.contains("file_url"))
        fileUrl = juce::String(uploadResult.data.value("file_url", ""));
    }

    if (fileUrl.isEmpty()) {
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<juce::String>::error("Upload succeeded but no URL returned")); });
      }
      return;
    }

    // Now create the project file record
    nlohmann::json recordData;
    recordData["filename"] = filename.toStdString();
    recordData["file_url"] = fileUrl.toStdString();
    recordData["file_size"] = fileSize;
    recordData["daw_type"] = dawType.toStdString();
    recordData["is_public"] = isPublic;

    if (description.isNotEmpty())
      recordData["description"] = description.toStdString();

    if (audioPostId.isNotEmpty())
      recordData["audio_post_id"] = audioPostId.toStdString();

    auto recordResult = makeRequestWithRetry(buildApiPath("/project-files"), "POST", recordData, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, recordResult]() {
        if (recordResult.success) {
          juce::String projectFileId;
          if (recordResult.data.is_object() && recordResult.data.contains("id"))
            projectFileId = juce::String(recordResult.data.value("id", ""));
          callback(Outcome<juce::String>::ok(projectFileId));
        } else {
          callback(Outcome<juce::String>::error(recordResult.getUserFriendlyError()));
        }
      });
    }

    if (recordResult.success)
      Log::info("Project file uploaded successfully");
    else
      Log::error("Project file record creation failed: " + recordResult.getUserFriendlyError());
  });
}
