#include "TwoFactorSettings.h"
#include "../../network/NetworkClient.h"
#include "../../stores/AppStore.h"
#include "../../util/Log.h"

TwoFactorSettings::TwoFactorSettings(AppStore *store) : AppStoreComponent(store) {
  // Close button
  closeButton = std::make_unique<juce::TextButton>("X");
  closeButton->addListener(this);
  addAndMakeVisible(closeButton.get());

  // Enable button (shown when 2FA is disabled)
  enableButton = std::make_unique<juce::TextButton>("Enable Two-Factor Authentication");
  enableButton->addListener(this);
  addAndMakeVisible(enableButton.get());

  // Type selection buttons
  totpButton = std::make_unique<juce::TextButton>("Authenticator App (TOTP)");
  totpButton->addListener(this);
  addChildComponent(totpButton.get());

  hotpButton = std::make_unique<juce::TextButton>("Hardware Key / YubiKey (HOTP)");
  hotpButton->addListener(this);
  addChildComponent(hotpButton.get());

  // Back button
  backButton = std::make_unique<juce::TextButton>("Back");
  backButton->addListener(this);
  addChildComponent(backButton.get());

  // Disable button (shown when 2FA is enabled)
  disableButton = std::make_unique<juce::TextButton>("Disable 2FA");
  disableButton->addListener(this);
  addChildComponent(disableButton.get());

  // Regenerate backup codes button
  regenerateButton = std::make_unique<juce::TextButton>("Regenerate Backup Codes");
  regenerateButton->addListener(this);
  addChildComponent(regenerateButton.get());

  // Copy buttons
  copySecretButton = std::make_unique<juce::TextButton>("Copy Secret");
  copySecretButton->addListener(this);
  addChildComponent(copySecretButton.get());

  copyCodesButton = std::make_unique<juce::TextButton>("Copy Backup Codes");
  copyCodesButton->addListener(this);
  addChildComponent(copyCodesButton.get());

  // Verify button
  verifyButton = std::make_unique<juce::TextButton>("Verify & Enable");
  verifyButton->addListener(this);
  addChildComponent(verifyButton.get());

  // Confirm disable button
  confirmDisableButton = std::make_unique<juce::TextButton>("Confirm Disable");
  confirmDisableButton->addListener(this);
  addChildComponent(confirmDisableButton.get());

  // Password input
  passwordInput = std::make_unique<juce::TextEditor>();
  passwordInput->setPasswordCharacter('*');
  passwordInput->setTextToShowWhenEmpty("Enter your password", Colors::textSecondary);
  passwordInput->setColour(juce::TextEditor::backgroundColourId, Colors::inputBg);
  passwordInput->setColour(juce::TextEditor::outlineColourId, Colors::inputBorder);
  passwordInput->setColour(juce::TextEditor::textColourId, Colors::textPrimary);
  addChildComponent(passwordInput.get());

  // Code input
  codeInput = std::make_unique<juce::TextEditor>();
  codeInput->setTextToShowWhenEmpty("Enter 6-digit code", Colors::textSecondary);
  codeInput->setColour(juce::TextEditor::backgroundColourId, Colors::inputBg);
  codeInput->setColour(juce::TextEditor::outlineColourId, Colors::inputBorder);
  codeInput->setColour(juce::TextEditor::textColourId, Colors::textPrimary);
  codeInput->setInputRestrictions(9, "0123456789-"); // Allow digits and dash for backup codes
  addChildComponent(codeInput.get());
  initialize();
}

TwoFactorSettings::~TwoFactorSettings() = default;

// ==============================================================================
void TwoFactorSettings::onAppStateChanged(const AuthState & [[maybe_unused]] state) {
  // Update 2FA status from auth state if available
  // Note: 2FA status might need to be loaded separately via NetworkClient
  // as it's not typically part of the core auth state
  repaint();
}

void TwoFactorSettings::subscribeToAppStore() {
  if (!appStore)
    return;

  juce::Component::SafePointer<TwoFactorSettings> safeThis(this);
  storeUnsubscriber = appStore->subscribeToAuth([safeThis](const AuthState &state) {
    if (!safeThis)
      return;
    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });
}

