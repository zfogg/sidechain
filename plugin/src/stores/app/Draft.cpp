#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/cache/DraftCache.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::loadDrafts() {
  auto draftState = stateManager.draft;
  DraftState newState = draftState->getState();
  newState.isLoading = true;
  draftState->setState(newState);

  // Load drafts from cache directory
  std::vector<std::shared_ptr<Sidechain::Draft>> draftsList;

  try {
    auto cacheDir = draftCache.getCacheDirectory();
    if (!cacheDir.exists()) {
      Util::logInfo("AppStore", "No draft cache directory found");
    } else {
      // Iterate through cache directory to find all draft files
      for (const auto &file : cacheDir.findChildFiles(juce::File::findFiles, false, "*.cache")) {
        try {
          auto content = file.loadFileAsString();
          auto jsonObj = nlohmann::json::parse(content.toStdString());
          if (jsonObj.is_object()) {
            auto draftResult = Sidechain::Draft::createFromJson(jsonObj);
            if (draftResult.isOk()) {
              draftsList.push_back(draftResult.getValue());
            }
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

  DraftState finalState = draftState->getState();
  finalState.drafts = draftsList;
  finalState.isLoading = false;
  finalState.draftError = "";
  draftState->setState(finalState);
}

void AppStore::deleteDraft(const juce::String &draftId) {
  auto draftState = stateManager.draft;
  DraftState newState = draftState->getState();
  // Remove draft from list
  for (int i = static_cast<int>(newState.drafts.size()) - 1; i >= 0; --i) {
    auto draft = newState.drafts[static_cast<size_t>(i)];
    if (draft && draft->id == draftId) {
      newState.drafts.erase(newState.drafts.begin() + static_cast<int>(i));
      Util::logInfo("AppStore", "Deleted draft: " + draftId);
      break;
    }
  }
  draftState->setState(newState);

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

  DraftState newState = stateManager.draft->getState();
  newState.draftError = "";
  stateManager.draft->setState(newState);
}

void AppStore::saveDrafts() {
  try {
    const auto &draftState = stateManager.draft->getState();

    for (const auto &draft : draftState.drafts) {
      if (!draft) {
        continue;
      }

      try {
        juce::String draftId = draft->id;
        if (draftId.isEmpty()) {
          continue;
        }

        // Convert draft to JSON and write to temporary file
        juce::String draftJsonStr;
        try {
          nlohmann::json j;
          to_json(j, *draft);
          draftJsonStr = j.dump();
        } catch (...) {
          Util::logWarning("AppStore", "Failed to serialize draft: " + draftId);
          continue;
        }
        juce::File tempFile =
            juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("draft_" + draftId + ".tmp");

        if (!tempFile.replaceWithText(draftJsonStr)) {
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
