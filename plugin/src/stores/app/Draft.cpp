#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/cache/DraftCache.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

void AppStore::loadDrafts() {
  auto draftSlice = sliceManager.getDraftSlice();
  draftSlice->dispatch([](DraftState &state) { state.isLoading = true; });

  // Load drafts from cache directory
  juce::Array<juce::var> draftsList;

  try {
    auto cacheDir = draftCache.getCacheDirectory();
    if (!cacheDir.exists()) {
      Util::logInfo("AppStore", "No draft cache directory found");
    } else {
      // Iterate through cache directory to find all draft files
      for (const auto &file : cacheDir.findChildFiles(juce::File::findFiles, false, "*.cache")) {
        try {
          auto content = file.loadFileAsString();
          auto draft = juce::JSON::parse(content);
          if (draft.isObject()) {
            draftsList.add(draft);
          }
        } catch (...) {
          Util::logWarning("AppStore", "Failed to parse draft file: " + file.getFileName());
        }
      }
      Util::logInfo("AppStore", "Loaded " + juce::String(draftsList.size()) + " drafts from cache");
    }
  } catch (const std::exception &e) {
    Util::logError("AppStore", "Failed to load drafts: " + juce::String(e.what()));
  }

  draftSlice->dispatch([draftsList](DraftState &state) {
    state.drafts = draftsList;
    state.isLoading = false;
    state.draftError = "";
  });
}

void AppStore::deleteDraft(const juce::String &draftId) {
  auto draftSlice = sliceManager.getDraftSlice();
  draftSlice->dispatch([draftId](DraftState &state) {
    // Remove draft from list
    for (int i = state.drafts.size() - 1; i >= 0; --i) {
      auto draft = state.drafts[i];
      if (draft.hasProperty("id") && draft.getProperty("id", juce::var()).toString() == draftId) {
        state.drafts.remove(i);
        Util::logInfo("AppStore", "Deleted draft: " + draftId);
        break;
      }
    }
  });

  // Remove from cache
  try {
    DraftKey key(static_cast<const juce::String &>(draftId));
    draftCache.removeDraftFile(key);
    Util::logInfo("AppStore", "Removed draft from cache: " + draftId);
  } catch (const std::exception &e) {
    Util::logError("AppStore", "Failed to remove draft from cache: " + juce::String(e.what()));
  }
}

void AppStore::clearAutoRecoveryDraft() {
  Util::logInfo("AppStore", "Clearing auto-recovery draft");

  // Remove auto-recovery draft from cache
  try {
    DraftKey key(juce::String("autoRecoveryDraft"));
    draftCache.removeDraftFile(key);
    Util::logInfo("AppStore", "Auto-recovery draft cleared from cache");
  } catch (const std::exception &e) {
    Util::logError("AppStore", "Failed to clear auto-recovery draft: " + juce::String(e.what()));
  }

  sliceManager.getDraftSlice()->dispatch([](DraftState &state) { state.draftError = ""; });
}

void AppStore::saveDrafts() {
  try {
    auto draftSlice = sliceManager.getDraftSlice();
    const auto &draftState = draftSlice->getState();

    for (const auto &draft : draftState.drafts) {
      if (!draft.isObject() || !draft.hasProperty("id")) {
        continue;
      }

      try {
        juce::String draftId = draft.getProperty("id", "").toString();
        if (draftId.isEmpty()) {
          continue;
        }

        // Convert draft to JSON and write to temporary file
        juce::String draftJson = juce::JSON::toString(draft);
        juce::File tempFile =
            juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("draft_" + draftId + ".tmp");

        if (!tempFile.replaceWithText(draftJson)) {
          Util::logWarning("AppStore", "Failed to write draft to temp file: " + draftId);
          continue;
        }

        // Cache the draft file
        DraftKey key(draftId);
        draftCache.cacheDraftFile(key, tempFile);
        tempFile.deleteFile();

        Util::logInfo("AppStore", "Saved draft to cache: " + draftId);
      } catch (const std::exception &e) {
        Util::logWarning("AppStore", "Failed to save individual draft: " + juce::String(e.what()));
      }
    }

    Util::logInfo("AppStore", "Saved " + juce::String(draftState.drafts.size()) + " drafts to cache");
  } catch (const std::exception &e) {
    Util::logError("AppStore", "Failed to save drafts: " + juce::String(e.what()));
  }
}

} // namespace Stores
} // namespace Sidechain