// ==============================================================================
void TwoFactorSettings::loadStatus() {
  if (!networkClient) {
    showError("Network client not available");
    return;
  }

  currentState = State::Loading;
  isProcessing = true;
  repaint();

  networkClient->get2FAStatus([this](auto result) {
    isProcessing = false;

    if (result.isOk()) {
      auto status = result.getValue();
      twoFactorEnabled = status.enabled;
      twoFactorType = status.type;
      backupCodesRemaining = status.backupCodesRemaining;

      if (twoFactorEnabled) {
        showEnabledState();
      } else {
        showDisabledState();
      }
    } else {
      showError(result.getError());
    }
  });
}

void TwoFactorSettings::showModal(juce::Component *parentComponent) {
  if (parentComponent) {
    auto bounds = parentComponent->getLocalBounds();
    int width = juce::jmin(450, bounds.getWidth() - 40);
    int height = juce::jmin(600, bounds.getHeight() - 40);
    setBounds(bounds.getCentreX() - width / 2, bounds.getCentreY() - height / 2, width, height);

    parentComponent->addAndMakeVisible(this);
    loadStatus();
  }
}

void TwoFactorSettings::closeDialog() {
  setVisible(false);
  if (auto *parent = getParentComponent())
    parent->removeChildComponent(this);
  if (onClose)
    onClose();
}

void TwoFactorSettings::paint(juce::Graphics &g) {
  // Background with rounded corners
  g.setColour(Colors::background);
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);

  // Border
  g.setColour(Colors::inputBorder);
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12.0f, 1.0f);

  // Header
  auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
  drawHeader(g, headerBounds);

  auto contentBounds = getLocalBounds().reduced(PADDING);
  contentBounds.removeFromTop(HEADER_HEIGHT);

  // Draw content based on state
  switch (currentState) {
  case State::Loading:
    g.setColour(Colors::textSecondary);
    g.setFont(16.0f);
    g.drawText("Loading...", contentBounds, juce::Justification::centred);
    break;

  case State::Error:
    g.setColour(Colors::errorRed);
    g.setFont(16.0f);
    g.drawText(errorMessage, contentBounds, juce::Justification::centred);
    break;

  case State::Disabled:
    drawStatus(g, contentBounds);
    break;

  case State::SetupType:
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
    g.drawText("Choose Authentication Method", contentBounds.removeFromTop(40), juce::Justification::centred);
    break;

  case State::SetupPassword:
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
    g.drawText("Enter Your Password", contentBounds.removeFromTop(40), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    g.drawText("Required to enable two-factor authentication", contentBounds.removeFromTop(30),
               juce::Justification::centred);
    break;

  case State::SetupQR:
    drawSetupInfo(g, contentBounds);
    break;

  case State::SetupVerify:
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
    g.drawText("Verify Setup", contentBounds.removeFromTop(40), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    g.drawText("Enter the code from your authenticator app", contentBounds.removeFromTop(30),
               juce::Justification::centred);
    break;

  case State::Enabled:
    drawStatus(g, contentBounds);
    break;

  case State::Disabling:
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
    g.drawText("Disable Two-Factor Authentication", contentBounds.removeFromTop(40), juce::Justification::centred);
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    g.drawText("Enter a 2FA code or your password", contentBounds.removeFromTop(30), juce::Justification::centred);
    break;
  }

  // Status message
  if (statusMessage.isNotEmpty()) {
    g.setColour(Colors::successGreen);
    g.setFont(14.0f);
    g.drawText(statusMessage, getLocalBounds().removeFromBottom(40).reduced(PADDING, 0), juce::Justification::centred);
  }
}

void TwoFactorSettings::drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::headerBg);
  g.fillRoundedRectangle(bounds.toFloat(), 12.0f);
  // Square off bottom corners
  g.fillRect(bounds.removeFromBottom(12));

  g.setColour(Colors::textPrimary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(20.0f).withStyle("Bold")));
  g.drawText("Two-Factor Authentication", bounds, juce::Justification::centred);
}

