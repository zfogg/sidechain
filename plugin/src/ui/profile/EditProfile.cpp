#include "EditProfile.h"
#include "../../network/NetworkClient.h"
#include "../../security/InputValidation.h"

#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/Validate.h"

//==============================================================================
EditProfile::EditProfile(Sidechain::Stores::AppStore *store) : AppStoreComponent(store) {
  Log::info("EditProfile: Initializing");
  setupEditors();
  // Set size last to avoid resized() being called before components are created
  setSize(500, 1050); // Height for profile editing + settings section
  initialize();
}

EditProfile::~EditProfile() {
  Log::debug("EditProfile: Destroying");
}

//==============================================================================
void EditProfile::setupEditors() {
  auto styleEditor = [this](juce::TextEditor &editor, const juce::String &placeholder, bool multiLine = false) {
    editor.setMultiLine(multiLine, true);
    editor.setReturnKeyStartsNewLine(multiLine);
    editor.setScrollbarsShown(multiLine);
    editor.setCaretVisible(true);
    editor.setPopupMenuEnabled(true);
    editor.setTextToShowWhenEmpty(placeholder, Colors::textPlaceholder);
    editor.setColour(juce::TextEditor::backgroundColourId, Colors::inputBg);
    editor.setColour(juce::TextEditor::outlineColourId, Colors::inputBorder);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, Colors::inputBorderFocused);
    editor.setColour(juce::TextEditor::textColourId, Colors::textPrimary);
    editor.setColour(juce::CaretComponent::caretColourId, Colors::accent);
    editor.setFont(juce::FontOptions(14.0f));
    editor.setIndents(12, multiLine ? 8 : 0);
    editor.addListener(this);
  };

  // Username editor (special case - needs validation)
  usernameEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*usernameEditor, "username");
  usernameEditor->setInputRestrictions(30, "abcdefghijklmnopqrstuvwxyz0123456789_");
  addAndMakeVisible(usernameEditor.get());

  // Basic info editors
  displayNameEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*displayNameEditor, "Display Name");
  addAndMakeVisible(displayNameEditor.get());

  bioEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*bioEditor, "Tell us about yourself...", true);
  addAndMakeVisible(bioEditor.get());

  locationEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*locationEditor, "City, Country");
  addAndMakeVisible(locationEditor.get());

  genreEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*genreEditor, "e.g., Electronic, Hip-Hop, House");
  addAndMakeVisible(genreEditor.get());

  dawEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*dawEditor, "e.g., Ableton Live, FL Studio");
  addAndMakeVisible(dawEditor.get());

  // Social link editors
  instagramEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*instagramEditor, "Instagram username");
  addAndMakeVisible(instagramEditor.get());

  soundcloudEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*soundcloudEditor, "SoundCloud URL");
  addAndMakeVisible(soundcloudEditor.get());

  spotifyEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*spotifyEditor, "Spotify artist URL");
  addAndMakeVisible(spotifyEditor.get());

  twitterEditor = std::make_unique<juce::TextEditor>();
  styleEditor(*twitterEditor, "Twitter/X username");
  addAndMakeVisible(twitterEditor.get());

  // Buttons
  cancelButton = std::make_unique<juce::TextButton>("Cancel");
  cancelButton->setColour(juce::TextButton::buttonColourId, Colors::cancelButton);
  cancelButton->setColour(juce::TextButton::textColourOffId, Colors::textSecondary);
  cancelButton->addListener(this);
  addAndMakeVisible(cancelButton.get());

  saveButton = std::make_unique<juce::TextButton>("Save");
  saveButton->setColour(juce::TextButton::buttonColourId, Colors::saveButtonDisabled);
  saveButton->setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
  saveButton->setEnabled(false);
  saveButton->addListener(this);
  addAndMakeVisible(saveButton.get());

  logoutButton = std::make_unique<juce::TextButton>("Logout");
  logoutButton->setColour(juce::TextButton::buttonColourId, Colors::errorRed);
  logoutButton->setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
  logoutButton->addListener(this);
  addAndMakeVisible(logoutButton.get());

  changePhotoButton = std::make_unique<juce::TextButton>("Change Photo");
  changePhotoButton->setColour(juce::TextButton::buttonColourId, Colors::accent.withAlpha(0.2f));
  changePhotoButton->setColour(juce::TextButton::textColourOffId, Colors::accent);
  changePhotoButton->addListener(this);
  addAndMakeVisible(changePhotoButton.get());

  // Private account toggle
  privateAccountToggle = std::make_unique<juce::ToggleButton>("Private Account");
  privateAccountToggle->setColour(juce::ToggleButton::textColourId, Colors::textPrimary);
  privateAccountToggle->setColour(juce::ToggleButton::tickColourId, Colors::accent);
  privateAccountToggle->setColour(juce::ToggleButton::tickDisabledColourId, Colors::textSecondary);
  privateAccountToggle->onClick = [this]() { updateHasChanges(); };
  addAndMakeVisible(privateAccountToggle.get());

  // Settings section buttons
  activityStatusButton = std::make_unique<juce::TextButton>("Activity Status");
  activityStatusButton->setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  activityStatusButton->setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
  activityStatusButton->addListener(this);
  addAndMakeVisible(activityStatusButton.get());

  mutedUsersButton = std::make_unique<juce::TextButton>("Muted Users");
  mutedUsersButton->setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  mutedUsersButton->setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
  mutedUsersButton->addListener(this);
  addAndMakeVisible(mutedUsersButton.get());

  twoFactorButton = std::make_unique<juce::TextButton>("Two-Factor Authentication");
  twoFactorButton->setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  twoFactorButton->setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
  twoFactorButton->addListener(this);
  addAndMakeVisible(twoFactorButton.get());

  profileSetupButton = std::make_unique<juce::TextButton>("Edit Username & Avatar");
  profileSetupButton->setColour(juce::TextButton::buttonColourId, Colors::inputBg);
  profileSetupButton->setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
  profileSetupButton->addListener(this);
  addAndMakeVisible(profileSetupButton.get());
}

