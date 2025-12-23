#include "DraftsView.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/Time.h"
#include "Upload.h" // For key/genre names

// ==============================================================================
DraftsView::DraftsView(Sidechain::Stores::AppStore *store)
    : AppStoreComponent(
          store, [store](auto cb) { return store ? store->subscribeToDrafts(cb) : std::function<void()>([]() {}); }) {
  scrollBar = std::make_unique<juce::ScrollBar>(true);
  scrollBar->addListener(this);
  scrollBar->setAutoHide(true);
  addAndMakeVisible(scrollBar.get());
}

DraftsView::~DraftsView() {
  if (scrollBar)
    scrollBar->removeListener(this);
}

// ==============================================================================

void DraftsView::onAppStateChanged(const Sidechain::Stores::DraftState & /*state*/) {
  repaint();
}

// ==============================================================================
void DraftsView::refresh() {
  isLoading = true;
  repaint();

  Sidechain::Stores::AppStore::getInstance().loadDrafts();
}

void DraftsView::loadDrafts() {
  refresh();
}

// ==============================================================================
void DraftsView::paint(juce::Graphics &g) {
  g.fillAll(SidechainColors::background());

  drawHeader(g);

  if (showingDeleteConfirmation) {
    drawDeleteConfirmation(g);
    return;
  }

  auto contentBounds = getContentBounds();

  // Recovery banner
  if (hasRecoveryDraft) {
    drawRecoveryBanner(g);
  }

  // Drafts list or empty state
  if (drafts.isEmpty()) {
    drawEmptyState(g);
  } else {
    // Draw visible draft cards
    for (int i = 0; i < drafts.size(); ++i) {
      auto cardBounds = getDraftCardBounds(i);
      if (cardBounds.getBottom() > contentBounds.getY() && cardBounds.getY() < contentBounds.getBottom()) {
        drawDraftCard(g, drafts[i], cardBounds, i);
      }
    }
  }
}

void DraftsView::drawHeader(juce::Graphics &g) {
  auto bounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);

  // Background
  g.setColour(SidechainColors::backgroundLight());
  g.fillRect(bounds);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(SidechainColors::textSecondary());
  g.setFont(20.0f);
  g.drawText(juce::String::fromUTF8("\xE2\x86\x90"), backBounds,
             juce::Justification::centred); // <-

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
  g.drawText("Drafts", bounds.withTrimmedLeft(50), juce::Justification::centredLeft);

  // Draft count
  if (!drafts.isEmpty()) {
    g.setColour(SidechainColors::textSecondary());
    g.setFont(14.0f);
    g.drawText(juce::String(drafts.size()) + " draft" + (drafts.size() != 1 ? "s" : ""), bounds.withTrimmedRight(60),
               juce::Justification::centredRight);
  }

  // New Recording button
  auto newBounds = getNewRecordingButtonBounds();
  g.setColour(SidechainColors::primary());
  g.fillRoundedRectangle(newBounds.toFloat(), 6.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(12.0f);
  g.drawText("+", newBounds, juce::Justification::centred);
}

void DraftsView::drawRecoveryBanner(juce::Graphics &g) {
  auto bounds = getRecoveryBannerBounds();

  // Background
  g.setColour(SidechainColors::warning().darker(0.5f)); // Warm amber background
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Border
  g.setColour(SidechainColors::warning());
  g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 8.0f, 2.0f);

  // Icon
  g.setColour(SidechainColors::warning());
  g.setFont(24.0f);
  g.drawText(juce::String::fromUTF8("\xE2\x9A\xA0"), // Warning symbol
             bounds.withWidth(50), juce::Justification::centred);

  // Text
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
  g.drawText("Unsaved recording found",
             bounds.withTrimmedLeft(50).withTrimmedRight(180).withTrimmedBottom(bounds.getHeight() / 2),
             juce::Justification::bottomLeft);

  g.setColour(SidechainColors::textSecondary());
  g.setFont(12.0f);
  g.drawText("Would you like to restore it?",
             bounds.withTrimmedLeft(50).withTrimmedRight(180).withTrimmedTop(bounds.getHeight() / 2),
             juce::Justification::topLeft);

  // Restore button
  auto restoreBounds = getRecoveryRestoreButtonBounds();
  g.setColour(SidechainColors::primary());
  g.fillRoundedRectangle(restoreBounds.toFloat(), 4.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(12.0f);
  g.drawText("Restore", restoreBounds, juce::Justification::centred);

  // Discard button
  auto discardBounds = getRecoveryDiscardButtonBounds();
  g.setColour(SidechainColors::buttonSecondary());
  g.fillRoundedRectangle(discardBounds.toFloat(), 4.0f);
  g.setColour(SidechainColors::textSecondary());
  g.drawText("Discard", discardBounds, juce::Justification::centred);
}

