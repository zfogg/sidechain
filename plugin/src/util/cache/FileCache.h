#pragma once

#include <JuceHeader.h>
#include <optional>
#include <map>
#include "../logging/Logger.h"

// ==============================================================================
/**
 * CacheKeyTraits - Template for extracting cache keys from domain types
 *
 * Specializations define how to convert domain objects to string keys.
 * Callers must provide a specialization for their type.
 *
 * Example:
 *   template<>
 *   struct CacheKeyTraits<Draft> {
 *     static juce::String getKey(const Draft& draft) { return draft.id; }
 *   };
 */
template <typename T> struct CacheKeyTraits {
  // Callers must specialize this for their type
  static juce::String getKey(const T &value);
};

// Built-in specialization for juce::String (URL-based caching)
template <> struct CacheKeyTraits<juce::String> {
  static juce::String getKey(const juce::String &url) {
    return url;
  }
};

// ==============================================================================
/**
 * FileCache - Generic cross-platform file caching with LRU eviction
 *
 * Template parameter T should have a CacheKeyTraits<T> specialization that
 * defines how to extract the cache key from domain objects.
 *
 * Features:
 * - Cross-platform cache directory (~/Library/Application Support/Sidechain/cache/{subdir}/)
 * - Automatic LRU eviction when size limits exceeded
 * - Manifest tracking file metadata (key, size, access time)
 * - Thread-safe operations
 * - Configurable size limits
 *
 * Usage:
 *   // Define trait specialization
 *   template<>
 *   struct CacheKeyTraits<ImageKey> {
 *     static juce::String getKey(const ImageKey& key) { return key.url; }
 *   };
 *
 *   // Use cache
 *   FileCache<ImageKey> cache("images", 500 * 1024 * 1024);
 *   auto file = cache.getFile(imageKey);
 */
template <typename T> class FileCache {
public:
  /**
   * Initialize cache for a specific subdirectory.
   * @param subdirectory Name of cache subdirectory (e.g., "images", "audio")
   * @param maxSizeBytes Maximum cache size before LRU eviction
   */
  FileCache(const juce::String &subdirectory, int64_t maxSizeBytes);
  ~FileCache() = default;

  // ==============================================================================
  // Public API

  /**
   * Get cached file for value. Returns std::nullopt if not cached.
   * Updates last access time on hit.
   * Uses CacheKeyTraits<T>::getKey() to extract the cache key.
   */
  std::optional<juce::File> getFile(const T &value);

  /**
   * Store file in cache. Returns the cached file path.
   * Overwrites if file already exists for this key.
   * Uses CacheKeyTraits<T>::getKey() to extract the cache key.
   */
  juce::File cacheFile(const T &value, const juce::File &sourceFile);

  /**
   * Remove file from cache by value.
   * Uses CacheKeyTraits<T>::getKey() to extract the cache key.
   */
  void removeFile(const T &value);

  /**
   * Clear entire cache directory.
   */
  void clear();

  /**
   * Get current cache directory.
   */
  juce::File getCacheDirectory() const {
    return cacheDir;
  }

  /**
   * Get current cache size in bytes.
   */
  int64_t getCacheSizeBytes() const;

  /**
   * Get maximum cache size in bytes.
   */
  int64_t getMaxSizeBytes() const {
    return maxSize;
  }

  /**
   * Force manifest to disk.
   */
  void flush();

private:
  // ==============================================================================
  // Manifest structure
  struct CacheEntry {
    juce::String key;
    juce::String filename;
    int64_t fileSize = 0;
    double lastAccessTime = 0.0;

    juce::var toJSON() const;
    static CacheEntry fromJSON(const juce::var &json);
  };

  // ==============================================================================
  juce::File cacheDir;
  int64_t maxSize;
  std::map<juce::String, CacheEntry> manifest;
  juce::ReadWriteLock manifestLock;

  // Manifest file path
  juce::File getManifestFile() const;

  // Load manifest from disk
  void loadManifest();

  // Save manifest to disk
  void saveManifest();

  // Evict files using LRU strategy until target size is reached
  void evictLRU(int64_t targetSizeBytes);

  // Generate unique filename for key
  juce::String generateCacheFilename(const juce::String &key);

  // Hash string to create filename
  static juce::String hashString(const juce::String &str);

  // FIX #1: Internal unlocked version of getCacheSizeBytes() for use when already holding manifestLock
  int64_t getCacheSizeBytes_unlocked() const {
    int64_t totalSize = 0;
    for (const auto &pair : manifest) {
      totalSize += pair.second.fileSize;
    }
    return totalSize;
  }
};

