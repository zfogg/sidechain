#pragma once

#include "../../models/MidiChallenge.h"
#include "../../util/Colors.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>

class NetworkClient;
class HttpAudioPlayer;

// ==============================================================================
/**
 * MidiChallengeDetail displays a single MIDI challenge with entries (R.2.2.4.2)
 *
 * Features:
 * - Show full challenge description
 * - List all entries with vote counts
 * - Play entries (audio + MIDI visualization)
 * - Vote button on each entry
 * - Submit entry button (if not submitted)
 * - Leaderboard showing top entries
 */
class MidiChallengeDetail : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChallengeState>,
                            public juce::ScrollBar::Listener {
public:
  MidiChallengeDetail(Sidechain::Stores::AppStore *store = nullptr);
  ~MidiChallengeDetail() override;

  // ==============================================================================
  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  // ==============================================================================
  // Network client integration
  void setNetworkClient(NetworkClient *client);
  void setAudioPlayer(HttpAudioPlayer *player);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load challenge
  void loadChallenge(const juce::String &challengeId);
  void refresh();

  // ==============================================================================
  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void()> onSubmitEntry;                              // Navigate to submission view
  std::function<void(const juce::String &entryId)> onEntrySelected; // Navigate to entry/post

protected:
  // ==============================================================================
  // AppStoreComponent virtual methods
  void onAppStateChanged(const Sidechain::Stores::ChallengeState &state) override;
  void subscribeToAppStore() override;

private:
  // ==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  HttpAudioPlayer *audioPlayer = nullptr;
  juce::String currentUserId;
  juce::String challengeId;
  MIDIChallenge challenge;
  juce::Array<MIDIChallengeEntry> entries;
  juce::String userEntryId; // ID of current user's entry (if submitted)
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
  static constexpr int INFO_HEIGHT = 180;
  static constexpr int ENTRY_CARD_HEIGHT = 90;
  static constexpr int BUTTON_HEIGHT = 44;
  static constexpr int PADDING = 16;

  // ==============================================================================
  // Drawing methods
  void drawHeader(juce::Graphics &g);
  void drawChallengeInfo(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawActionButtons(juce::Graphics &g, juce::Rectangle<int> &bounds);
  void drawEntryCard(juce::Graphics &g, juce::Rectangle<int> bounds, const MIDIChallengeEntry &entry, int index);
  void drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds);

  // ==============================================================================
  // Hit testing helpers
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getSubmitButtonBounds() const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getEntryCardBounds(int index) const;
  juce::Rectangle<int> getVoteButtonBounds(int index) const;
  juce::Rectangle<int> getPlayButtonBounds(int index) const;

  // ==============================================================================
  // Network operations
  void fetchChallenge();
  void voteForEntry(const juce::String &entryId);

  // ==============================================================================
  // Helper methods
  int calculateContentHeight() const;
  void updateScrollBounds();
  bool hasUserSubmitted() const;
  juce::String formatConstraints(const MIDIChallengeConstraints &constraints) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiChallengeDetail)
};