void DraftsView::drawDraftCard(juce::Graphics &g, const juce::var &draft, juce::Rectangle<int> bounds, int index) {
  bool isHovered = (index == hoveredDraftIndex);

  // Card background
  g.setColour(isHovered ? SidechainColors::surfaceHover() : SidechainColors::surface());
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Card border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 8.0f, 1.0f);

  int x = bounds.getX() + PADDING;
  int y = bounds.getY() + PADDING;
  int contentWidth = bounds.getWidth() - PADDING * 2 - 100; // Leave space for buttons

  // Filename or "Untitled"
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
  juce::String filename = draft.getProperty("filename", "").toString();
  juce::String displayName = filename.isEmpty() ? "Untitled Draft" : filename;
  g.drawText(displayName, x, y, contentWidth, 22, juce::Justification::centredLeft);

  // Duration and date
  g.setColour(SidechainColors::textSecondary());
  g.setFont(13.0f);
  auto duration = static_cast<float>(draft.getProperty("duration_ms", 0.0));
  juce::Time updated = juce::Time::fromISO8601(draft.getProperty("updated_at", "").toString());
  juce::String durationStr = juce::String(static_cast<int>(duration / 1000.0f)) + "s";
  juce::String info = durationStr + " | " + Sidechain::TimeUtils::formatTimeAgo(updated);
  g.drawText(info, x, y + 24, contentWidth, 18, juce::Justification::centredLeft);

  // BPM and Key
  juce::String metadata;
  int bpm = static_cast<int>(draft.getProperty("bpm", 0));
  if (bpm > 0)
    metadata += juce::String(bpm) + " BPM";

  int keyIndex = static_cast<int>(draft.getProperty("key_index", 0));
  if (keyIndex > 0 && keyIndex < Upload::NUM_KEYS) {
    if (metadata.isNotEmpty())
      metadata += " | ";
    metadata += Upload::getMusicalKeys()[static_cast<size_t>(keyIndex)].name;
  }

  if (metadata.isNotEmpty()) {
    g.setFont(12.0f);
    g.drawText(metadata, x, y + 44, contentWidth, 16, juce::Justification::centredLeft);
  }

  // MIDI indicator
  bool hasMidi = static_cast<bool>(draft.getProperty("has_midi", false));
  if (hasMidi) {
    g.setColour(SidechainColors::accent());
    g.setFont(11.0f);
    g.drawText("MIDI", x + contentWidth - 40, y + 44, 40, 16, juce::Justification::centredRight);
  }

  // Resume button
  auto resumeBounds = getDraftResumeButtonBounds(index);
  bool resumeHovered = isHovered && (hoveredButtonType == 0);
  g.setColour(resumeHovered ? SidechainColors::primary().brighter(0.2f) : SidechainColors::primary());
  g.fillRoundedRectangle(resumeBounds.toFloat(), 4.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(12.0f);
  g.drawText("Resume", resumeBounds, juce::Justification::centred);

  // Delete button
  auto deleteBounds = getDraftDeleteButtonBounds(index);
  bool deleteHovered = isHovered && (hoveredButtonType == 1);
  g.setColour(deleteHovered ? SidechainColors::buttonDanger().brighter(0.2f) : SidechainColors::buttonSecondary());
  g.fillRoundedRectangle(deleteBounds.toFloat(), 4.0f);
  g.setColour(deleteHovered ? SidechainColors::buttonDanger() : SidechainColors::textSecondary());
  g.setFont(14.0f);
  g.drawText(juce::String::fromUTF8("\xF0\x9F\x97\x91"), deleteBounds,
             juce::Justification::centred); // Trash icon
}

void DraftsView::drawEmptyState(juce::Graphics &g) {
  auto bounds = getContentBounds();

  g.setColour(SidechainColors::textSecondary());
  g.setFont(48.0f);
  g.drawText(juce::String::fromUTF8("\xF0\x9F\x93\x9D"), // Memo icon
             bounds.withHeight(60).withY(bounds.getCentreY() - 60), juce::Justification::centred);

  g.setFont(16.0f);
  g.drawText("No drafts yet", bounds.withHeight(24).withY(bounds.getCentreY() + 10), juce::Justification::centred);

  g.setFont(13.0f);
  g.drawText("Save recordings as drafts to continue later", bounds.withHeight(20).withY(bounds.getCentreY() + 38),
             juce::Justification::centred);
}