// ==============================================================================
// Template implementations (inline in header for all translation units)
// ==============================================================================

template <typename T>
FileCache<T>::FileCache(const juce::String &subdirectory, int64_t maxSizeBytes) : maxSize(maxSizeBytes) {
  // Construct cache directory path
  auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
  cacheDir = appDataDir.getChildFile("Sidechain").getChildFile("cache").getChildFile(subdirectory);

  if (!cacheDir.exists()) {
    cacheDir.createDirectory();
  }

  loadManifest();
}

template <typename T> std::optional<juce::File> FileCache<T>::getFile(const T &value) {
  juce::String key = CacheKeyTraits<T>::getKey(value);

  // FIX #4: TOCTOU PREVENTION
  // Keep read lock while checking file existence to prevent race condition
  // where file is deleted between our manifest lookup and existence check
  juce::ScopedReadLock readLock(manifestLock);

  auto it = manifest.find(key);
  if (it == manifest.end()) {
    return std::nullopt;
  }

  auto cachedFile = cacheDir.getChildFile(it->second.filename);
  if (!cachedFile.exists()) {
    // Lock releases automatically when exiting scope
    removeFile(value);
    return std::nullopt;
  }

  // Lock releases automatically when exiting scope

  // Update access time with write lock
  {
    juce::ScopedWriteLock writeLocker(manifestLock);
    auto it2 = manifest.find(key);
    if (it2 != manifest.end()) {
      it2->second.lastAccessTime = static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()) / 1000.0;
      saveManifest();
    }
  }

  return cachedFile;
}

template <typename T> juce::File FileCache<T>::cacheFile(const T &value, const juce::File &sourceFile) {
  juce::String key = CacheKeyTraits<T>::getKey(value);

  if (!sourceFile.exists()) {
    return juce::File();
  }

  juce::File destFile;
  {
    juce::ScopedWriteLock locker(manifestLock);

    auto it = manifest.find(key);
    if (it != manifest.end()) {
      it->second.lastAccessTime = static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()) / 1000.0;
      destFile = cacheDir.getChildFile(it->second.filename);

      if (!sourceFile.copyFileTo(destFile)) {
        return juce::File();
      }

      it->second.fileSize = destFile.getSize();
      saveManifest();
      return destFile;
    }

    CacheEntry entry;
    entry.key = key;
    entry.filename = generateCacheFilename(key);
    entry.lastAccessTime = static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()) / 1000.0;

    destFile = cacheDir.getChildFile(entry.filename);

    if (!sourceFile.copyFileTo(destFile)) {
      return juce::File();
    }

    entry.fileSize = destFile.getSize();
    manifest[key] = entry;

    // FIX #1: Use unlocked version since we already hold write lock (prevents deadlock)
    if (getCacheSizeBytes_unlocked() > maxSize) {
      evictLRU(static_cast<int64_t>(static_cast<double>(maxSize) * 0.8));
    }

    saveManifest();
    return destFile;
  }
}

template <typename T> void FileCache<T>::removeFile(const T &value) {
  juce::String key = CacheKeyTraits<T>::getKey(value);

  {
    juce::ScopedWriteLock locker(manifestLock);

    auto it = manifest.find(key);
    if (it == manifest.end()) {
      return;
    }

    juce::File fileToDelete = cacheDir.getChildFile(it->second.filename);
    fileToDelete.deleteFile();
    manifest.erase(it);
    saveManifest();
  }
}

template <typename T> void FileCache<T>::clear() {
  {
    juce::ScopedWriteLock locker(manifestLock);
    manifest.clear();
  }

  if (cacheDir.exists()) {
    cacheDir.deleteRecursively();
    cacheDir.createDirectory();
  }

  saveManifest();
}

template <typename T> int64_t FileCache<T>::getCacheSizeBytes() const {
  juce::ScopedReadLock locker(manifestLock);
  int64_t totalSize = 0;

  for (const auto &pair : manifest) {
    totalSize += pair.second.fileSize;
  }

  return totalSize;
}