// Show modal with current profile from UserStore
void EditProfile::showWithCurrentProfile(juce::Component *parentComponent) {
  if (!appStore) {
    Log::error("EditProfile: Cannot show modal - AppStore not set!");
    return;
  }

  // Reset local form state
  pendingAvatarPath = "";
  avatarImage = juce::Image();
  hasUnsavedChanges = false;

  // Populate from UserStore
  populateFromUserStore();

  // Show the modal
  showModal(parentComponent);
}

// Populate form from UserStore (not local state)
void EditProfile::populateFromUserStore() {
  if (!appStore) {
    Log::error("EditProfile: Cannot populate - AppStore not set!");
    return;
  }

  const auto &state = appStore->getState().user;

  // Populate basic fields from UserStore
  usernameEditor->setText(state.username, false);
  displayNameEditor->setText(state.displayName, false);
  bioEditor->setText(state.bio, false);
  locationEditor->setText(state.location, false);
  genreEditor->setText(state.genre, false);
  dawEditor->setText(state.dawPreference, false);
  privateAccountToggle->setToggleState(state.isPrivate, juce::dontSendNotification);

  // Store original username for change detection
  originalUsername = state.username;

  // Reset username validation state
  isUsernameValid = true;
  usernameError = "";

  // Parse social links from UserStore
  if (state.socialLinks.isObject()) {
    auto *obj = state.socialLinks.getDynamicObject();
    if (obj != nullptr) {
      instagramEditor->setText(obj->getProperty("instagram").toString(), false);
      soundcloudEditor->setText(obj->getProperty("soundcloud").toString(), false);
      spotifyEditor->setText(obj->getProperty("spotify").toString(), false);
      twitterEditor->setText(obj->getProperty("twitter").toString(), false);
    }
  }

  // Load avatar from UserStore
  if (state.profileImage.isValid()) {
    avatarImage = state.profileImage;
  }

  updateHasChanges();
  repaint();
}

