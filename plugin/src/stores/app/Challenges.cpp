#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadChallenges() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load challenges - network client not set");
    return;
  }

  auto challengeSlice = sliceManager.challenge;
  ChallengeState newState = challengeSlice->getState();
  newState.isLoading = true;
  challengeSlice->setState(newState);

  networkClient->getMIDIChallenges("", [challengeSlice](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      std::vector<std::shared_ptr<Sidechain::MIDIChallenge>> challengesList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          auto challenge = Sidechain::MIDIChallenge::fromJSON(data[i]);
          challengesList.push_back(std::make_shared<Sidechain::MIDIChallenge>(challenge));
        }
      }

      ChallengeState successState = challengeSlice->getState();
      successState.challenges = challengesList;
      successState.isLoading = false;
      successState.challengeError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(challengesList.size()) + " challenges");
      challengeSlice->setState(successState);
    } else {
      ChallengeState errorState = challengeSlice->getState();
      errorState.isLoading = false;
      errorState.challengeError = result.getError();
      Util::logError("AppStore", "Failed to load challenges: " + result.getError());
      challengeSlice->setState(errorState);
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
  if (midiContent.isEmpty()) {
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

// ==============================================================================
// Reactive Challenge Methods (Phase 7)

rxcpp::observable<std::vector<MIDIChallenge>> AppStore::loadChallengesObservable() {
  return rxcpp::sources::create<std::vector<MIDIChallenge>>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot load challenges - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Loading challenges observable");

           networkClient->getMIDIChallenges("", [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               const auto data = result.getValue();
               std::vector<MIDIChallenge> challenges;

               if (data.isArray()) {
                 for (int i = 0; i < data.size(); ++i) {
                   auto challenge = MIDIChallenge::fromJSON(data[i]);
                   challenges.push_back(challenge);
                 }
               }

               Util::logInfo("AppStore", "Loaded " + juce::String(challenges.size()) + " challenges via observable");
               observer.on_next(challenges);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to load challenges: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::submitChallengeObservable(const juce::String &challengeId,
                                                           const juce::File &midiFile) {
  return rxcpp::sources::create<int>([this, challengeId, midiFile](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot submit challenge - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           if (!midiFile.existsAsFile()) {
             Util::logError("AppStore", "MIDI file does not exist: " + midiFile.getFullPathName());
             observer.on_error(std::make_exception_ptr(std::runtime_error("MIDI file not found")));
             return;
           }

           Util::logInfo("AppStore",
                         "Submitting challenge " + challengeId + " with MIDI file: " + midiFile.getFileName());

           // Read MIDI file content
           auto midiContent = midiFile.loadFileAsString();
           if (midiContent.isEmpty()) {
             Util::logError("AppStore", "Failed to read MIDI file content: " + midiFile.getFullPathName());
             observer.on_error(std::make_exception_ptr(std::runtime_error("Failed to read MIDI file")));
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
               // If not JSON, store as string
               midiData = midiContent;
             }
           } catch (...) {
             // If parsing fails, just use as string
             midiData = midiContent;
           }

           networkClient->submitMIDIChallengeEntry(
               challengeId, "", "", midiData, "", [challengeId, observer](Outcome<juce::var> result) {
                 if (result.isOk()) {
                   Util::logInfo("AppStore", "Successfully submitted challenge " + challengeId);
                   observer.on_next(0);
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Failed to submit challenge: " + result.getError());
                   observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