void TwoFactorSettings::drawStatus(juce::Graphics &g, juce::Rectangle<int> bounds) {
  auto statusArea = bounds.removeFromTop(80);

  // Status indicator
  juce::Colour statusColor = twoFactorEnabled ? Colors::successGreen : Colors::textSecondary;
  juce::String statusText = twoFactorEnabled ? "Enabled" : "Not Enabled";

  g.setColour(statusColor);
  g.fillEllipse(static_cast<float>(statusArea.getX() + PADDING), static_cast<float>(statusArea.getY() + 10), 12.0f,
                12.0f);

  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f).withStyle("Bold")));
  g.drawText(statusText, statusArea.withTrimmedLeft(PADDING + 20).removeFromTop(30), juce::Justification::centredLeft);

  if (twoFactorEnabled) {
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    juce::String typeLabel = twoFactorType == "hotp" ? "Hardware Key (HOTP)" : "Authenticator App (TOTP)";
    g.drawText("Method: " + typeLabel, statusArea.withTrimmedLeft(PADDING + 20).withTrimmedTop(30).removeFromTop(25),
               juce::Justification::centredLeft);
    g.drawText("Backup codes remaining: " + juce::String(backupCodesRemaining),
               statusArea.withTrimmedLeft(PADDING + 20).withTrimmedTop(55).removeFromTop(25),
               juce::Justification::centredLeft);
  } else {
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    g.drawText("Add an extra layer of security to your account",
               statusArea.withTrimmedLeft(PADDING + 20).withTrimmedTop(30), juce::Justification::centredLeft);
  }
}

void TwoFactorSettings::drawSetupInfo(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::textPrimary);
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));

  if (setupType == "hotp") {
    g.drawText("Configure Your YubiKey", bounds.removeFromTop(30), juce::Justification::centredLeft);
  } else {
    g.drawText("Scan QR Code or Enter Secret", bounds.removeFromTop(30), juce::Justification::centredLeft);
  }

  bounds.removeFromTop(SPACING);

  // QR URL (for TOTP)
  if (setupType != "hotp" && setupQRUrl.isNotEmpty()) {
    g.setColour(Colors::textSecondary);
    g.setFont(14.0f);
    g.drawText("Scan this URL as QR code in your authenticator app:", bounds.removeFromTop(25),
               juce::Justification::centredLeft);

    auto qrBounds = bounds.removeFromTop(60);
    g.setColour(Colors::codeBackground);
    g.fillRoundedRectangle(qrBounds.toFloat(), 6.0f);
    g.setColour(Colors::accent);
    g.setFont(juce::Font(juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withHeight(11.0f)));
    g.drawFittedText(setupQRUrl, qrBounds.reduced(8), juce::Justification::centredLeft, 3, 0.8f);
  }

  bounds.removeFromTop(SPACING);

  // Manual secret
  g.setColour(Colors::textSecondary);
  g.setFont(14.0f);
  g.drawText("Or enter this secret manually:", bounds.removeFromTop(25), juce::Justification::centredLeft);

  auto secretBounds = bounds.removeFromTop(40);
  g.setColour(Colors::codeBackground);
  g.fillRoundedRectangle(secretBounds.toFloat(), 6.0f);
  g.setColour(Colors::textPrimary);
  g.setFont(juce::Font(
      juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withHeight(16.0f).withStyle("Bold")));
  g.drawText(setupSecret, secretBounds, juce::Justification::centred);

  bounds.removeFromTop(SPACING * 2);

  // Backup codes
  drawBackupCodes(g, bounds);
}

