#include "PrivacySettings.h"
#include "../../network/NetworkClient.h"
#include "../../stores/AppStore.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/UIHelpers.h"

// ==============================================================================
PrivacySettings::PrivacySettings(AppStore *store)
    : AppStoreComponent(
          store, [store](auto cb) { return store ? store->subscribeToUser(cb) : std::function<void()>([]() {}); }) {
  Log::info("PrivacySettings: Initializing");
  setupToggle();
  // Set size last to avoid resized being called before components are created
  setSize(400, 280);
}

PrivacySettings::~PrivacySettings() {
  Log::debug("PrivacySettings: Destroying");
}

// ==============================================================================
void PrivacySettings::onAppStateChanged(const UserState & /*state*/) {
  // Update privacy settings from user state if available
  // Note: Privacy settings might need to be loaded separately via NetworkClient
  repaint();
}

// ==============================================================================
void PrivacySettings::setupToggle() {
  // Create toggle button using UIHelpers
  privateAccountToggle = std::make_unique<juce::ToggleButton>();
  UIHelpers::setupToggleButton(*privateAccountToggle, "Make Account Private", Colors::textPrimary, Colors::accent,
                               Colors::textSecondary, false,
                               [this]() { handleToggleChange(privateAccountToggle.get()); });
  addAndMakeVisible(privateAccountToggle.get());

  // Close button
  closeButton = std::make_unique<juce::TextButton>("Close");
  closeButton->setColour(juce::TextButton::buttonColourId, Colors::closeButton);
  closeButton->setColour(juce::TextButton::textColourOffId, Colors::textSecondary);
  closeButton->addListener(this);
  addAndMakeVisible(closeButton.get());
}

void PrivacySettings::loadSettings() {
  if (networkClient == nullptr) {
    Log::error("PrivacySettings: No network client set");
    return;
  }

  isLoading = true;
  errorMessage = "";
  repaint();

  networkClient->get("/settings/privacy", [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      isLoading = false;

      if (result.isOk()) {
        auto response = result.getValue();

        isPrivate = response.getProperty("is_private", false);

        populateFromSettings();
        Log::info("PrivacySettings: Settings loaded successfully");
      } else {
        errorMessage = "Failed to load settings: " + result.getError();
        Log::error("PrivacySettings: " + errorMessage);
      }

      repaint();
    });
  });
}

void PrivacySettings::populateFromSettings() {
  privateAccountToggle->setToggleState(isPrivate, juce::dontSendNotification);
}

void PrivacySettings::handleToggleChange(juce::ToggleButton *toggle) {
  // Update local state based on which toggle changed
  if (toggle == privateAccountToggle.get())
    isPrivate = toggle->getToggleState();

  // Save immediately when changed
  saveSettings();
}

void PrivacySettings::saveSettings() {
  if (networkClient == nullptr || isSaving)
    return;

  isSaving = true;
  errorMessage = "";

  // Build update payload
  auto *updateData = new juce::DynamicObject();
  updateData->setProperty("is_private", isPrivate);

  juce::var payload(updateData);

  networkClient->put("/users/me", payload, [this](Outcome<juce::var> result) {
    juce::MessageManager::callAsync([this, result]() {
      isSaving = false;

      if (result.isOk()) {
        Log::info("PrivacySettings: Settings saved successfully");
        // Privacy settings saved on backend
      } else {
        errorMessage = "Failed to save: " + result.getError();
        Log::error("PrivacySettings: " + errorMessage);
      }

      repaint();
    });
  });
}

// ==============================================================================
void PrivacySettings::paint(juce::Graphics &g) {
  // Background
  g.fillAll(Colors::background);

  // Header
  auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
  drawHeader(g, headerBounds);

  // Description text below toggle
  int y = HEADER_HEIGHT + PADDING;
  drawDescription(g, juce::Rectangle<int>(PADDING, y + TOGGLE_HEIGHT, getWidth() - PADDING * 2, DESCRIPTION_HEIGHT),
                  "Require approval for new followers and hide posts from non-followers");

  // Loading indicator
  if (isLoading) {
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
  }

  // Error message
  if (errorMessage.isNotEmpty()) {
    g.setColour(Colors::errorRed);
    g.setFont(12.0f);
    g.drawText(errorMessage, PADDING, getHeight() - 50, getWidth() - PADDING * 2, 20, juce::Justification::centred);
  }
}

void PrivacySettings::drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::headerBg);
  g.fillRect(bounds);

  // Title
  g.setColour(Colors::textPrimary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
  g.drawText("Privacy Settings", bounds, juce::Justification::centred);

  // Bottom border
  g.setColour(Colors::toggleBorder);
  g.drawLine(0, static_cast<float>(bounds.getBottom()), static_cast<float>(getWidth()),
             static_cast<float>(bounds.getBottom()), 1.0f);
}

void PrivacySettings::drawDescription(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text) {
  g.setColour(Colors::textSecondary);
  g.setFont(12.0f);
  g.drawText(text, bounds, juce::Justification::centredLeft);
}

// ==============================================================================
void PrivacySettings::resized() {
  // Close button in header
  closeButton->setBounds(getWidth() - PADDING - 60, 15, 60, 30);

  int y = HEADER_HEIGHT + PADDING;
  int toggleWidth = getWidth() - PADDING * 2;

  // Private account toggle
  privateAccountToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
}

// ==============================================================================
void PrivacySettings::buttonClicked(juce::Button *button) {
  if (button == closeButton.get()) {
    closeDialog();
  }
}

// ==============================================================================
void PrivacySettings::showModal(juce::Component *parentComponent) {
  if (parentComponent == nullptr)
    return;

  // Load settings when shown
  loadSettings();

  // Size to fill parent
  setBounds(parentComponent->getLocalBounds());
  parentComponent->addAndMakeVisible(this);
  toFront(true);
}

void PrivacySettings::closeDialog() {
  juce::MessageManager::callAsync([this]() {
    setVisible(false);
    if (auto *parent = getParentComponent())
      parent->removeChildComponent(this);
    if (onClose)
      onClose();
  });
}
