#include "ChallengeStore.h"
#include "../util/Json.h"
#include "../util/Log.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
ChallengeStore::ChallengeStore(NetworkClient *client) : networkClient(client) {
  Log::info("ChallengeStore: Initializing");
  setState(ChallengeState{});
}

//==============================================================================
void ChallengeStore::loadChallenges() {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.isLoading = true;
  setState(state);

  Log::info("ChallengeStore: Loading MIDI challenges");

  networkClient->getMIDIChallenges("", [this](Outcome<juce::var> result) { handleChallengesLoaded(result); });
}

//==============================================================================
void ChallengeStore::refreshChallenges() {
  Log::info("ChallengeStore: Refreshing challenges");

  auto state = getState();
  state.allChallenges.clear();
  state.filteredChallenges.clear();
  setState(state);

  loadChallenges();
}

//==============================================================================
void ChallengeStore::filterChallenges(ChallengeState::FilterType filterType) {
  auto state = getState();

  if (state.currentFilter == filterType)
    return; // No change needed

  state.currentFilter = filterType;
  state.filteredChallenges = applyChallengeFilter(state.allChallenges, filterType);
  setState(state);

  Log::debug("ChallengeStore: Filtered challenges, count: " + juce::String(state.filteredChallenges.size()));
}

//==============================================================================
void ChallengeStore::handleChallengesLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isLoading = false;

  if (result.isError()) {
    Log::error("ChallengeStore: Failed to load challenges - " + result.getError());
    state.errorMessage = "Failed to load challenges";
    setState(state);
    return;
  }

  auto response = result.getValue();

  // Parse challenges from response
  juce::Array<MIDIChallenge> challenges;

  if (response.isArray()) {
    for (int i = 0; i < response.size(); ++i) {
      challenges.add(MIDIChallenge::fromJSON(response[i]));
    }
  }

  updateChallenges(challenges);
}

//==============================================================================
void ChallengeStore::updateChallenges(const juce::Array<MIDIChallenge> &challenges) {
  auto state = getState();
  state.allChallenges = challenges;
  state.filteredChallenges = applyChallengeFilter(challenges, state.currentFilter);
  state.errorMessage = "";
  state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  setState(state);

  Log::info("ChallengeStore: Loaded " + juce::String(challenges.size()) + " challenges, " +
            juce::String(state.filteredChallenges.size()) + " after filtering");
}

//==============================================================================
juce::Array<MIDIChallenge> ChallengeStore::applyChallengeFilter(const juce::Array<MIDIChallenge> &challenges,
                                                                ChallengeState::FilterType filter) {
  juce::Array<MIDIChallenge> filtered;

  if (filter == ChallengeState::FilterType::All) {
    return challenges;
  }

  // Filter based on challenge status
  for (const auto &challenge : challenges) {
    bool includeChallenge = false;

    // Determine which challenges match the filter
    // This would check challenge.status against filter type
    // For now, just add all challenges (backend status parsing needed)
    includeChallenge = true;

    if (includeChallenge) {
      filtered.add(challenge);
    }
  }

  return filtered;
}

} // namespace Stores
} // namespace Sidechain