void TwoFactorSettings::drawBackupCodes(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::warningOrange);
  g.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
  g.drawText("Save Your Backup Codes!", bounds.removeFromTop(25), juce::Justification::centredLeft);

  g.setColour(Colors::textSecondary);
  g.setFont(12.0f);
  g.drawText("Use these if you lose access to your authenticator", bounds.removeFromTop(20),
             juce::Justification::centredLeft);

  bounds.removeFromTop(8);

  // Draw codes in a grid
  auto codesBounds = bounds.removeFromTop(120);
  g.setColour(Colors::codeBackground);
  g.fillRoundedRectangle(codesBounds.toFloat(), 6.0f);

  g.setColour(Colors::textPrimary);
  g.setFont(juce::Font(juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withHeight(13.0f)));

  int codesPerRow = 2;
  int codeHeight = 22;
  int codeWidth = codesBounds.getWidth() / codesPerRow;

  for (int i = 0; i < backupCodes.size(); ++i) {
    int row = i / codesPerRow;
    int col = i % codesPerRow;
    auto codeBounds = juce::Rectangle<int>(codesBounds.getX() + col * codeWidth + 10,
                                           codesBounds.getY() + row * codeHeight + 8, codeWidth - 20, codeHeight);
    g.drawText(backupCodes[i], codeBounds, juce::Justification::centredLeft);
  }
}

void TwoFactorSettings::resized() {
  auto bounds = getLocalBounds();

  // Close button in top right
  closeButton->setBounds(bounds.getRight() - 45, 10, 35, 35);

  auto contentBounds = bounds.reduced(PADDING);
  contentBounds.removeFromTop(HEADER_HEIGHT + SPACING);

  // Layout based on state
  switch (currentState) {
  case State::Disabled:
    enableButton->setBounds(contentBounds.removeFromBottom(BUTTON_HEIGHT));
    enableButton->setVisible(true);
    break;

  case State::SetupType: {
    contentBounds.removeFromTop(60); // Space for title
    totpButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    hotpButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
    contentBounds.removeFromTop(SPACING * 2);
    backButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT - 10));

    totpButton->setVisible(true);
    hotpButton->setVisible(true);
    backButton->setVisible(true);
    break;
  }

  case State::SetupPassword: {
    contentBounds.removeFromTop(80); // Space for title
    passwordInput->setBounds(contentBounds.removeFromTop(INPUT_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    verifyButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    backButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT - 10));

    passwordInput->setVisible(true);
    verifyButton->setVisible(true);
    verifyButton->setButtonText("Continue");
    backButton->setVisible(true);
    break;
  }

  case State::SetupQR: {
    auto bottomArea = contentBounds.removeFromBottom(BUTTON_HEIGHT * 3 + SPACING * 2);
    copySecretButton->setBounds(bottomArea.removeFromTop(BUTTON_HEIGHT));
    bottomArea.removeFromTop(SPACING);
    copyCodesButton->setBounds(bottomArea.removeFromTop(BUTTON_HEIGHT));
    bottomArea.removeFromTop(SPACING);

    auto buttonRow = bottomArea.removeFromTop(BUTTON_HEIGHT);
    backButton->setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2 - 5));
    verifyButton->setBounds(buttonRow.removeFromRight(buttonRow.getWidth() - 5));

    copySecretButton->setVisible(true);
    copyCodesButton->setVisible(true);
    backButton->setVisible(true);
    verifyButton->setVisible(true);
    verifyButton->setButtonText("Next: Verify");
    break;
  }

  case State::SetupVerify: {
    contentBounds.removeFromTop(80);
    codeInput->setBounds(contentBounds.removeFromTop(INPUT_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    verifyButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    backButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT - 10));

    codeInput->setVisible(true);
    verifyButton->setVisible(true);
    verifyButton->setButtonText("Verify & Enable");
    backButton->setVisible(true);
    break;
  }

  case State::Enabled: {
    contentBounds.removeFromTop(100); // Space for status
    regenerateButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    disableButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));

    regenerateButton->setVisible(true);
    disableButton->setVisible(true);
    break;
  }

  case State::Disabling: {
    contentBounds.removeFromTop(80);
    codeInput->setBounds(contentBounds.removeFromTop(INPUT_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    confirmDisableButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT));
    contentBounds.removeFromTop(SPACING);
    backButton->setBounds(contentBounds.removeFromTop(BUTTON_HEIGHT - 10));

    codeInput->setVisible(true);
    confirmDisableButton->setVisible(true);
    backButton->setVisible(true);
    break;
  }

  case State::Loading:
  case State::Error:
    // Loading and Error states don't modify component bounds
    break;
  }
}