void DraftsView::drawDeleteConfirmation(juce::Graphics &g) {
  // Dim background
  g.setColour(juce::Colours::black.withAlpha(0.6f));
  g.fillRect(getLocalBounds());

  // Dialog box
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(300, 180);
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(dialogBounds.toFloat(), 12.0f);

  // Title
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
  g.drawText("Delete Draft?", dialogBounds.withHeight(50), juce::Justification::centred);

  // Message
  g.setColour(SidechainColors::textSecondary());
  g.setFont(13.0f);
  g.drawText("This action cannot be undone.", dialogBounds.withTrimmedTop(50).withHeight(40),
             juce::Justification::centred);

  // Confirm button
  auto confirmBounds = getConfirmDeleteButtonBounds();
  g.setColour(SidechainColors::buttonDanger());
  g.fillRoundedRectangle(confirmBounds.toFloat(), 6.0f);
  g.setColour(SidechainColors::textPrimary());
  g.setFont(14.0f);
  g.drawText("Delete", confirmBounds, juce::Justification::centred);

  // Cancel button
  auto cancelBounds = getCancelDeleteButtonBounds();
  g.setColour(SidechainColors::buttonSecondary());
  g.fillRoundedRectangle(cancelBounds.toFloat(), 6.0f);
  g.setColour(SidechainColors::textSecondary());
  g.drawText("Cancel", cancelBounds, juce::Justification::centred);
}

// ==============================================================================
void DraftsView::resized() {
  auto bounds = getLocalBounds();

  // Scroll bar on right
  scrollBar->setBounds(bounds.getRight() - 10, HEADER_HEIGHT, 10, bounds.getHeight() - HEADER_HEIGHT);

  // Update scroll range
  int contentHeight = calculateContentHeight();
  int visibleHeight = bounds.getHeight() - HEADER_HEIGHT;
  scrollBar->setRangeLimits(0.0, static_cast<double>(contentHeight));
  scrollBar->setCurrentRange(scrollOffset, static_cast<double>(visibleHeight));
}

void DraftsView::scrollBarMoved(juce::ScrollBar *, double newRangeStart) {
  scrollOffset = newRangeStart;
  repaint();
}

// ==============================================================================
void DraftsView::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  // Delete confirmation dialog
  if (showingDeleteConfirmation) {
    if (getConfirmDeleteButtonBounds().contains(pos)) {
      confirmDelete();
      return;
    }
    if (getCancelDeleteButtonBounds().contains(pos)) {
      cancelDelete();
      return;
    }
    // Click outside cancels
    cancelDelete();
    return;
  }

  // Back button
  if (getBackButtonBounds().contains(pos)) {
    if (onClose)
      onClose();
    return;
  }

  // New recording button
  if (getNewRecordingButtonBounds().contains(pos)) {
    if (onNewRecording)
      onNewRecording();
    return;
  }

  // Recovery banner buttons
  if (hasRecoveryDraft) {
    if (getRecoveryRestoreButtonBounds().contains(pos)) {
      restoreRecoveryDraft();
      return;
    }
    if (getRecoveryDiscardButtonBounds().contains(pos)) {
      discardRecoveryDraft();
      return;
    }
  }

  // Draft card buttons
  for (int i = 0; i < drafts.size(); ++i) {
    if (getDraftResumeButtonBounds(i).contains(pos)) {
      resumeDraft(i);
      return;
    }
    if (getDraftDeleteButtonBounds(i).contains(pos)) {
      draftToDeleteIndex = i;
      showingDeleteConfirmation = true;
      repaint();
      return;
    }
    // Click on card body also resumes
    if (getDraftCardBounds(i).contains(pos)) {
      resumeDraft(i);
      return;
    }
  }
}

// ==============================================================================
juce::Rectangle<int> DraftsView::getBackButtonBounds() const {
  return juce::Rectangle<int>(PADDING, (HEADER_HEIGHT - 30) / 2, 30, 30);
}

juce::Rectangle<int> DraftsView::getNewRecordingButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - PADDING - 36, (HEADER_HEIGHT - 36) / 2, 36, 36);
}

juce::Rectangle<int> DraftsView::getRecoveryBannerBounds() const {
  return juce::Rectangle<int>(PADDING, HEADER_HEIGHT + PADDING, getWidth() - PADDING * 2 - 10, RECOVERY_BANNER_HEIGHT);
}

juce::Rectangle<int> DraftsView::getRecoveryRestoreButtonBounds() const {
  auto banner = getRecoveryBannerBounds();
  return juce::Rectangle<int>(banner.getRight() - 80 - PADDING, banner.getY() + 15, 70, 28);
}

