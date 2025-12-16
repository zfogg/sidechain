#include "ImageCache.h"
#include "../Log.h"

SidechainImageCache::SidechainImageCache(int64_t maxSizeBytes) : fileCache("images", maxSizeBytes) {}

std::optional<juce::Image> SidechainImageCache::getImage(const juce::String &url) {
  // Check memory cache first
  {
    juce::ScopedReadLock locker(memoryCacheLock);
    auto it = memoryCache.find(url);
    if (it != memoryCache.end()) {
      return it->second;
    }
  }

  // Check file cache
  auto cachedFile = fileCache.getFile(url);
  if (!cachedFile) {
    return std::nullopt;
  }

  // Load from disk
  auto image = loadImageFromFile(*cachedFile);
  if (image) {
    // Cache in memory
    juce::ScopedWriteLock locker(memoryCacheLock);
    memoryCache[url] = *image;
  }

  return image;
}

void SidechainImageCache::cacheImage(const juce::String &url, const juce::Image &image) {
  if (image.isNull()) {
    Log::warn("SidechainImageCache: Attempted to cache null image for " + url);
    return;
  }

  // Write to temporary file
  auto tempFile = juce::File::createTempFile("img_");
  auto ext = juce::File(url).getFileExtension().toLowerCase();
  if (ext.isEmpty()) {
    ext = ".png";
  }

  std::unique_ptr<juce::ImageFileFormat> format;
  if (ext.containsIgnoreCase("png")) {
    format = std::make_unique<juce::PNGImageFormat>();
  } else if (ext.containsIgnoreCase("jpg") || ext.containsIgnoreCase("jpeg")) {
    format = std::make_unique<juce::JPEGImageFormat>();
  } else {
    // Default to PNG
    format = std::make_unique<juce::PNGImageFormat>();
  }

  if (!format) {
    Log::warn("SidechainImageCache: Could not determine image format for " + url);
    tempFile.deleteFile();
    return;
  }

  juce::FileOutputStream out(tempFile);
  if (!out.openedOk()) {
    Log::warn("SidechainImageCache: Failed to open temp file for writing");
    tempFile.deleteFile();
    return;
  }

  if (!format->writeImageToStream(image, out)) {
    Log::warn("SidechainImageCache: Failed to write image to temp file");
    out.flush();
    tempFile.deleteFile();
    return;
  }

  out.flush();

  // Move to file cache
  auto cachedFile = fileCache.cacheFile(url, tempFile);
  tempFile.deleteFile();

  if (!cachedFile.exists()) {
    Log::warn("SidechainImageCache: Failed to cache image file");
    return;
  }

  // Cache in memory
  {
    juce::ScopedWriteLock locker(memoryCacheLock);
    memoryCache[url] = image;
  }

  Log::debug("SidechainImageCache: Cached image " + url + " (" + juce::String(image.getWidth()) + "x" +
             juce::String(image.getHeight()) + ")");
}

std::optional<juce::Image> SidechainImageCache::loadImageFromFile(const juce::File &file) {
  juce::FileInputStream in(file);
  if (!in.openedOk()) {
    Log::warn("SidechainImageCache: Failed to open cached image file: " + file.getFullPathName());
    return std::nullopt;
  }

  auto format = juce::ImageFileFormat::findImageFormatForStream(in);
  if (format == nullptr) {
    Log::warn("SidechainImageCache: Could not detect image format for: " + file.getFullPathName());
    return std::nullopt;
  }

  auto image = format->decodeImage(in);
  if (image.isNull()) {
    Log::warn("SidechainImageCache: Failed to decode image: " + file.getFullPathName());
    return std::nullopt;
  }

  return image;
}
