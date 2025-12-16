#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
// Draft Management

void AppStore::loadDrafts() {
  Util::logInfo("AppStore", "Loading drafts");

  // TODO: Implement draft loading from disk
}

void AppStore::deleteDraft(const juce::String &draftId) {
  if (draftId.isEmpty()) {
    return;
  }

  Util::logInfo("AppStore", "Deleting draft", "draftId=" + draftId);

  // TODO: Implement draft deletion
}

void AppStore::clearAutoRecoveryDraft() {
  Util::logInfo("AppStore", "Clearing auto recovery draft");

  // TODO: Implement auto recovery draft clearing
}

} // namespace Stores
} // namespace Sidechain
