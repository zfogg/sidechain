#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadChallenges() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load challenges - network client not set");
    return;
  }

  auto challengeSlice = sliceManager.getChallengeSlice();
  challengeSlice->dispatch([](ChallengeState &state) { state.isLoading = true; });

  networkClient->getMIDIChallenges("", [challengeSlice](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> challengesList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          challengesList.push_back(data[i]);
        }
      }

      challengeSlice->dispatch([challengesList](ChallengeState &state) {
        state.challenges = challengesList;
        state.isLoading = false;
        state.challengeError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(challengesList.size()) + " challenges");
      });
    } else {
      challengeSlice->dispatch([result](ChallengeState &state) {
        state.isLoading = false;
        state.challengeError = result.getError();
        Util::logError("AppStore", "Failed to load challenges: " + result.getError());
      });
    }
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

  // Read MIDI file content
  auto midiContent = midiFile.loadFileAsString();
  if (midiContent.empty()) {
    Util::logError("AppStore", "Failed to read MIDI file content: " + midiFile.getFullPathName());
    return;
  }

  // Parse as JSON var (if MIDI data is in JSON format) or convert to base64
  juce::var midiData;
  try {
    // Try to parse as JSON first
    juce::var parsed = juce::JSON::parse(midiContent);
    if (!parsed.isVoid()) {
      midiData = parsed;
    } else {
      // If not JSON, store as string (base64 encoded would be ideal but keeping it simple)
      midiData = midiContent;
    }
  } catch (...) {
    // If parsing fails, just use as string
    midiData = midiContent;
  }

  // Submit challenge entry with MIDI data
  // audioUrl is empty since we're submitting MIDI data directly
  // Backend will synthesize audio from MIDI data if needed
  networkClient->submitMIDIChallengeEntry(challengeId, "", "", midiData, "", [challengeId](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      Util::logInfo("AppStore", "Successfully submitted challenge " + challengeId);
      // Update state if needed
    } else {
      Util::logError("AppStore", "Failed to submit challenge: " + result.getError());
    }
  });
}

} // namespace Stores
} // namespace Sidechain
