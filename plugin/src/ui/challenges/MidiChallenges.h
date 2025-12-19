#pragma once

#include "../../models/MidiChallenge.h"
#include "../../stores/AppStore.h"
#include "../../util/Colors.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>
#include <memory>

// ==============================================================================
/**
 * MidiChallenges displays active MIDI challenges
 *
 * Features:
 * - List of active challenges
 * - Show challenge details, constraints, deadline
 * - Button to view entries or submit entry
 * - Filter by status (active, voting, past, upcoming)
 *
 * Inherits from AppStoreComponent for automatic reactive state binding
 */
class MidiChallenges : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChallengeState>,
                       public juce::ScrollBar::Listener {
public:
  explicit MidiChallenges(Sidechain::Stores::AppStore *store = nullptr);
  ~MidiChallenges() override;

  // ==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  // ==============================================================================
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load challenges
  void loadChallenges();
  void refresh();

  // ==============================================================================
  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void(const juce::String &challengeId)> onChallengeSelected; // Navigate to challenge detail

protected:
  // ==============================================================================
  // AppStoreComponent virtual methods
  void onAppStateChanged(const Sidechain::Stores::ChallengeState &state) override;
  void subscribeToAppStore() override;

private:
  // ==============================================================================
  // Filter types
  enum class FilterType { All, Active, Voting, Past, Upcoming };

  FilterType currentFilter = FilterType::Active;

  // ==============================================================================
  // Data
  juce::String currentUserId;
  juce::Array<MIDIChallenge> challenges;
  bool isLoading = false;
  juce::String errorMessage;

  // ==============================================================================
  // UI Components
  juce::ScrollBar scrollBar{true}; // vertical

  // Scroll state
  int scrollOffset = 0;

  // ==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int FILTER_TAB_HEIGHT = 40;
  static constexpr int CHALLENGE_CARD_HEIGHT = 140;
  static constexpr int PADDING = 16;

  // ==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g);
  void drawFilterTabs(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawChallengeCard(juce::Graphics &g, juce::Rectangle<int> bounds, const MIDIChallenge &challenge);
  void drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds);

  // ==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getFilterTabBounds(FilterType filter) const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getChallengeCardBounds(int index) const;

  // ==============================================================================
  // Network operations
  void fetchChallenges(FilterType filter);

  // ==============================================================================
  // Helper methods
  int calculateContentHeight() const;
  void updateScrollBounds();
  juce::String getStatusDisplayText(const MIDIChallenge &challenge) const;
  juce::String getTimeRemainingText(const MIDIChallenge &challenge) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiChallenges)
};
