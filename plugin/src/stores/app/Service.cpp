#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/Async.h"
#include <chrono>
#include <thread>
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

//==============================================================================
// Memory Cache Implementation - Type-Safe Ephemeral Data Caching
//
// Caches non-persistent data (users, posts, messages) with TTL-based expiration.
// Binary assets (images, audio) use file caching instead (see Phase 2).

void AppStore::invalidateCache(const juce::String &key) {
  std::lock_guard<std::mutex> lock(memoryCacheLock);
  size_t erased = memoryCache.erase(key);
  if (erased > 0) {
    Util::logDebug("AppStore", "Cache invalidated: " + key);
  }
}

void AppStore::invalidateCachePattern(const juce::String &pattern) {
  std::lock_guard<std::mutex> lock(memoryCacheLock);

  // Simple wildcard pattern matching: "feed:*" matches "feed:home", "feed:following", etc.
  if (pattern.endsWith("*")) {
    auto prefix = pattern.dropLastCharacters(1);
    int count = 0;

    auto it = memoryCache.begin();
    while (it != memoryCache.end()) {
      if (it->first.startsWith(prefix)) {
        it = memoryCache.erase(it);
        ++count;
      } else {
        ++it;
      }
    }

    if (count > 0) {
      Util::logDebug("AppStore", "Cache pattern invalidated: " + pattern + " (" + juce::String(count) + " entries)");
    }
  } else {
    memoryCache.erase(pattern);
    Util::logDebug("AppStore", "Cache invalidated: " + pattern);
  }
}

void AppStore::clearMemoryCaches() {
  std::lock_guard<std::mutex> lock(memoryCacheLock);
  int count = static_cast<int>(memoryCache.size());
  memoryCache.clear();
  Util::logInfo("AppStore", "All memory caches cleared (" + juce::String(count) + " entries removed)");
}

size_t AppStore::getMemoryCacheSize() const {
  std::lock_guard<std::mutex> lock(memoryCacheLock);
  size_t totalSize = 0;

  for (const auto &[key, entry] : memoryCache) {
    // Approximate size: key length + 64 bytes overhead per entry
    totalSize += static_cast<size_t>(key.length()) + 64;
  }

  return totalSize;
}

//==============================================================================
// Cache Getter Methods (sync access to existing file caches)

juce::Image AppStore::getCachedImage(const juce::String &url) {
  auto cached = imageCache.getImage(url);
  return cached ? *cached : juce::Image();
}

juce::File AppStore::getCachedAudio(const juce::String &url) {
  auto cached = audioCache.getAudioFile(url);
  return cached ? *cached : juce::File();
}

//==============================================================================
// Reactive Image Service Operations (rxcpp::observable pattern for Phase 2)
//
// These methods wrap the callback-based getImage() into reactive observables.
// They leverage the multi-level caching (memory → file → network) that getImage() provides.

rxcpp::observable<juce::Image> AppStore::loadImageObservable(const juce::String &url) {
  return rxcpp::sources::create<juce::Image>([this, url](auto observer) {
    getImage(url, [observer](const juce::Image &image) {
      if (image.isValid()) {
        observer.on_next(image);
      } else {
        observer.on_next(juce::Image()); // Emit empty image on error
      }
      observer.on_completed();
    });
  });
}

//==============================================================================
// Reactive Audio Service Operations (rxcpp::observable pattern for Phase 2)
//
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
            output.flush();

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
        [observer](const juce::File &file) {
          if (file.existsAsFile()) {
            observer.on_next(file);
          } else {
            observer.on_next(juce::File()); // Emit empty file on error
          }
          observer.on_completed();
        });
  });
}

} // namespace Stores
} // namespace Sidechain
