#include "AudioCache.h"
#include "../Log.h"

SidechainAudioCache::SidechainAudioCache(int64_t maxSizeBytes) : fileCache("audio", maxSizeBytes) {}

std::optional<juce::File> SidechainAudioCache::getAudioFile(const juce::String &url) {
  auto cachedFile = fileCache.getFile(url);
  return cachedFile;
}

juce::File SidechainAudioCache::cacheAudioFile(const juce::String &url, const juce::File &sourceFile) {
  if (!sourceFile.exists()) {
    Log::warn("SidechainAudioCache: Source file does not exist: " + sourceFile.getFullPathName());
    return juce::File();
  }

  auto ext = sourceFile.getFileExtension().toLowerCase();
  if (!ext.matchesWildcard("*.mp3|*.wav|*.flac|*.aac|*.m4a|*.ogg|*.wma", true)) {
    Log::warn("SidechainAudioCache: Unsupported audio format: " + ext);
  }

  auto cachedFile = fileCache.cacheFile(url, sourceFile);

  if (cachedFile.exists()) {
    Log::debug("SidechainAudioCache: Cached audio file " + url + " (" +
               juce::String(cachedFile.getSize() / 1024 / 1024) + " MB)");
  }

  return cachedFile;
}
