#pragma once

#include "../../stores/DraftStorage.h"
#include <JuceHeader.h>

//==============================================================================
/**
 * DraftsView - View for managing saved recording drafts
 *
 * Features:
 * - Scrollable list of draft cards
 * - Preview info (duration, BPM, key, date)
 * - Resume editing button
 * - Delete button with confirmation
 * - Empty state when no drafts
 * - Auto-recovery prompt at top
 */
class DraftsView : public juce::Component, public juce::ScrollBar::Listener {
public:
  DraftsView();
  ~DraftsView() override;

  //==========================================================================
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;

  //==========================================================================
  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  //==========================================================================
  // Set draft storage reference
  void setDraftStorage(DraftStorage *storage);

  // Reload drafts list
  void refresh();

  //==========================================================================
  // Callbacks
  std::function<void(const Draft &)> onDraftSelected; // Resume editing
  std::function<void()> onClose;                      // Close view
  std::function<void()> onNewRecording;               // Start new recording

private:
  DraftStorage *draftStorage = nullptr;
  juce::Array<Draft> drafts;
  bool hasRecoveryDraft = false;

  // UI State
  int hoveredDraftIndex = -1;
  int hoveredButtonType = -1; // 0 = resume, 1 = delete
  bool isLoading = false;
  juce::String errorMessage;

  // Scroll
  std::unique_ptr<juce::ScrollBar> scrollBar;
  double scrollOffset = 0.0;

  // Confirmation dialog state
  bool showingDeleteConfirmation = false;
  int draftToDeleteIndex = -1;

  // Layout constants
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int DRAFT_CARD_HEIGHT = 100;
  static constexpr int DRAFT_CARD_SPACING = 12;
  static constexpr int RECOVERY_BANNER_HEIGHT = 80;
  static constexpr int PADDING = 16;
  static constexpr int BUTTON_HEIGHT = 36;

  //==========================================================================
  // Drawing
  void drawHeader(juce::Graphics &g);
  void drawRecoveryBanner(juce::Graphics &g);
  void drawDraftCard(juce::Graphics &g, const Draft &draft, juce::Rectangle<int> bounds, int index);
  void drawEmptyState(juce::Graphics &g);
  void drawDeleteConfirmation(juce::Graphics &g);

  // Hit testing
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getNewRecordingButtonBounds() const;
  juce::Rectangle<int> getRecoveryBannerBounds() const;
  juce::Rectangle<int> getRecoveryRestoreButtonBounds() const;
  juce::Rectangle<int> getRecoveryDiscardButtonBounds() const;
  juce::Rectangle<int> getDraftCardBounds(int index) const;
  juce::Rectangle<int> getDraftResumeButtonBounds(int index) const;
  juce::Rectangle<int> getDraftDeleteButtonBounds(int index) const;
  juce::Rectangle<int> getContentBounds() const;
  juce::Rectangle<int> getConfirmDeleteButtonBounds() const;
  juce::Rectangle<int> getCancelDeleteButtonBounds() const;

  int getDraftIndexAt(juce::Point<int> pos) const;

  // Actions
  void loadDrafts();
  void resumeDraft(int index);
  void deleteDraft(int index);
  void confirmDelete();
  void cancelDelete();
  void restoreRecoveryDraft();
  void discardRecoveryDraft();

  // Helpers
  int calculateContentHeight() const;
  juce::String formatRelativeTime(const juce::Time &time) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DraftsView)
};
