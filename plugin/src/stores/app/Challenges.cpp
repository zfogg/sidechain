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

  auto challengeState = stateManager.challenge;
  ChallengeState newState = challengeState->getState();
  newState.isLoading = true;
  challengeState->setState(newState);

  loadChallengesObservable().subscribe(
      [challengeState](const std::vector<MIDIChallenge> &challenges) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<Sidechain::MIDIChallenge>> challengesList;
        challengesList.reserve(challenges.size());
        for (const auto &c : challenges) {
          challengesList.push_back(std::make_shared<Sidechain::MIDIChallenge>(c));
        }

        ChallengeState successState = challengeState->getState();
        successState.challenges = std::move(challengesList);
        successState.isLoading = false;
        successState.challengeError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(successState.challenges.size()) + " challenges");
        challengeState->setState(successState);
      },
      [challengeState](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        ChallengeState errorState = challengeState->getState();
        errorState.isLoading = false;
        errorState.challengeError = errorMsg;
        Util::logError("AppStore", "Failed to load challenges: " + errorMsg);
        challengeState->setState(errorState);
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

  submitChallengeObservable(challengeId, midiFile)
      .subscribe([challengeId](int) { Util::logInfo("AppStore", "Successfully submitted challenge " + challengeId); },
                 [challengeId](std::exception_ptr ep) {
                   juce::String errorMsg;
                   try {
                     std::rethrow_exception(ep);
                   } catch (const std::exception &e) {
                     errorMsg = e.what();
                   }
                   Util::logError("AppStore", "Failed to submit challenge " + challengeId + ": " + errorMsg);
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

rxcpp::observable<AppStore::MIDIChallengeDetailResult>
AppStore::getMIDIChallengeObservable(const juce::String &challengeId) {
  return rxcpp::sources::create<MIDIChallengeDetailResult>([this, challengeId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot get MIDI challenge - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Getting MIDI challenge via observable: " + challengeId);

           networkClient->getMIDIChallenge(challengeId, [observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Got MIDI challenge via observable");
               MIDIChallengeDetailResult detailResult;

               const auto &response = result.getValue();

               // Parse challenge
               if (response.hasProperty("challenge")) {
                 detailResult.challenge = MIDIChallenge::fromJSON(response["challenge"]);
               } else {
                 detailResult.challenge = MIDIChallenge::fromJSON(response);
               }

               // Parse entries
               juce::var entriesVar;
               if (response.hasProperty("challenge") && response["challenge"].hasProperty("entries")) {
                 entriesVar = response["challenge"]["entries"];
               } else if (response.hasProperty("entries")) {
                 entriesVar = response["entries"];
               }

               if (entriesVar.isArray()) {
                 detailResult.entries.reserve(static_cast<size_t>(entriesVar.size()));
                 for (int i = 0; i < entriesVar.size(); ++i) {
                   detailResult.entries.push_back(MIDIChallengeEntry::fromJSON(entriesVar[i]));
                 }
               }

               observer.on_next(detailResult);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to get MIDI challenge: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::voteMIDIChallengeEntryObservable(const juce::String &challengeId,
                                                                  const juce::String &entryId) {
  return rxcpp::sources::create<int>([this, challengeId, entryId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Cannot vote for MIDI challenge entry - network client not set");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
             return;
           }

           Util::logInfo("AppStore", "Voting for MIDI challenge entry via observable: " + entryId);

           networkClient->voteMIDIChallengeEntry(challengeId, entryId, [observer, entryId](Outcome<juce::var> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore", "Voted for entry via observable: " + entryId);
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to vote for entry: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
