#pragma once

#include "../models/MidiChallenge.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * ChallengeState - Immutable state for MIDI challenges
 */
struct ChallengeState {
  enum class FilterType { All, Active, Voting, Past, Upcoming };

  juce::Array<MIDIChallenge> allChallenges;
  juce::Array<MIDIChallenge> filteredChallenges;
  FilterType currentFilter = FilterType::Active;
  bool isLoading = false;
  juce::String errorMessage;
  int64_t lastUpdated = 0;
};

/**
 * ChallengeStore - Reactive store for managing MIDI challenges (R.2.2.4.1)
 *
 * Features:
 * - Load active and upcoming MIDI challenges
 * - Filter challenges by status (Active, Voting, Past, Upcoming)
 * - Track loading state and errors
 * - Pagination support (if needed)
 *
 * Usage:
 * ```cpp
 * auto challengeStore = std::make_shared<ChallengeStore>(networkClient);
 * challengeStore->subscribe([this](const ChallengeState& state) {
 *   updateChallengesUI(state.filteredChallenges);
 * });
 * challengeStore->loadChallenges();
 * challengeStore->filterChallenges(ChallengeState::FilterType::Active);
 * ```
 */
class ChallengeStore : public Store<ChallengeState> {
public:
  explicit ChallengeStore(NetworkClient *client);
  ~ChallengeStore() override = default;

  //==============================================================================
  // Data Loading
  void loadChallenges();
  void refreshChallenges();

  //==============================================================================
  // Filtering
  void filterChallenges(ChallengeState::FilterType filterType);

  //==============================================================================
  // Current State Access
  bool isLoading() const {
    return getState().isLoading;
  }

  juce::Array<MIDIChallenge> getAllChallenges() const {
    return getState().allChallenges;
  }

  juce::Array<MIDIChallenge> getFilteredChallenges() const {
    return getState().filteredChallenges;
  }

  ChallengeState::FilterType getCurrentFilter() const {
    return getState().currentFilter;
  }

  juce::String getError() const {
    return getState().errorMessage;
  }

  int getChallengeCount() const {
    return getState().filteredChallenges.size();
  }

protected:
  //==============================================================================
  // Helper methods
  void updateChallenges(const juce::Array<MIDIChallenge> &challenges);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handleChallengesLoaded(Outcome<juce::var> result);

  //==============================================================================
  // Filtering logic
  juce::Array<MIDIChallenge> applyChallengeFilter(const juce::Array<MIDIChallenge> &challenges,
                                                  ChallengeState::FilterType filter);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChallengeStore)
};

} // namespace Stores
} // namespace Sidechain