// Task 2.4: Build social links JSON from editors
juce::var EditProfile::getSocialLinksFromEditors() const {
  using namespace Sidechain::Security;

  auto *linksObj = new juce::DynamicObject();

  // Validate and sanitize social links
  auto instagramRule = InputValidator::string();
  instagramRule->maxLength(100);

  auto soundcloudRule = InputValidator::string();
  soundcloudRule->maxLength(200);

  auto spotifyRule = InputValidator::string();
  spotifyRule->maxLength(200);

  auto twitterRule = InputValidator::string();
  twitterRule->maxLength(100);

  auto linkValidator = InputValidator::create()
                           ->addRule("instagram", instagramRule)
                           ->addRule("soundcloud", soundcloudRule)
                           ->addRule("spotify", spotifyRule)
                           ->addRule("twitter", twitterRule);

  juce::StringPairArray socialData;
  socialData.set("instagram", instagramEditor->getText().trim());
  socialData.set("soundcloud", soundcloudEditor->getText().trim());
  socialData.set("spotify", spotifyEditor->getText().trim());
  socialData.set("twitter", twitterEditor->getText().trim());

  auto socialResult = linkValidator->validate(socialData);

  // Use sanitized social links (XSS protection)
  juce::String instagram = socialResult.getValue("instagram").value_or("");
  juce::String soundcloud = socialResult.getValue("soundcloud").value_or("");
  juce::String spotify = socialResult.getValue("spotify").value_or("");
  juce::String twitter = socialResult.getValue("twitter").value_or("");

  if (instagram.isNotEmpty())
    linksObj->setProperty("instagram", instagram);
  if (soundcloud.isNotEmpty())
    linksObj->setProperty("soundcloud", soundcloud);
  if (spotify.isNotEmpty())
    linksObj->setProperty("spotify", spotify);
  if (twitter.isNotEmpty())
    linksObj->setProperty("twitter", twitter);

  return juce::var(linksObj);
}

// Compare editors to UserStore to detect changes
void EditProfile::updateHasChanges() {
  if (!appStore) {
    hasUnsavedChanges = false;
    saveButton->setEnabled(false);
    return;
  }

  const auto &state = appStore->getState().user;

  // Get current editor values
  juce::String currentUsername = usernameEditor->getText().trim().toLowerCase();
  juce::String currentDisplayName = displayNameEditor->getText().trim();
  juce::String currentBio = bioEditor->getText().trim();
  juce::String currentLocation = locationEditor->getText().trim();
  juce::String currentGenre = genreEditor->getText().trim();
  juce::String currentDaw = dawEditor->getText().trim();
  bool currentPrivate = privateAccountToggle->getToggleState();
  juce::var currentSocialLinks = getSocialLinksFromEditors();

  // Check if username changed
  bool usernameChanged = currentUsername != originalUsername;

  // Compare all fields to UserStore state
  hasUnsavedChanges =
      (usernameChanged || currentDisplayName != state.displayName || currentBio != state.bio ||
       currentLocation != state.location || currentGenre != state.genre || currentDaw != state.dawPreference ||
       currentPrivate != state.isPrivate || pendingAvatarPath.isNotEmpty() ||
       juce::JSON::toString(currentSocialLinks) != juce::JSON::toString(state.socialLinks));

  // Can only save if there are changes AND username is valid (if changed) AND
  // not currently saving
  bool isSaving = false;
  bool canSave = hasUnsavedChanges && !isSaving && (!usernameChanged || isUsernameValid);

  saveButton->setEnabled(canSave);
  saveButton->setColour(juce::TextButton::buttonColourId, canSave ? Colors::saveButton : Colors::saveButtonDisabled);

  // Update username editor border color based on validation
  if (usernameChanged && !isUsernameValid) {
    usernameEditor->setColour(juce::TextEditor::outlineColourId, Colors::errorRed);
    usernameEditor->setColour(juce::TextEditor::focusedOutlineColourId, Colors::errorRed);
  } else {
    usernameEditor->setColour(juce::TextEditor::outlineColourId, Colors::inputBorder);
    usernameEditor->setColour(juce::TextEditor::focusedOutlineColourId, Colors::inputBorderFocused);
  }
}