void TwoFactorSettings::buttonClicked(juce::Button *button) {
  if (button == closeButton.get()) {
    closeDialog();
  } else if (button == enableButton.get()) {
    showTypeSelection();
  } else if (button == totpButton.get()) {
    startEnable("totp");
  } else if (button == hotpButton.get()) {
    startEnable("hotp");
  } else if (button == backButton.get()) {
    statusMessage.clear();
    errorMessage.clear();
    if (currentState == State::SetupType)
      showDisabledState();
    else if (currentState == State::SetupPassword)
      showTypeSelection();
    else if (currentState == State::SetupQR)
      showPasswordEntry();
    else if (currentState == State::SetupVerify)
      showSetupQR();
    else if (currentState == State::Disabling)
      showEnabledState();
    else
      loadStatus();
  } else if (button == verifyButton.get()) {
    if (currentState == State::SetupPassword)
      submitPassword();
    else if (currentState == State::SetupQR)
      showVerifyEntry();
    else if (currentState == State::SetupVerify)
      submitVerificationCode();
  } else if (button == copySecretButton.get()) {
    copyToClipboard(setupSecret);
    statusMessage = "Secret copied to clipboard!";
    repaint();
  } else if (button == copyCodesButton.get()) {
    copyToClipboard(backupCodes.joinIntoString("\n"));
    statusMessage = "Backup codes copied to clipboard!";
    repaint();
  } else if (button == disableButton.get()) {
    startDisable();
  } else if (button == confirmDisableButton.get()) {
    submitDisableCode();
  } else if (button == regenerateButton.get()) {
    doRegenerateBackupCodes();
  }
}

void TwoFactorSettings::hideAllInputs() {
  enableButton->setVisible(false);
  totpButton->setVisible(false);
  hotpButton->setVisible(false);
  backButton->setVisible(false);
  disableButton->setVisible(false);
  regenerateButton->setVisible(false);
  copySecretButton->setVisible(false);
  copyCodesButton->setVisible(false);
  verifyButton->setVisible(false);
  confirmDisableButton->setVisible(false);
  passwordInput->setVisible(false);
  codeInput->setVisible(false);
}

void TwoFactorSettings::showDisabledState() {
  hideAllInputs();
  currentState = State::Disabled;
  enableButton->setVisible(true);
  resized();
  repaint();
}

void TwoFactorSettings::showTypeSelection() {
  hideAllInputs();
  currentState = State::SetupType;
  resized();
  repaint();
}

void TwoFactorSettings::showPasswordEntry() {
  hideAllInputs();
  currentState = State::SetupPassword;
  passwordInput->clear();
  resized();
  repaint();
}

void TwoFactorSettings::showSetupQR() {
  hideAllInputs();
  currentState = State::SetupQR;
  resized();
  repaint();
}

void TwoFactorSettings::showVerifyEntry() {
  hideAllInputs();
  currentState = State::SetupVerify;
  codeInput->clear();
  resized();
  repaint();
}

void TwoFactorSettings::showEnabledState() {
  hideAllInputs();
  currentState = State::Enabled;
  resized();
  repaint();
}

void TwoFactorSettings::showDisablingEntry() {
  hideAllInputs();
  currentState = State::Disabling;
  codeInput->clear();
  codeInput->setTextToShowWhenEmpty("Enter 2FA code or password", Colors::textSecondary);
  resized();
  repaint();
}

void TwoFactorSettings::showError(const juce::String &message) {
  hideAllInputs();
  currentState = State::Error;
  errorMessage = message;
  repaint();
}

void TwoFactorSettings::startEnable(const juce::String &type) {
  setupType = type;
  showPasswordEntry();
}

