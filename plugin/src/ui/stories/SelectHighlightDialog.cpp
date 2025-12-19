#include "SelectHighlightDialog.h"
#include "../../network/NetworkClient.h"

#include "../../util/Colors.h"
#include "../../util/Json.h"
#include "../../util/Log.h"

// ==============================================================================
SelectHighlightDialog::SelectHighlightDialog() {
  // Scroll bar
  scrollBar = std::make_unique<juce::ScrollBar>(true);
  scrollBar->addListener(this);
  scrollBar->setRangeLimits(0.0, 0.0);
  addAndMakeVisible(scrollBar.get());

  // Cancel button
  cancelButton = std::make_unique<juce::TextButton>("Cancel");
  cancelButton->addListener(this);
  cancelButton->setColour(juce::TextButton::buttonColourId, SidechainColors::surface());
  cancelButton->setColour(juce::TextButton::textColourOnId, SidechainColors::textPrimary());
  cancelButton->setColour(juce::TextButton::textColourOffId, SidechainColors::textPrimary());
  addAndMakeVisible(cancelButton.get());

  // Set size last to avoid resized being called before components are created
  setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

SelectHighlightDialog::~SelectHighlightDialog() {
  scrollBar->removeListener(this);
}

// ==============================================================================
void SelectHighlightDialog::paint(juce::Graphics &g) {
  // Semi-transparent backdrop
  g.fillAll(juce::Colours::black.withAlpha(0.6f));

  // Dialog background
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);

  // Shadow
  g.setColour(juce::Colours::black.withAlpha(0.3f));
  g.fillRoundedRectangle(dialogBounds.toFloat().translated(4, 4), 12.0f);

  // Background
  g.setColour(SidechainColors::backgroundLight());
  g.fillRoundedRectangle(dialogBounds.toFloat(), 12.0f);

  // Border
  g.setColour(SidechainColors::border());
  g.drawRoundedRectangle(dialogBounds.toFloat(), 12.0f, 1.0f);

  drawHeader(g);

  if (isLoading)
    drawLoadingState(g);
  else if (!errorMessage.isEmpty())
    drawError(g);
  else if (highlights.isEmpty())
    drawEmptyState(g);
  else
    drawHighlightsList(g);
}

void SelectHighlightDialog::drawHeader(juce::Graphics &g) {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  auto headerBounds = dialogBounds.removeFromTop(HEADER_HEIGHT);

  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("Add to Highlight", headerBounds.reduced(PADDING, 0), juce::Justification::centredLeft);

  // Subtitle
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
  g.drawText("Select a highlight or create a new one", headerBounds.reduced(PADDING, 0).translated(0, 24),
             juce::Justification::centredLeft);
}

void SelectHighlightDialog::drawHighlightsList(juce::Graphics &g) {
  auto contentBounds = getContentBounds();

  // Clip to content area
  g.saveState();
  g.reduceClipRegion(contentBounds);

  int yOffset = contentBounds.getY() - static_cast<int>(scrollOffset);

  // Draw "Create New" option first
  auto createNewBounds =
      juce::Rectangle<int>(contentBounds.getX(), yOffset, contentBounds.getWidth() - 12, CREATE_NEW_HEIGHT);
  drawCreateNewItem(g, createNewBounds);
  yOffset += CREATE_NEW_HEIGHT + 8;

  // Draw each highlight
  for (int i = 0; i < highlights.size(); ++i) {
    auto itemBounds = juce::Rectangle<int>(contentBounds.getX(), yOffset + i * ITEM_HEIGHT,
                                           contentBounds.getWidth() - 12, ITEM_HEIGHT - 4);

    if (itemBounds.getBottom() >= contentBounds.getY() && itemBounds.getY() <= contentBounds.getBottom()) {
      drawHighlightItem(g, highlights[i], itemBounds);
    }
  }

  g.restoreState();
}