//==============================================================================
void EditProfile::paint(juce::Graphics &g) {
  // Background
  g.fillAll(Colors::background);

  // Header
  auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
  drawHeader(g, headerBounds);

  // Avatar area
  drawAvatar(g, getAvatarBounds());

  // Form sections
  int y = HEADER_HEIGHT + AVATAR_SIZE + 70;

  // Username section
  drawFormSection(g, "Username", juce::Rectangle<int>(PADDING, y - 25, getWidth() - PADDING * 2, 20));

  // Draw @ prefix for username
  g.setColour(Colors::textSecondary);
  g.setFont(14.0f);
  g.drawText("@", PADDING + 4, y + 10, 15, 20, juce::Justification::centred);

  // Draw username error if any
  if (!isUsernameValid && usernameError.isNotEmpty()) {
    g.setColour(Colors::errorRed);
    g.setFont(11.0f);
    g.drawText(usernameError, PADDING, y + FIELD_HEIGHT + 2, getWidth() - PADDING * 2, 15,
               juce::Justification::centredLeft);
  }

  // Basic Info section
  int basicInfoY = y + FIELD_HEIGHT + FIELD_SPACING + 20;
  drawFormSection(g, "Basic Info", juce::Rectangle<int>(PADDING, basicInfoY - 25, getWidth() - PADDING * 2, 20));

  // Social Links section
  int socialY = basicInfoY + (FIELD_HEIGHT + FIELD_SPACING) * 5 + SECTION_SPACING;
  drawFormSection(g, "Social Links", juce::Rectangle<int>(PADDING, socialY - 25, getWidth() - PADDING * 2, 20));

  // Privacy section
  int privacyY = socialY + (FIELD_HEIGHT + FIELD_SPACING) * 4 + SECTION_SPACING;
  drawFormSection(g, "Privacy", juce::Rectangle<int>(PADDING, privacyY - 25, getWidth() - PADDING * 2, 20));

  // Draw privacy description
  g.setColour(Colors::textSecondary);
  g.setFont(11.0f);
  g.drawText("When enabled, only approved followers can see your posts.", PADDING, privacyY + FIELD_HEIGHT + 5,
             getWidth() - PADDING * 2, 15, juce::Justification::centredLeft);

  // Settings section
  int settingsY = privacyY + FIELD_HEIGHT + SECTION_SPACING + 25;
  drawFormSection(g, "Settings", juce::Rectangle<int>(PADDING, settingsY - 25, getWidth() - PADDING * 2, 20));
}

void EditProfile::drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds) {
  g.setColour(Colors::headerBg);
  g.fillRect(bounds);

  // Title
  g.setColour(Colors::textPrimary);
  g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
  g.drawText("Edit Profile", bounds, juce::Justification::centred);

  // Bottom border
  g.setColour(Colors::inputBorder);
  g.drawLine(0, static_cast<float>(bounds.getBottom()), static_cast<float>(getWidth()),
             static_cast<float>(bounds.getBottom()), 1.0f);
}

void EditProfile::drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds) {
  // Circular clip
  juce::Path circlePath;
  circlePath.addEllipse(bounds.toFloat());

  g.saveState();
  g.reduceClipRegion(circlePath);

  if (avatarImage.isValid()) {
    auto scaledImage =
        avatarImage.rescaled(bounds.getWidth(), bounds.getHeight(), juce::Graphics::highResamplingQuality);
    g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
  } else {
    // Placeholder gradient
    g.setGradientFill(juce::ColourGradient(Colors::accent.darker(0.3f), static_cast<float>(bounds.getX()),
                                           static_cast<float>(bounds.getY()), Colors::accent.darker(0.6f),
                                           static_cast<float>(bounds.getRight()),
                                           static_cast<float>(bounds.getBottom()), true));
    g.fillEllipse(bounds.toFloat());

    // Task 2.4: Display initial from UserStore
    g.setColour(Colors::textPrimary);
    g.setFont(juce::FontOptions(32.0f).withStyle("Bold"));
    juce::String initial = "?";
    if (appStore) {
      const auto &state = appStore->getState().user;
      initial = state.displayName.isEmpty()
                    ? (state.username.isEmpty() ? "?" : state.username.substring(0, 1).toUpperCase())
                    : state.displayName.substring(0, 1).toUpperCase();
    }
    g.drawText(initial, bounds, juce::Justification::centred);
  }

  g.restoreState();

  // Border
  g.setColour(Colors::accent.withAlpha(0.5f));
  g.drawEllipse(bounds.toFloat(), 2.0f);
}

