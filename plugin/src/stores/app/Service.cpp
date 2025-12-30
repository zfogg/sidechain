#include "../AppStore.h"
#include "../../network/NetworkClient.h"
#include "../../util/logging/Logger.h"
#include "../../util/Async.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/User.h"
#include <chrono>
#include <thread>
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Cache Getter Methods (sync access to existing file caches)

juce::Image AppStore::getCachedImage(const juce::String &url) {
  auto cached = imageCache.getImage(url);
  return cached ? *cached : juce::Image();
}

juce::File AppStore::getCachedAudio(const juce::String &url) {
  auto cached = audioCache.getAudioFile(url);
  return cached ? *cached : juce::File();
}

// ==============================================================================
// Reactive Image Service Operations (rxcpp::observable pattern for Phase 2)

// These methods wrap the callback-based getImage into reactive observables.
// They leverage the multi-level caching (memory → file → network) that getImage provides.

rxcpp::observable<juce::Image> AppStore::loadImageObservable(const juce::String &url) {
  return rxcpp::sources::create<juce::Image>([this, url](auto observer) {
           Util::logDebug("AppStore", "loadImageObservable: Loading " + url);
           getImage(url, [observer, url](const juce::Image &image) {
             if (image.isValid()) {
               Util::logInfo("AppStore", "loadImageObservable: Loaded successfully - " + url);
               observer.on_next(image);
               observer.on_completed();
             } else {
               Util::logWarning("AppStore", "loadImageObservable: Image invalid after download - " + url);
               observer.on_error(
                   std::make_exception_ptr(std::runtime_error("Failed to load image: " + url.toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

// ==============================================================================
// Reactive Audio Service Operations (rxcpp::observable pattern for Phase 2)

// These methods wrap audio loading into reactive observables.
// They leverage file caching for downloaded audio files.

rxcpp::observable<juce::File> AppStore::loadAudioObservable(const juce::String &url) {
  return rxcpp::sources::create<juce::File>([this, url](auto observer) {
           // Try file cache first
           if (auto cached = audioCache.getAudioFile(url)) {
             Util::logDebug("AppStore", "Audio cache hit: " + url);
             observer.on_next(*cached);
             observer.on_completed();
             return;
           }

           // Download from network on background thread
           Async::run<juce::File>(
               [this, url]() {
                 try {
                   Util::logDebug("AppStore", "Audio downloading: " + url);
                   juce::URL audioUrl(url);
                   auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                       .getChildFile(juce::String("audio_") +
                                                     juce::String::toHexString(static_cast<int>(std::time(nullptr))));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                   auto inputStream =
                       audioUrl.createInputStream(false, nullptr, nullptr, "User-Agent: Sidechain/1.0", 5000, nullptr);
#pragma clang diagnostic pop
                   if (inputStream == nullptr) {
                     Util::logWarning("AppStore", "Failed to open audio stream for " + url);
                     return juce::File();
                   }

                   juce::FileOutputStream output(tempFile);
                   if (!output.openedOk()) {
                     Util::logWarning("AppStore", "Failed to create temp file for audio");
                     return juce::File();
                   }

                   // writeFromInputStream returns int64, but audio files are always < 2GB
                   // so safe to cast to int (INT_MAX = 2.1GB)
                   int64_t bytesWritten = output.writeFromInputStream(*inputStream, -1);
                   int numBytesWritten = static_cast<int>(bytesWritten);

                   output.flush(); // FileOutputStream::flush() returns void in modern JUCE
                   if (!output.openedOk()) {
                     Util::logWarning("AppStore", "Failed to write audio file");
                     tempFile.deleteFile();
                     return juce::File();
                   }

                   if (numBytesWritten <= 0) {
                     Util::logWarning("AppStore", "Failed to download audio file");
                     tempFile.deleteFile();
                     return juce::File();
                   }

                   // Cache the downloaded file
                   audioCache.cacheAudioFile(url, tempFile);
                   Util::logInfo("AppStore", "Audio downloaded and cached: " + url);
                   return tempFile;
                 } catch (const std::exception &e) {
                   Util::logError("AppStore", "Audio fetch error: " + juce::String(e.what()));
                   return juce::File();
                 }
               },
               [observer, url](const juce::File &file) {
                 if (file.existsAsFile()) {
                   observer.on_next(file);
                   observer.on_completed();
                 } else {
                   observer.on_error(
                       std::make_exception_ptr(std::runtime_error("Failed to download audio: " + url.toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

// ==============================================================================
// Image Loading (Callback-based, called by loadImageObservable and other methods)

void AppStore::getImage(const juce::String &url, std::function<void(const juce::Image &)> callback) {
  if (url.isEmpty()) {
    callback(juce::Image());
    return;
  }

  // Try memory cache first
  if (auto cached = imageCache.getImage(url)) {
    Log::info("AppStore::getImage: Cache hit - " + url);
    callback(*cached);
    return;
  }

  Log::debug("AppStore::getImage: Cache miss, downloading - " + url);

  // Download image on background thread
  Async::run<juce::Image>(
      [this, url]() {
        try {
          Log::debug("AppStore::getImage: Starting download - " + url);
          juce::URL imageUrl(url);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
          // Follow redirects (true), handle auth properly, 5s timeout
          // Note: Don't send custom User-Agent as some servers may reject it
          auto inputStream = imageUrl.createInputStream(true, nullptr, nullptr, juce::String(), 5000, nullptr);
#pragma clang diagnostic pop

          if (inputStream == nullptr) {
            Log::warn("AppStore::getImage: Failed to open image stream for " + url);
            return juce::Image();
          }

          Log::debug("AppStore::getImage: Stream opened - " + url);

          // Read all data from stream into memory
          // Note: getTotalLength() might return incorrect value, so we read incrementally
          std::vector<char> buffer;
          const int READ_CHUNK_SIZE = 8192;
          char chunk[READ_CHUNK_SIZE];
          int bytesRead;

          while ((bytesRead = inputStream->read(chunk, READ_CHUNK_SIZE)) > 0) {
            buffer.insert(buffer.end(), chunk, chunk + bytesRead);
          }

          int64_t streamLength = static_cast<int64_t>(buffer.size());
          Log::debug("AppStore::getImage: Read " + juce::String(streamLength) + " bytes");

          // Check magic bytes to identify format
          if (streamLength >= 4) {
            unsigned char byte0 = static_cast<unsigned char>(buffer[0]);
            unsigned char byte1 = static_cast<unsigned char>(buffer[1]);

            // If it's HTML (starts with <html or <meta, etc), log the error
            if (byte0 == 0x3C && (byte1 == 0x68 || byte1 == 0x6D || byte1 == 0x21)) {
              // It's an HTML error response
              std::string htmlPreview(buffer.begin(),
                                      buffer.begin() + juce::jmin(static_cast<int>(buffer.size()), 200));
              Log::warn("AppStore::getImage: Got HTML response instead of image from " + url);
              Log::warn("AppStore::getImage: HTML preview: " + juce::String(htmlPreview));
              return juce::Image();
            }
          }

          if (streamLength > 0) {
            juce::MemoryInputStream memIn(buffer.data(), static_cast<size_t>(streamLength), false);
            auto image = juce::ImageFileFormat::loadFrom(memIn);

            if (image.isValid()) {
              Log::info("AppStore::getImage: Image decoded successfully - " + juce::String(image.getWidth()) + "x" +
                        juce::String(image.getHeight()) + " from " + url);
              imageCache.cacheImage(url, image);
            } else {
              Log::warn("AppStore::getImage: Failed to decode image (" + juce::String(streamLength) + " bytes) from " +
                        url);
            }

            return image;
          } else {
            Log::warn("AppStore::getImage: Stream has no data from " + url);
            return juce::Image();
          }
        } catch (const std::exception &e) {
          Log::error("AppStore::getImage: Image fetch error: " + juce::String(e.what()));
          return juce::Image();
        }
      },
      [callback](const juce::Image &image) { callback(image); });
}

// ==============================================================================
// Search Operations

rxcpp::observable<std::vector<User>> AppStore::searchUsersObservable(const juce::String &query) {
  if (!networkClient) {
    return rxcpp::sources::error<std::vector<User>>(
        std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  if (query.isEmpty()) {
    return rxcpp::sources::just(std::vector<User>());
  }

  // Use the network client's observable API - already returns typed vector
  return networkClient->searchUsersObservable(query, 20).map([](const std::vector<User> &users) {
    Util::logInfo("AppStore", "Search returned " + juce::String(static_cast<int>(users.size())) + " users");
    return users;
  });
}

// ==============================================================================
// Follow Operations

rxcpp::observable<int> AppStore::followUserObservable(const juce::String &userId) {
  if (!networkClient) {
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  if (userId.isEmpty()) {
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("User ID is empty")));
  }

  // Use the network client's observable API and transform the result
  return networkClient->followUserObservable(userId).map([userId](const NetworkClient::FollowResult &) {
    Util::logInfo("AppStore", "Followed user successfully: " + userId);
    return 1;
  });
}

rxcpp::observable<int> AppStore::unfollowUserObservable(const juce::String &userId) {
  if (!networkClient) {
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  if (userId.isEmpty()) {
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("User ID is empty")));
  }

  // Use the network client's observable API and transform the result
  return networkClient->unfollowUserObservable(userId).map([userId](int) {
    Util::logInfo("AppStore", "Unfollowed user successfully: " + userId);
    return 1;
  });
}

} // namespace Stores
} // namespace Sidechain