void TwoFactorSettings::submitPassword() {
  if (!networkClient)
    return;

  auto password = passwordInput->getText();
  if (password.isEmpty()) {
    errorMessage = "Please enter your password";
    repaint();
    return;
  }

  isProcessing = true;
  errorMessage.clear();
  repaint();

  networkClient->enable2FA(password, setupType, [this](auto result) {
    isProcessing = false;

    if (result.isOk()) {
      auto setup = result.getValue();
      setupSecret = setup.secret;
      setupQRUrl = setup.qrCodeUrl;
      backupCodes = setup.backupCodes;
      showSetupQR();
    } else {
      errorMessage = result.getError();
      repaint();
    }
  });
}

void TwoFactorSettings::submitVerificationCode() {
  if (!networkClient)
    return;

  auto code = codeInput->getText().removeCharacters(" -");
  if (code.isEmpty()) {
    errorMessage = "Please enter the verification code";
    repaint();
    return;
  }

  isProcessing = true;
  errorMessage.clear();
  repaint();

  networkClient->verify2FASetup(code, [this](auto result) {
    isProcessing = false;

    if (result.isOk()) {
      twoFactorEnabled = true;
      twoFactorType = setupType;
      backupCodesRemaining = static_cast<int>(backupCodes.size());
      statusMessage = "Two-factor authentication enabled!";
      showEnabledState();
    } else {
      errorMessage = result.getError();
      repaint();
    }
  });
}

void TwoFactorSettings::startDisable() {
  showDisablingEntry();
}

void TwoFactorSettings::submitDisableCode() {
  if (!networkClient)
    return;

  auto code = codeInput->getText();
  if (code.isEmpty()) {
    errorMessage = "Please enter a code or your password";
    repaint();
    return;
  }

  isProcessing = true;
  errorMessage.clear();
  repaint();

  networkClient->disable2FA(code, [this](auto result) {
    isProcessing = false;

    if (result.isOk()) {
      twoFactorEnabled = false;
      twoFactorType.clear();
      backupCodesRemaining = 0;
      statusMessage = "Two-factor authentication disabled";
      showDisabledState();
    } else {
      errorMessage = result.getError();
      repaint();
    }
  });
}

void TwoFactorSettings::doRegenerateBackupCodes() {
  // Need to get a code first - show a simple dialog
  auto *dialog = new juce::AlertWindow("Regenerate Backup Codes",
                                       "Enter your current 2FA code to generate new backup codes.\n"
                                       "This will invalidate all existing backup codes.",
                                       juce::MessageBoxIconType::QuestionIcon);

  dialog->addTextEditor("code", "", "2FA Code");
  dialog->addButton("Cancel", 0);
  dialog->addButton("Regenerate", 1);

  dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog](int result) {
                            if (result == 1) {
                              auto code = dialog->getTextEditorContents("code");
                              if (code.isNotEmpty() && networkClient) {
                                isProcessing = true;
                                repaint();

                                networkClient->regenerateBackupCodes(code, [this](auto regenResult) {
                                  isProcessing = false;

                                  if (regenResult.isOk()) {
                                    auto data = regenResult.getValue();
                                    if (data.isObject()) {
                                      auto codes = data.getProperty("backup_codes", juce::var());
                                      backupCodes.clear();
                                      if (codes.isArray()) {
                                        for (int i = 0; i < codes.size(); ++i)
                                          backupCodes.add(codes[i].toString());
                                      }
                                      backupCodesRemaining = backupCodes.size();

                                      // Show the new codes
                                      juce::String codesText = backupCodes.joinIntoString("\n");
                                      juce::AlertWindow::showMessageBoxAsync(
                                          juce::MessageBoxIconType::InfoIcon, "New Backup Codes",
                                          "Save these codes securely:\n\n" + codesText);
                                    }
                                    statusMessage = "Backup codes regenerated!";
                                  } else {
                                    errorMessage = regenResult.getError();
                                  }
                                  repaint();
                                });
                              }
                            }
                            delete dialog;
                          }),
                          true);
}

void TwoFactorSettings::copyToClipboard(const juce::String &text) {
  juce::SystemClipboard::copyTextToClipboard(text);
}
