#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/cache/DraftCache.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

void AppStore::loadDrafts() {
  updateState([](AppState &state) { state.drafts.isLoading = true; });

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

  updateState([draftsList](AppState &state) {
    state.drafts.drafts = draftsList;
    state.drafts.isLoading = false;
    state.drafts.draftError = "";
  });

  notifyObservers();
}

void AppStore::deleteDraft(const juce::String &draftId) {
  updateState([draftId](AppState &state) {
    // Remove draft from list
    for (int i = state.drafts.drafts.size() - 1; i >= 0; --i) {
      auto draft = state.drafts.drafts[i];
      if (draft.hasProperty("id") && draft.getProperty("id", juce::var()).toString() == draftId) {
        state.drafts.drafts.remove(i);
        Util::logInfo("AppStore", "Deleted draft: " + draftId);
        break;
      }
    }
  });

  // Remove from cache
  try {
    DraftKey key(static_cast<const juce::String&>(draftId));
    draftCache.removeDraftFile(key);
    Util::logInfo("AppStore", "Removed draft from cache: " + draftId);
  } catch (const std::exception &e) {
    Util::logError("AppStore", "Failed to remove draft from cache: " + juce::String(e.what()));
  }

  notifyObservers();
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

  updateState([](AppState &state) { state.drafts.draftError = ""; });

  notifyObservers();
}

} // namespace Stores
} // namespace Sidechain
