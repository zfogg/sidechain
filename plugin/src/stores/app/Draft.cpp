#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/PropertiesFileUtils.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

void AppStore::loadDrafts() {
  updateState([](AppState &state) { state.drafts.isLoading = true; });

  // Load drafts from local storage (PropertiesFile)
  juce::Array<juce::var> draftsList;

  try {
    auto options = Util::PropertiesFileUtils::getStandardOptions();
    std::unique_ptr<juce::PropertiesFile> propertiesFile(new juce::PropertiesFile(options));

    // Get the drafts JSON string from properties
    juce::String draftsJson = propertiesFile->getValue("drafts", "[]");

    // Parse JSON array
    juce::var parsed = juce::JSON::parse(draftsJson);
    if (parsed.isArray()) {
      draftsList = juce::Array<juce::var>(static_cast<const juce::Array<juce::var>&>(parsed));
      Util::logInfo("AppStore", "Loaded " + juce::String(draftsList.size()) + " drafts from local storage");
    } else {
      Util::logWarning("AppStore", "Drafts JSON is not an array, starting with empty list");
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

  // Persist changes to local storage
  try {
    auto options = Util::PropertiesFileUtils::getStandardOptions();
    std::unique_ptr<juce::PropertiesFile> propertiesFile(new juce::PropertiesFile(options));

    // Convert current drafts to JSON and save
    juce::var draftsArray = getState().drafts.drafts;
    juce::String draftsJson = juce::JSON::toString(draftsArray);
    propertiesFile->setValue("drafts", draftsJson);
    propertiesFile->save();

    Util::logInfo("AppStore", "Persisted draft deletion to local storage");
  } catch (const std::exception &e) {
    Util::logError("AppStore", "Failed to persist draft deletion: " + juce::String(e.what()));
  }

  notifyObservers();
}

void AppStore::clearAutoRecoveryDraft() {
  Util::logInfo("AppStore", "Clearing auto-recovery draft");

  // Clear auto-recovery draft from local storage
  try {
    auto options = Util::PropertiesFileUtils::getStandardOptions();
    std::unique_ptr<juce::PropertiesFile> propertiesFile(new juce::PropertiesFile(options));

    // Remove the auto-recovery draft entry
    propertiesFile->removeValue("autoRecoveryDraft");
    propertiesFile->save();

    Util::logInfo("AppStore", "Auto-recovery draft cleared from local storage");
  } catch (const std::exception &e) {
    Util::logError("AppStore", "Failed to clear auto-recovery draft: " + juce::String(e.what()));
  }

  updateState([](AppState &state) { state.drafts.draftError = ""; });

  notifyObservers();
}

} // namespace Stores
} // namespace Sidechain
