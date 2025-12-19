#pragma once

#include "../../models/Story.h"
#include "../../stores/AppStore.h"
#include "../common/AppStoreComponent.h"
#include <JuceHeader.h>

class NetworkClient;

// ==============================================================================
/**
 * StoryHighlights displays a horizontal row of story highlight circles
 *
 * Similar to Instagram's highlights row on profile pages. Shows circular
 * icons with the highlight name below, scrollable horizontally.
 */
class StoryHighlights : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::StoriesState> {
public:
  StoryHighlights(Sidechain::Stores::AppStore *store = nullptr);
  ~StoryHighlights() override;

  // ==============================================================================
  // Configuration
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void setUserId(const juce::String &userId);
  void setIsOwnProfile(bool isOwn) {
    isOwnProfile = isOwn;
  }

  // ==============================================================================
  // Data
  void loadHighlights();
  void setHighlights(const juce::Array<StoryHighlight> &highlights);
  const juce::Array<StoryHighlight> &getHighlights() const {
    return highlights;
  }
  bool hasHighlights() const {
    return !highlights.isEmpty();
  }

  // ==============================================================================
  // Callbacks
  std::function<void(const StoryHighlight &)> onHighlightClicked;
  std::function<void()> onCreateHighlightClicked; // For "New" button on own profile

  // ==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;

  // ==============================================================================
  // Layout
  int getPreferredHeight() const {
    return HIGHLIGHT_SIZE + NAME_HEIGHT + PADDING * 2;
  }

protected:
  void onAppStateChanged(const Sidechain::Stores::StoriesState &state) override;
  void subscribeToAppStore() override;

private:
  // ==============================================================================
  // Data
  NetworkClient *networkClient = nullptr;
  juce::String userId;
  bool isOwnProfile = false;
  juce::Array<StoryHighlight> highlights;

  // Loading state
  bool isLoading = false;

  // Horizontal scroll
  int scrollOffset = 0;

  // ==============================================================================
  // Layout constants
  static constexpr int HIGHLIGHT_SIZE = 64;
  static constexpr int NAME_HEIGHT = 20;
  static constexpr int SPACING = 16;
  static constexpr int PADDING = 12;
  static constexpr int ADD_BUTTON_SIZE = 64;

  // ==============================================================================
  // Drawing methods
  void drawHighlight(juce::Graphics &g, const StoryHighlight &highlight, juce::Rectangle<int> bounds);
  void drawAddButton(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawLoadingState(juce::Graphics &g);

  // ==============================================================================
  // Hit testing
  int getHighlightIndexAt(juce::Point<int> pos) const;
  bool isAddButtonAt(juce::Point<int> pos) const;
  juce::Rectangle<int> getHighlightBounds(int index) const;
  juce::Rectangle<int> getAddButtonBounds() const;

  // ==============================================================================
  // Image loading
  void loadCoverImage(const StoryHighlight &highlight);

  // ==============================================================================
  // Colors
  struct Colors {
    static inline juce::Colour background{0xff1a1a1e};
    static inline juce::Colour highlightRing{0xff00d4ff};
    static inline juce::Colour highlightRingSeen{0xff4a4a4e};
    static inline juce::Colour textPrimary{0xffffffff};
    static inline juce::Colour textSecondary{0xffa0a0a0};
    static inline juce::Colour addButtonBg{0xff2d2d32};
    static inline juce::Colour addButtonIcon{0xff00d4ff};
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StoryHighlights)
};
