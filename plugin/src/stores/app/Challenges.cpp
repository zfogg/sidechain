#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadChallenges() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load challenges - network client not set");
    return;
  }

  updateState([](AppState &state) { state.challenges.isLoading = true; });

  networkClient->getMIDIChallenges("", [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> challengesList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          challengesList.add(data[i]);
        }
      }

      updateState([challengesList](AppState &state) {
        state.challenges.challenges = challengesList;
        state.challenges.isLoading = false;
        state.challenges.challengeError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(challengesList.size()) + " challenges");
      });
    } else {
      updateState([result](AppState &state) {
        state.challenges.isLoading = false;
        state.challenges.challengeError = result.getError();
        Util::logError("AppStore", "Failed to load challenges: " + result.getError());
      });
    }

    notifyObservers();
  });
}

void AppStore::submitChallenge(const juce::String &challengeId, const juce::File &midiFile) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot submit challenge - network client not set");
    return;
  }

  if (!midiFile.existsAsFile()) {
    Util::logError("AppStore", "MIDI file does not exist: " + midiFile.getFullPathName());
    return;
  }

  Util::logInfo("AppStore", "Submitting challenge " + challengeId + " with MIDI file: " + midiFile.getFileName());

  // TODO: Implement MIDI challenge submission with audio upload
  Util::logDebug("AppStore", "Challenge submission pending implementation");
}

} // namespace Stores
} // namespace Sidechain
