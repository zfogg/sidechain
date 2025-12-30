#include "CreateHighlightDialog.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include <nlohmann/json.hpp>

// ==============================================================================
CreateHighlightDialog::CreateHighlightDialog() {
  // Name input
  nameInput = std::make_unique<juce::TextEditor>();
  nameInput->setMultiLine(false);
  nameInput->setReturnKeyStartsNewLine(false);
  nameInput->setScrollbarsShown(false);
  nameInput->setCaretVisible(true);
  nameInput->setTextToShowWhenEmpty("Highlight name...", SidechainColors::textMuted());
  nameInput->setFont(juce::Font(juce::FontOptions().withHeight(16.0f)));
  nameInput->setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
  nameInput->setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
  nameInput->setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
  nameInput->setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
  nameInput->onTextChange = [this]() { updateCreateButtonState(); };
  addAndMakeVisible(nameInput.get());

  // Description input (optional)
  descriptionInput = std::make_unique<juce::TextEditor>();
  descriptionInput->setMultiLine(true);
  descriptionInput->setReturnKeyStartsNewLine(true);
  descriptionInput->setScrollbarsShown(true);
  descriptionInput->setCaretVisible(true);
  descriptionInput->setTextToShowWhenEmpty("Description (optional)...", SidechainColors::textMuted());
  descriptionInput->setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
  descriptionInput->setColour(juce::TextEditor::backgroundColourId, SidechainColors::surface());
  descriptionInput->setColour(juce::TextEditor::textColourId, SidechainColors::textPrimary());
  descriptionInput->setColour(juce::TextEditor::outlineColourId, SidechainColors::border());
  descriptionInput->setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
  addAndMakeVisible(descriptionInput.get());

  // Create button
  createButton = std::make_unique<juce::TextButton>("Create");
  createButton->addListener(this);
  createButton->setEnabled(false);
  createButton->setColour(juce::TextButton::buttonColourId, SidechainColors::primary());
  createButton->setColour(juce::TextButton::textColourOnId, SidechainColors::textPrimary());
  createButton->setColour(juce::TextButton::textColourOffId, SidechainColors::textPrimary());
  addAndMakeVisible(createButton.get());

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

CreateHighlightDialog::~CreateHighlightDialog() {}

// ==============================================================================
void CreateHighlightDialog::paint(juce::Graphics &g) {
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
  drawError(g);
}

void CreateHighlightDialog::drawHeader(juce::Graphics &g) {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  auto headerBounds = dialogBounds.removeFromTop(60);

  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
  g.drawText("Create Highlight", headerBounds.reduced(PADDING, 0), juce::Justification::centredLeft);

  // Subtitle
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
  g.drawText("Save your best stories to your profile", headerBounds.reduced(PADDING, 0).translated(0, 24),
             juce::Justification::centredLeft);
}

void CreateHighlightDialog::drawError(juce::Graphics &g) {
  if (errorMessage.isEmpty())
    return;

  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);
  auto errorBounds = dialogBounds.removeFromBottom(70).removeFromTop(25);

  g.setColour(SidechainColors::error());
  g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
  g.drawText(errorMessage, errorBounds.reduced(PADDING, 0), juce::Justification::centred);
}

// ==============================================================================
void CreateHighlightDialog::resized() {
  auto dialogBounds = getLocalBounds().withSizeKeepingCentre(DIALOG_WIDTH, DIALOG_HEIGHT);

  // Skip header
  dialogBounds.removeFromTop(60);

  auto contentBounds = dialogBounds.reduced(PADDING);

  // Name label + input
  contentBounds.removeFromTop(5); // Spacing
  auto nameBounds = contentBounds.removeFromTop(INPUT_HEIGHT);
  nameInput->setBounds(nameBounds);

  contentBounds.removeFromTop(15); // Spacing

  // Description label + input
  auto descBounds = contentBounds.removeFromTop(70);
  descriptionInput->setBounds(descBounds);

  // Buttons at bottom
  auto buttonBounds = dialogBounds.reduced(PADDING).removeFromBottom(BUTTON_HEIGHT);
  cancelButton->setBounds(buttonBounds.removeFromLeft(100));
  buttonBounds.removeFromLeft(10); // Spacing
  createButton->setBounds(buttonBounds.removeFromLeft(100));
}

// ==============================================================================
void CreateHighlightDialog::buttonClicked(juce::Button *button) {
  if (button == createButton.get()) {
    createHighlight();
  } else if (button == cancelButton.get()) {
    closeDialog();
    if (onCancelled)
      onCancelled();
  }
}

// ==============================================================================
void CreateHighlightDialog::showModal(juce::Component *parentComponent) {
  if (parentComponent == nullptr)
    return;

  // Reset state
  nameInput->clear();
  descriptionInput->clear();
  errorMessage.clear();
  isCreating = false;
  updateCreateButtonState();

  // Size to fill parent
  setBounds(parentComponent->getLocalBounds());
  parentComponent->addAndMakeVisible(this);
  toFront(true);
  nameInput->grabKeyboardFocus();
}

void CreateHighlightDialog::closeDialog() {
  juce::MessageManager::callAsync([this]() {
    setVisible(false);
    if (auto *parent = getParentComponent())
      parent->removeChildComponent(this);
  });
}

// ==============================================================================
void CreateHighlightDialog::createHighlight() {
  if (networkClient == nullptr) {
    errorMessage = "Not connected";
    repaint();
    return;
  }

  juce::String name = nameInput->getText().trim();
  if (name.isEmpty()) {
    errorMessage = "Please enter a name";
    repaint();
    return;
  }

  if (name.length() > 30) {
    errorMessage = "Name must be 30 characters or less";
    repaint();
    return;
  }

  isCreating = true;
  errorMessage.clear();
  createButton->setEnabled(false);
  createButton->setButtonText("Creating...");
  repaint();

  juce::String description = descriptionInput->getText().trim();

  juce::Component::SafePointer<CreateHighlightDialog> safeThis(this);
  networkClient->createHighlight(name, description, [safeThis](Outcome<nlohmann::json> result) {
    if (safeThis == nullptr)
      return;

    safeThis->isCreating = false;
    safeThis->createButton->setButtonText("Create");
    safeThis->updateCreateButtonState();

    if (result.isError()) {
      Log::error("CreateHighlightDialog: Failed to create highlight - " + result.getError());
      safeThis->errorMessage = "Failed to create highlight";
      safeThis->repaint();
      return;
    }

    // Extract highlight ID from response
    auto response = result.getValue();
    juce::String highlightId = response.value("highlight_id", "");

    Log::info("CreateHighlightDialog: Created highlight " + highlightId);

    safeThis->closeDialog();
    if (safeThis->onHighlightCreated)
      safeThis->onHighlightCreated(highlightId);
  });
}

void CreateHighlightDialog::updateCreateButtonState() {
  bool canCreate = !isCreating && nameInput->getText().trim().isNotEmpty();
  createButton->setEnabled(canCreate);
}