void SelectHighlightDialog::drawHighlightItem(juce::Graphics &g, const StoryHighlight &highlight,
                                              juce::Rectangle<int> bounds) {
  // Background
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Cover image or placeholder
  auto imageBounds = bounds.removeFromLeft(ITEM_HEIGHT - 8).reduced(8);
  auto it = coverImages.find(highlight.id);
  if (it != coverImages.end() && it->second.isValid()) {
    // Draw circular cover
    juce::Path clipPath;
    clipPath.addEllipse(imageBounds.toFloat());
    g.saveState();
    g.reduceClipRegion(clipPath);
    g.drawImage(it->second, imageBounds.toFloat(),
                juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.restoreState();

    // Ring
    g.setColour(SidechainColors::primary().withAlpha(0.5f));
    g.drawEllipse(imageBounds.toFloat(), 2.0f);
  } else {
    // Placeholder with initial
    g.setColour(SidechainColors::backgroundLighter());
    g.fillEllipse(imageBounds.toFloat());
    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)).boldened());
    juce::String initial = highlight.name.isNotEmpty() ? highlight.name.substring(0, 1).toUpperCase() : "?";
    g.drawText(initial, imageBounds, juce::Justification::centred);
  }

  // Text content
  auto textBounds = bounds.reduced(10, 8);

  // Name
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)).boldened());
  g.drawText(highlight.name, textBounds.removeFromTop(20), juce::Justification::centredLeft);

  // Story count
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
  juce::String countText = juce::String(highlight.storyCount) + " " + (highlight.storyCount == 1 ? "story" : "stories");
  g.drawText(countText, textBounds, juce::Justification::centredLeft);

  // Add indicator on right
  auto addBounds = bounds.removeFromRight(30).withSizeKeepingCentre(20, 20);
  g.setColour(SidechainColors::primary());
  float cx = static_cast<float>(addBounds.getCentreX());
  float cy = static_cast<float>(addBounds.getCentreY());
  g.drawLine(cx - 6, cy, cx + 6, cy, 2.0f);
  g.drawLine(cx, cy - 6, cx, cy + 6, 2.0f);
}

void SelectHighlightDialog::drawCreateNewItem(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Background with accent border
  g.setColour(SidechainColors::surface());
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
  g.setColour(SidechainColors::primary().withAlpha(0.3f));
  g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 2.0f);

  // Plus icon in circle
  auto iconBounds = bounds.removeFromLeft(CREATE_NEW_HEIGHT).reduced(12);
  g.setColour(SidechainColors::primary().withAlpha(0.2f));
  g.fillEllipse(iconBounds.toFloat());
  g.setColour(SidechainColors::primary());
  g.drawEllipse(iconBounds.toFloat(), 2.0f);

  // Plus sign
  float cx = static_cast<float>(iconBounds.getCentreX());
  float cy = static_cast<float>(iconBounds.getCentreY());
  g.drawLine(cx - 8, cy, cx + 8, cy, 2.5f);
  g.drawLine(cx, cy - 8, cx, cy + 8, 2.5f);

  // Text
  auto textBounds = bounds.reduced(10, 0);
  g.setColour(SidechainColors::primary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(15.0f)).boldened());
  g.drawText("Create New Highlight", textBounds, juce::Justification::centredLeft);
}

void SelectHighlightDialog::drawLoadingState(juce::Graphics &g) {
  auto contentBounds = getContentBounds();
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText("Loading highlights...", contentBounds, juce::Justification::centred);
}

void SelectHighlightDialog::drawEmptyState(juce::Graphics &g) {
  auto contentBounds = getContentBounds();

  // Still show "Create New" option
  auto createNewBounds = juce::Rectangle<int>(contentBounds.getX(), contentBounds.getY() + 10,
                                              contentBounds.getWidth() - 12, CREATE_NEW_HEIGHT);
  drawCreateNewItem(g, createNewBounds);

  // Message below
  auto messageBounds = contentBounds.withTrimmedTop(CREATE_NEW_HEIGHT + 40);
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText("No highlights yet.\nCreate one to save your stories!", messageBounds, juce::Justification::centredTop);
}

void SelectHighlightDialog::drawError(juce::Graphics &g) {
  auto contentBounds = getContentBounds();
  g.setColour(SidechainColors::error());
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  g.drawText(errorMessage, contentBounds, juce::Justification::centred);
}

// ==============================================================================
void SelectHighlightDialog::resized() {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);

  // Header
  dialogBounds.removeFromTop(HEADER_HEIGHT);

  // Button at bottom
  auto buttonBounds = dialogBounds.removeFromBottom(BUTTON_HEIGHT + PADDING);
  cancelButton->setBounds(buttonBounds.reduced(PADDING).removeFromLeft(100));

  // Scrollbar
  auto contentArea = dialogBounds.reduced(PADDING, 0);
  scrollBar->setBounds(contentArea.removeFromRight(10));

  // Update scroll range
  int totalHeight = CREATE_NEW_HEIGHT + 8 + highlights.size() * ITEM_HEIGHT;
  int visibleHeight = contentArea.getHeight();
  scrollBar->setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - visibleHeight)));
  scrollBar->setCurrentRangeStart(scrollOffset, juce::dontSendNotification);
}

void SelectHighlightDialog::mouseUp(const juce::MouseEvent &event) {
  if (isLoading || isAddingToHighlight)
    return;

  auto pos = event.getPosition();

  // Check "Create New"
  if (isCreateNewAt(pos)) {
    closeDialog();
    if (onCreateNewClicked)
      onCreateNewClicked();
    return;
  }

  // Check highlights
  int index = getHighlightIndexAt(pos);
  if (index >= 0 && index < highlights.size()) {
    addStoryToHighlight(highlights[index].id);
  }
}

void SelectHighlightDialog::buttonClicked(juce::Button *button) {
  if (button == cancelButton.get()) {
    closeDialog();
    if (onCancelled)
      onCancelled();
  }
}