void EditProfile::drawFormSection(juce::Graphics &g, const juce::String &title, juce::Rectangle<int> bounds) {
  g.setColour(Colors::textSecondary);
  g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
  g.drawText(title.toUpperCase(), bounds, juce::Justification::centredLeft);
}

//==============================================================================
void EditProfile::resized() {
  // Header buttons
  cancelButton->setBounds(PADDING, 15, 70, 30);
  logoutButton->setBounds(getWidth() - PADDING - 150, 15, 70, 30);
  saveButton->setBounds(getWidth() - PADDING - 70, 15, 70, 30);

  // Avatar and change photo button
  auto avatarBounds = getAvatarBounds();
  changePhotoButton->setBounds(avatarBounds.getCentreX() - 60, avatarBounds.getBottom() + 10, 120, 28);

  // Form fields
  int y = HEADER_HEIGHT + AVATAR_SIZE + 70;
  int fieldWidth = getWidth() - PADDING * 2;

  // Username field (with space for @ prefix)
  usernameEditor->setBounds(PADDING + 20, y, fieldWidth - 20, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING + 20; // Extra space for error message

  displayNameEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  bioEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT * 2);
  y += FIELD_HEIGHT * 2 + FIELD_SPACING;

  locationEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  genreEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  dawEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + SECTION_SPACING + 25;

  // Social links
  instagramEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  soundcloudEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  spotifyEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  twitterEditor->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + SECTION_SPACING + 25;

  // Privacy section
  privateAccountToggle->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + SECTION_SPACING + 25;

  // Settings section
  activityStatusButton->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  mutedUsersButton->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  twoFactorButton->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
  y += FIELD_HEIGHT + FIELD_SPACING;

  profileSetupButton->setBounds(PADDING, y, fieldWidth, FIELD_HEIGHT);
}

juce::Rectangle<int> EditProfile::getAvatarBounds() const {
  return juce::Rectangle<int>((getWidth() - AVATAR_SIZE) / 2, HEADER_HEIGHT + 15, AVATAR_SIZE, AVATAR_SIZE);
}

//==============================================================================
void EditProfile::buttonClicked(juce::Button *button) {
  if (button == cancelButton.get()) {
    // Task 2.4: Close dialog directly without callback
    closeDialog();
  } else if (button == saveButton.get()) {
    handleSave();
  } else if (button == changePhotoButton.get()) {
    handlePhotoSelect();
  } else if (button == activityStatusButton.get()) {
    if (onActivityStatusClicked)
      onActivityStatusClicked();
  } else if (button == mutedUsersButton.get()) {
    if (onMutedUsersClicked)
      onMutedUsersClicked();
  } else if (button == twoFactorButton.get()) {
    if (onTwoFactorClicked)
      onTwoFactorClicked();
  } else if (button == profileSetupButton.get()) {
    if (onProfileSetupClicked)
      onProfileSetupClicked();
  } else if (button == logoutButton.get()) {
    Log::info("EditProfile: Logout button clicked");
    if (onLogoutClicked)
      onLogoutClicked();
  }
}

void EditProfile::textEditorTextChanged(juce::TextEditor &editor) {
  // Validate username when it changes
  if (&editor == usernameEditor.get()) {
    validateUsername(usernameEditor->getText().trim().toLowerCase());
  }

  updateHasChanges();
}