template <typename T> void FileCache<T>::flush() {
  saveManifest();
}

template <typename T> juce::File FileCache<T>::getManifestFile() const {
  return cacheDir.getChildFile("manifest.json");
}

template <typename T> void FileCache<T>::loadManifest() {
  juce::ScopedWriteLock locker(manifestLock);

  juce::File manifestFile = getManifestFile();
  if (!manifestFile.exists()) {
    manifest.clear();
    return;
  }

  auto manifestContent = manifestFile.loadFileAsString();
  juce::var parsedJson = juce::JSON::parse(manifestContent);

  if (!parsedJson.isArray()) {
    // DATA-2: Log warning instead of silently clearing (potential corruption)
    Sidechain::Util::logWarning("FileCache", "Cache manifest corrupted or invalid format - cache will be rebuilt");
    manifest.clear();
    return;
  }

  manifest.clear();
  for (int i = 0; i < parsedJson.size(); ++i) {
    try {
      CacheEntry entry = CacheEntry::fromJSON(parsedJson[i]);
      manifest[entry.key] = entry;
    } catch (const std::exception &e) {
      Sidechain::Util::logWarning("FileCache", "Failed to parse cache entry - skipping");
      continue;
    }
  }
}

template <typename T> void FileCache<T>::saveManifest() {
  juce::var manifestArray;

  for (const auto &pair : manifest) {
    manifestArray.append(pair.second.toJSON());
  }

  juce::String manifestJson = juce::JSON::toString(manifestArray);
  juce::File manifestFile = getManifestFile();

  // DATA-1: Check if manifest write succeeds instead of silently failing
  if (!manifestFile.replaceWithText(manifestJson)) {
    Sidechain::Util::logError("FileCache", "Failed to save cache manifest - disk full or permission denied");
    return;
  }
}

template <typename T> void FileCache<T>::evictLRU(int64_t targetSizeBytes) {
  // FIX #1: DEADLOCK PREVENTION
  // This is called from cacheFile() which already holds write lock.
  // IMPORTANT: Assumes caller holds manifestLock write lock. Does NOT acquire lock.
  // Use unlocked version for all size checks to avoid recursive lock acquisition.

  if (getCacheSizeBytes_unlocked() <= targetSizeBytes) {
    return;
  }

  std::vector<std::pair<juce::String, CacheEntry>> entries(manifest.begin(), manifest.end());
  std::sort(entries.begin(), entries.end(),
            [](const auto &a, const auto &b) { return a.second.lastAccessTime < b.second.lastAccessTime; });

  for (const auto &pair : entries) {
    if (getCacheSizeBytes_unlocked() <= targetSizeBytes) {
      break;
    }

    juce::File fileToDelete = cacheDir.getChildFile(pair.second.filename);
    // DATA-3: Only update manifest AFTER successful file deletion
    if (fileToDelete.deleteFile()) {
      manifest.erase(pair.first);
    } else {
      Sidechain::Util::logWarning("FileCache", "Failed to delete cache file - manifest may be inconsistent");
    }
  }

  // Save manifest after all deletions complete
  saveManifest();
}

template <typename T> juce::String FileCache<T>::generateCacheFilename(const juce::String &key) {
  return hashString(key) + ".cache";
}

template <typename T> juce::String FileCache<T>::hashString(const juce::String &str) {
  return juce::SHA256(str.toUTF8()).toHexString().substring(0, 16);
}

template <typename T> juce::var FileCache<T>::CacheEntry::toJSON() const {
  juce::DynamicObject::Ptr dynObj = new juce::DynamicObject();
  dynObj->setProperty("key", key);
  dynObj->setProperty("filename", filename);
  dynObj->setProperty("fileSize", static_cast<int64>(fileSize));
  dynObj->setProperty("lastAccessTime", lastAccessTime);
  return juce::var(dynObj);
}

template <typename T> typename FileCache<T>::CacheEntry FileCache<T>::CacheEntry::fromJSON(const juce::var &json) {
  CacheEntry entry;
  entry.key = json.getProperty("key", "").toString();
  entry.filename = json.getProperty("filename", "").toString();
  entry.fileSize = static_cast<int64_t>(static_cast<juce::int64>(json.getProperty("fileSize", 0)));
  entry.lastAccessTime = static_cast<double>(json.getProperty("lastAccessTime", 0.0));
  return entry;
}
