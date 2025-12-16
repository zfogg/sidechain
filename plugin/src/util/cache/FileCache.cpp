#include "FileCache.h"
#include "../Log.h"
#include <cmath>

FileCache::FileCache(const juce::String &subdirectory, int64_t maxSizeBytes) : maxSize(maxSizeBytes) {
  // Construct cache directory path: ~/Library/Application Support/Sidechain/cache/{subdir}/
  auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
  cacheDir = appDataDir.getChildFile("Sidechain").getChildFile("cache").getChildFile(subdirectory);

  // Create directory if it doesn't exist
  if (!cacheDir.exists()) {
    cacheDir.createDirectory();
  }

  Log::debug("FileCache: Initialized for " + subdirectory + " at " + cacheDir.getFullPathName());

  // Load existing manifest
  loadManifest();
}

//==============================================================================
std::optional<juce::File> FileCache::getFile(const juce::String &url) {
  // First check if file exists (with read lock)
  juce::File cachedFile;
  bool fileExists = false;
  {
    juce::ScopedReadLock locker(manifestLock);
    auto it = manifest.find(url);
    if (it == manifest.end()) {
      return std::nullopt;
    }
    cachedFile = cacheDir.getChildFile(it->second.filename);
    fileExists = cachedFile.exists();
  }

  if (!fileExists) {
    // File was deleted manually, remove from manifest
    removeFile(url);
    return std::nullopt;
  }

  // Update last access time
  {
    juce::ScopedWriteLock writeLocker(manifestLock);
    auto it = manifest.find(url);
    if (it != manifest.end()) {
      it->second.lastAccessTime = static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()) / 1000.0;
      saveManifest();
    }
  }

  return cachedFile;
}

juce::File FileCache::cacheFile(const juce::String &url, const juce::File &sourceFile) {
  if (!sourceFile.exists()) {
    Log::warn("FileCache: Source file does not exist: " + sourceFile.getFullPathName());
    return juce::File();
  }

  juce::File destFile;
  {
    juce::ScopedWriteLock locker(manifestLock);

    // Check if already cached
    auto it = manifest.find(url);
    if (it != manifest.end()) {
      // Update existing entry
      it->second.lastAccessTime = static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()) / 1000.0;
      destFile = cacheDir.getChildFile(it->second.filename);

      // Copy file
      if (!sourceFile.copyFileTo(destFile)) {
        Log::warn("FileCache: Failed to copy file to cache: " + destFile.getFullPathName());
        return juce::File();
      }

      it->second.fileSize = destFile.getSize();
      saveManifest();
      return destFile;
    }

    // New cache entry
    CacheEntry entry;
    entry.url = url;
    entry.filename = generateCacheFilename(url);
    entry.lastAccessTime = static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()) / 1000.0;

    destFile = cacheDir.getChildFile(entry.filename);

    // Copy file to cache
    if (!sourceFile.copyFileTo(destFile)) {
      Log::warn("FileCache: Failed to copy file to cache: " + destFile.getFullPathName());
      return juce::File();
    }

    entry.fileSize = destFile.getSize();

    // Add to manifest
    manifest[url] = entry;
    int64_t currentSize = getCacheSizeBytes();
    saveManifest();

    if (currentSize > maxSize) {
      // Release write lock before evicting (evictLRU will acquire its own lock)
    } else {
      return destFile;
    }
  }

  // Evict LRU if needed (outside of write lock)
  if (getCacheSizeBytes() > maxSize) {
    evictLRU(maxSize / 2); // Evict until 50% full
  }

  Log::debug("FileCache: Cached " + url + " (" + juce::String(destFile.getSize() / 1024) + " KB)");
  return destFile;
}

void FileCache::removeFile(const juce::String &url) {
  juce::ScopedWriteLock locker(manifestLock);

  auto it = manifest.find(url);
  if (it == manifest.end()) {
    return;
  }

  auto file = cacheDir.getChildFile(it->second.filename);
  if (file.exists()) {
    file.deleteFile();
  }

  manifest.erase(it);
  saveManifest();
}

void FileCache::clear() {
  juce::ScopedWriteLock locker(manifestLock);

  manifest.clear();
  cacheDir.deleteRecursively();
  cacheDir.createDirectory();
  saveManifest();

  Log::info("FileCache: Cleared cache directory");
}

int64_t FileCache::getCacheSizeBytes() const {
  int64_t totalSize = 0;
  for (const auto &entry : manifest) {
    totalSize += entry.second.fileSize;
  }
  return totalSize;
}