//==============================================================================
// Save editor values to UserStore
void EditProfile::handleSave() {
  if (!appStore || !hasUnsavedChanges) {
    Log::warn("EditProfile: Cannot save - AppStore not set or no changes");
    return;
  }

  Log::info("EditProfile: Saving profile changes to UserStore");

  // Get values from editors
  juce::String newUsername = usernameEditor->getText().trim().toLowerCase();
  juce::String newDisplayName = displayNameEditor->getText().trim();
  juce::String newBio = bioEditor->getText().trim();
  juce::String newLocation = locationEditor->getText().trim();
  juce::String newGenre = genreEditor->getText().trim();
  juce::String newDaw = dawEditor->getText().trim();
  bool newPrivate = privateAccountToggle->getToggleState();
  juce::var newSocialLinks = getSocialLinksFromEditors();

  bool usernameChanged = newUsername != originalUsername;

  // If username changed, update it first
  if (usernameChanged && isUsernameValid) {
    Log::info("EditProfile: Username changed to: " + newUsername);
    Sidechain::Stores::AppStore::getInstance().changeUsername(newUsername);
    originalUsername = newUsername; // Update for next comparison
  }

  // Update profile data (all fields except username)
  juce::String avatarUrl =
      pendingAvatarPath.isNotEmpty() ? pendingAvatarPath : appStore->getState().user.profilePictureUrl;
  appStore->updateProfileComplete(newDisplayName, newBio, newLocation, newGenre, newDaw, newSocialLinks, newPrivate,
                                  avatarUrl);

  // Reset form state
  hasUnsavedChanges = false;
  pendingAvatarPath = "";
  updateHasChanges();

  // Close dialog after short delay (allow UserStore to process)
  juce::MessageManager::callAsync([this]() {
    Log::info("EditProfile: Profile saved successfully");
    closeDialog();
  });
}

void EditProfile::validateUsername(const juce::String &username) {
  // Compare to originalUsername (from UserStore)
  if (username == originalUsername) {
    isUsernameValid = true;
    usernameError = "";
    return;
  }

  // Use centralized validation
  if (!Validate::isUsername(username)) {
    isUsernameValid = false;
    // Provide specific error messages based on what's wrong
    if (username.length() < 3)
      usernameError = "Username must be at least 3 characters";
    else if (username.length() > 30)
      usernameError = "Username must be 30 characters or less";
    else
      usernameError = "Username must start with a letter and contain only "
                      "letters, numbers, and underscores";
    return;
  }

  // Validation passed
  isUsernameValid = true;
  usernameError = "";
}

void EditProfile::handlePhotoSelect() {
  auto chooser = std::make_shared<juce::FileChooser>("Select Profile Picture",
                                                     juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
                                                     "*.jpg;*.jpeg;*.png;*.gif");

  chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                       [this, chooser](const juce::FileChooser &fc) {
                         auto results = fc.getResults();
                         if (results.isEmpty())
                           return;

                         juce::File selectedFile = results[0];
                         if (!selectedFile.existsAsFile())
                           return;

                         // Load the image for preview
                         avatarImage = juce::ImageFileFormat::loadFrom(selectedFile);
                         pendingAvatarPath = selectedFile.getFullPathName();
                         updateHasChanges();
                         repaint();

                         // Upload via AppStore
                         if (appStore != nullptr) {
                           Log::debug("EditProfile: Uploading profile picture via AppStore");
                           appStore->uploadProfilePicture(selectedFile);
                         } else {
                           Log::warn("EditProfile: AppStore not set for profile picture upload");
                         }
                       });
}

//==============================================================================
void EditProfile::showModal(juce::Component *parentComponent) {
  if (parentComponent == nullptr)
    return;

  // Size to fill parent
  setBounds(parentComponent->getLocalBounds());
  parentComponent->addAndMakeVisible(this);
  toFront(true);
}

void EditProfile::closeDialog() {
  juce::MessageManager::callAsync([this]() {
    setVisible(false);
    if (auto *parent = getParentComponent())
      parent->removeChildComponent(this);
  });
}

//==============================================================================
// AppStoreComponent overrides
//==============================================================================
void EditProfile::onAppStateChanged(const Sidechain::Stores::UserState &state) {
  // Update form if there are saved changes (e.g., username change completed)
  if (!hasUnsavedChanges && isVisible()) {
    // Refresh UI from updated UserStore state
    populateFromUserStore();
  }
}

void EditProfile::subscribeToAppStore() {
  juce::Component::SafePointer<EditProfile> safeThis(this);
  storeUnsubscriber = appStore->subscribeToUser([safeThis](const Sidechain::Stores::UserState &state) {
    if (!safeThis)
      return;
    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });
}
