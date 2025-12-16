#include "DraftCache.h"

SidechainDraftCache::SidechainDraftCache(int64_t maxSizeBytes) : fileCache("drafts", maxSizeBytes) {
}

std::optional<juce::File> SidechainDraftCache::getDraftFile(const DraftKey &key) {
  return fileCache.getFile(key);
}

juce::File SidechainDraftCache::cacheDraftFile(const DraftKey &key, const juce::File &sourceFile) {
  return fileCache.cacheFile(key, sourceFile);
}
