#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadDrafts() {
  updateState([](AppState &state) { state.drafts.isLoading = true; });

  // Drafts are stored locally in the DraftStorage
  // This method loads them from the local storage
  juce::Array<juce::var> draftsList;

  // TODO: Load drafts from local storage (DraftStorage)
  // For now, just mark as loaded with empty list
  updateState([draftsList](AppState &state) {
    state.drafts.drafts = draftsList;
    state.drafts.isLoading = false;
    state.drafts.draftError = "";
    Util::logInfo("AppStore", "Loaded " + juce::String(draftsList.size()) + " drafts");
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

    // TODO: Delete from local storage (DraftStorage)
  });

  notifyObservers();
}

void AppStore::clearAutoRecoveryDraft() {
  Util::logInfo("AppStore", "Clearing auto-recovery draft");

  // TODO: Clear auto-recovery draft from local storage (DraftStorage)

  updateState([](AppState &state) { state.drafts.draftError = ""; });

  notifyObservers();
}

} // namespace Stores
} // namespace Sidechain