void SelectHighlightDialog::scrollBarMoved(juce::ScrollBar * /*scrollBar*/, double newRangeStart) {
  scrollOffset = newRangeStart;
  repaint();
}

// ==============================================================================
juce::Rectangle<int> SelectHighlightDialog::getContentBounds() const {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  dialogBounds.removeFromTop(HEADER_HEIGHT);
  dialogBounds.removeFromBottom(BUTTON_HEIGHT + PADDING);
  return dialogBounds.reduced(PADDING, 5);
}

juce::Rectangle<int> SelectHighlightDialog::getHighlightBounds(int index) const {
  auto contentBounds = getContentBounds();
  int y = contentBounds.getY() - static_cast<int>(scrollOffset) + CREATE_NEW_HEIGHT + 8 + index * ITEM_HEIGHT;
  return juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth() - 12, ITEM_HEIGHT - 4);
}

juce::Rectangle<int> SelectHighlightDialog::getCreateNewBounds() const {
  auto contentBounds = getContentBounds();
  int y = contentBounds.getY() - static_cast<int>(scrollOffset);
  return juce::Rectangle<int>(contentBounds.getX(), y, contentBounds.getWidth() - 12, CREATE_NEW_HEIGHT);
}

int SelectHighlightDialog::getHighlightIndexAt(juce::Point<int> pos) const {
  auto contentBounds = getContentBounds();
  if (!contentBounds.contains(pos))
    return -1;

  for (int i = 0; i < highlights.size(); ++i) {
    if (getHighlightBounds(i).contains(pos))
      return i;
  }
  return -1;
}

bool SelectHighlightDialog::isCreateNewAt(juce::Point<int> pos) const {
  return getCreateNewBounds().contains(pos);
}

// ==============================================================================
void SelectHighlightDialog::showModal(juce::Component *parentComponent) {
  if (parentComponent == nullptr)
    return;

  // Reset state
  errorMessage.clear();
  scrollOffset = 0.0;
  isAddingToHighlight = false;

  // Size to fill parent
  setBounds(parentComponent->getLocalBounds());
  parentComponent->addAndMakeVisible(this);
  toFront(true);

  loadHighlights();
}

void SelectHighlightDialog::closeDialog() {
  juce::MessageManager::callAsync([this]() {
    setVisible(false);
    if (auto *parent = getParentComponent())
      parent->removeChildComponent(this);
  });
}

void SelectHighlightDialog::loadHighlights() {
  if (networkClient == nullptr) {
    errorMessage = "Not connected";
    repaint();
    return;
  }

  if (currentUserId.isEmpty()) {
    errorMessage = "User not logged in";
    repaint();
    return;
  }

  isLoading = true;
  repaint();

  juce::Component::SafePointer<SelectHighlightDialog> safeThis(this);

  networkClient->getHighlights(currentUserId, [safeThis](Outcome<juce::var> result) {
    if (safeThis == nullptr)
      return;

    safeThis->isLoading = false;

    if (result.isError()) {
      safeThis->errorMessage = "Failed to load highlights";
      safeThis->repaint();
      return;
    }

    auto response = result.getValue();
    if (Json::isObject(response)) {
      auto highlightsArray = Json::getArray(response, "highlights");
      if (Json::isArray(highlightsArray)) {
        safeThis->highlights.clear();
        for (int i = 0; i < highlightsArray.size(); ++i) {
          safeThis->highlights.add(StoryHighlight::fromJSON(highlightsArray[i]));
        }

        // Load cover images
        for (const auto &highlight : safeThis->highlights) {
          safeThis->loadCoverImage(highlight);
        }
      }
    }

    safeThis->resized(); // Update scroll bounds
    safeThis->repaint();
  });
}

void SelectHighlightDialog::addStoryToHighlight(const juce::String &highlightId) {
  if (networkClient == nullptr || storyId.isEmpty())
    return;

  isAddingToHighlight = true;
  repaint();

  juce::Component::SafePointer<SelectHighlightDialog> safeThis(this);
  networkClient->addStoryToHighlight(highlightId, storyId, [safeThis, highlightId](Outcome<juce::var> result) {
    if (safeThis == nullptr)
      return;

    safeThis->isAddingToHighlight = false;

    if (result.isError()) {
      Log::error("SelectHighlightDialog: Failed to add story - " + result.getError());
      safeThis->errorMessage = "Failed to add story to highlight";
      safeThis->repaint();
      return;
    }

    Log::info("SelectHighlightDialog: Added story to highlight " + highlightId);

    safeThis->closeDialog();
    if (safeThis->onHighlightSelected)
      safeThis->onHighlightSelected(highlightId);
  });
}

void SelectHighlightDialog::loadCoverImage(const StoryHighlight &highlight) {
  (void)highlight; // Not implemented yet
  // TODO: Load highlight cover image using ImageLoader
  // Image loading to be implemented
}