juce::Rectangle<int> DraftsView::getRecoveryDiscardButtonBounds() const {
  auto banner = getRecoveryBannerBounds();
  return juce::Rectangle<int>(banner.getRight() - 80 - PADDING, banner.getY() + 48, 70, 28);
}

juce::Rectangle<int> DraftsView::getContentBounds() const {
  int topOffset = HEADER_HEIGHT;
  if (hasRecoveryDraft)
    topOffset += RECOVERY_BANNER_HEIGHT + PADDING * 2;

  return juce::Rectangle<int>(0, topOffset, getWidth() - 12, getHeight() - topOffset);
}

juce::Rectangle<int> DraftsView::getDraftCardBounds(int index) const {
  int topOffset = HEADER_HEIGHT + PADDING;
  if (hasRecoveryDraft)
    topOffset += RECOVERY_BANNER_HEIGHT + PADDING;

  int y = topOffset + index * (DRAFT_CARD_HEIGHT + DRAFT_CARD_SPACING) - static_cast<int>(scrollOffset);
  return juce::Rectangle<int>(PADDING, y, getWidth() - PADDING * 2 - 10, DRAFT_CARD_HEIGHT);
}

juce::Rectangle<int> DraftsView::getDraftResumeButtonBounds(int index) const {
  auto card = getDraftCardBounds(index);
  return juce::Rectangle<int>(card.getRight() - 80 - PADDING, card.getY() + 20, 70, 28);
}

juce::Rectangle<int> DraftsView::getDraftDeleteButtonBounds(int index) const {
  auto card = getDraftCardBounds(index);
  return juce::Rectangle<int>(card.getRight() - 80 - PADDING, card.getY() + 54, 70, 28);
}

juce::Rectangle<int> DraftsView::getConfirmDeleteButtonBounds() const {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(300, 180);
  return juce::Rectangle<int>(dialogBounds.getX() + 20, dialogBounds.getBottom() - 60, 120, 40);
}

juce::Rectangle<int> DraftsView::getCancelDeleteButtonBounds() const {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(300, 180);
  return juce::Rectangle<int>(dialogBounds.getRight() - 140, dialogBounds.getBottom() - 60, 120, 40);
}

int DraftsView::getDraftIndexAt(juce::Point<int> pos) const {
  for (int i = 0; i < drafts.size(); ++i) {
    if (getDraftCardBounds(i).contains(pos))
      return i;
  }
  return -1;
}

int DraftsView::calculateContentHeight() const {
  int height = PADDING;
  if (hasRecoveryDraft)
    height += RECOVERY_BANNER_HEIGHT + PADDING;

  height += drafts.size() * (DRAFT_CARD_HEIGHT + DRAFT_CARD_SPACING);
  return height;
}

// ==============================================================================
void DraftsView::resumeDraft(int index) {
  if (index < 0 || index >= drafts.size())
    return;

  juce::String draftId = drafts[index].getProperty("id", "").toString();
  Log::info("DraftsView: Resuming draft " + draftId);

  if (onDraftSelected)
    onDraftSelected(drafts[index]);
}

void DraftsView::deleteDraft(int index) {
  if (index < 0 || index >= drafts.size())
    return;

  juce::String draftId = drafts[index].getProperty("id", "").toString();
  Log::info("DraftsView: Deleting draft " + draftId);

  Sidechain::Stores::AppStore::getInstance().deleteDraft(draftId);
}

void DraftsView::confirmDelete() {
  if (draftToDeleteIndex >= 0) {
    deleteDraft(draftToDeleteIndex);
  }
  showingDeleteConfirmation = false;
  draftToDeleteIndex = -1;
  repaint();
}

void DraftsView::cancelDelete() {
  showingDeleteConfirmation = false;
  draftToDeleteIndex = -1;
  repaint();
}

void DraftsView::restoreRecoveryDraft() {
  Log::info("DraftsView: Restoring auto-recovery draft");

  juce::var recoveryDraft = juce::var(juce::DynamicObject::Ptr(new juce::DynamicObject()));
  recoveryDraft.getProperty("id", "") = "_auto_recovery";

  if (onDraftSelected)
    onDraftSelected(recoveryDraft);
}

void DraftsView::discardRecoveryDraft() {
  Log::info("DraftsView: Discarding auto-recovery draft");

  Sidechain::Stores::AppStore::getInstance().clearAutoRecoveryDraft();
  hasRecoveryDraft = false;
  resized();
  repaint();
}