void FileCache::flush() {
  juce::ScopedWriteLock locker(manifestLock);
  saveManifest();
}

//==============================================================================
juce::File FileCache::getManifestFile() const {
  return cacheDir.getChildFile("manifest.json");
}

void FileCache::loadManifest() {
  auto manifestFile = getManifestFile();
  if (!manifestFile.exists()) {
    Log::debug("FileCache: No existing manifest found");
    return;
  }

  auto content = manifestFile.loadFileAsString();
  auto json = juce::JSON::parse(content);

  if (!json.isObject()) {
    Log::warn("FileCache: Invalid manifest format");
    return;
  }

  auto entriesArray = json.getProperty("entries", juce::var());
  if (!entriesArray.isArray()) {
    Log::warn("FileCache: Manifest has no entries array");
    return;
  }

  for (int i = 0; i < entriesArray.size(); ++i) {
    try {
      auto entry = CacheEntry::fromJSON(entriesArray[i]);
      manifest[entry.url] = entry;
    } catch (const std::exception &e) {
      Log::warn("FileCache: Failed to parse manifest entry: " + juce::String(e.what()));
    }
  }

  Log::debug("FileCache: Loaded " + juce::String(manifest.size()) + " entries from manifest");
}

void FileCache::saveManifest() {
  juce::DynamicObject obj;
  juce::Array<juce::var> entries;

  for (const auto &entry : manifest) {
    entries.add(entry.second.toJSON());
  }

  obj.setProperty("entries", entries);
  obj.setProperty("version", 1);
  obj.setProperty("timestamp", static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()) / 1000.0);

  auto manifestFile = getManifestFile();
  auto json = juce::JSON::toString(juce::var(&obj), true);
  manifestFile.replaceWithText(json);
}

void FileCache::evictLRU(int64_t targetSizeBytes) {
  juce::ScopedWriteLock locker(manifestLock);

  // Sort entries by last access time (oldest first)
  std::vector<std::pair<juce::String, CacheEntry>> sortedEntries(manifest.begin(), manifest.end());
  std::sort(sortedEntries.begin(), sortedEntries.end(),
            [](const auto &a, const auto &b) { return a.second.lastAccessTime < b.second.lastAccessTime; });

  int64_t currentSize = getCacheSizeBytes();
  for (const auto &entry : sortedEntries) {
    if (currentSize <= targetSizeBytes) {
      break;
    }

    auto file = cacheDir.getChildFile(entry.second.filename);
    if (file.exists()) {
      currentSize -= file.getSize();
      file.deleteFile();
      manifest.erase(entry.first);

      Log::debug("FileCache: Evicted " + entry.first);
    }
  }

  saveManifest();
}

juce::String FileCache::generateCacheFilename(const juce::String &url) {
  // Hash URL to create unique filename
  auto hash = hashString(url);
  auto ext = juce::File(url).getFileExtension();
  return hash + ext;
}

juce::String FileCache::hashString(const juce::String &str) {
  // Create filename from URL hash
  // Use a simple hash by summing character values mod a large prime
  uint64_t hash = 5381;
  for (char c : str.toStdString()) {
    hash = ((hash << 5) + hash) ^ static_cast<uint8_t>(c);
  }
  // Format as hex string (16 chars)
  return juce::String::toHexString(static_cast<uint32_t>(hash >> 32)) +
         juce::String::toHexString(static_cast<uint32_t>(hash));
}

//==============================================================================
juce::var FileCache::CacheEntry::toJSON() const {
  juce::DynamicObject obj;
  obj.setProperty("url", url);
  obj.setProperty("filename", filename);
  obj.setProperty("fileSize", static_cast<double>(fileSize));
  obj.setProperty("lastAccessTime", lastAccessTime);
  return juce::var(&obj);
}

FileCache::CacheEntry FileCache::CacheEntry::fromJSON(const juce::var &json) {
  if (!json.isObject()) {
    throw std::runtime_error("Expected JSON object for CacheEntry");
  }

  CacheEntry entry;
  entry.url = json.getProperty("url", "").toString();
  entry.filename = json.getProperty("filename", "").toString();
  entry.fileSize = static_cast<int64_t>(static_cast<double>(json.getProperty("fileSize", 0.0)));
  entry.lastAccessTime = json.getProperty("lastAccessTime", 0.0);

  if (entry.url.isEmpty() || entry.filename.isEmpty()) {
    throw std::runtime_error("Missing required fields in CacheEntry");
  }

  return entry;
}
