#pragma once

#include "../../models/Story.h"
#include <JuceHeader.h>

class NetworkClient;

//==============================================================================
/**
 * SelectHighlightDialog - Modal for selecting a highlight to add a story to
 *
 * Features:
 * - Displays existing highlights in a scrollable list
 * - "Create New" option at the top
 * - Visual feedback on selection
 * - Inline creation if no highlights exist
 */
class SelectHighlightDialog : public juce::Component, public juce::Button::Listener, public juce::ScrollBar::Listener {
public:
  SelectHighlightDialog();
  ~SelectHighlightDialog() override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void buttonClicked(juce::Button *button) override;
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  // Setup
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }
  void setStoryId(const juce::String &id) {
    storyId = id;
  }

  // Callbacks
  std::function<void(const juce::String &highlightId)> onHighlightSelected;
  std::function<void()> onCreateNewClicked; // Opens create dialog
  std::function<void()> onCancelled;

  // Show as modal overlay
  void showModal(juce::Component *parentComponent);
  void closeDialog();

  // Load highlights for current user
  void loadHighlights();

private:
  NetworkClient *networkClient = nullptr;
  juce::String currentUserId;
  juce::String storyId;

  // Data
  juce::Array<StoryHighlight> highlights;
  bool isLoading = false;
  bool isAddingToHighlight = false;
  juce::String errorMessage;

  // Scroll
  std::unique_ptr<juce::ScrollBar> scrollBar;
  double scrollOffset = 0.0;

  // Buttons
  std::unique_ptr<juce::TextButton> cancelButton;

  // Cover images cache
  std::map<juce::String, juce::Image> coverImages;

  // Layout constants
  static constexpr int DIALOG_WIDTH = 400;
  static constexpr int DIALOG_HEIGHT = 450;
  static constexpr int PADDING = 20;
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int ITEM_HEIGHT = 70;
  static constexpr int CREATE_NEW_HEIGHT = 60;
  static constexpr int BUTTON_HEIGHT = 44;

  // Drawing
  void drawHeader(juce::Graphics &g);
  void drawHighlightsList(juce::Graphics &g);
  void drawHighlightItem(juce::Graphics &g, const StoryHighlight &highlight, juce::Rectangle<int> bounds);
  void drawCreateNewItem(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawLoadingState(juce::Graphics &g);
  void drawEmptyState(juce::Graphics &g);
  void drawError(juce::Graphics &g);

  // Hit testing
  int getHighlightIndexAt(juce::Point<int> pos) const;
  bool isCreateNewAt(juce::Point<int> pos) const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getHighlightBounds(int index) const;
  juce::Rectangle<int> getCreateNewBounds() const;

  // Actions
  void addStoryToHighlight(const juce::String &highlightId);
  void loadCoverImage(const StoryHighlight &highlight);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SelectHighlightDialog)
};
