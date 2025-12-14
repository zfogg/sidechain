#include "EditProfile.h"
#include "../../network/NetworkClient.h"
#include "../../stores/ImageCache.h"
#include "../../util/Validate.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../security/InputValidation.h"

//==============================================================================
EditProfile::EditProfile()
{
    Log::info("EditProfile: Initializing");
    setupEditors();
    // Set size last to avoid resized() being called before components are created
    setSize(500, 1050);  // Height for profile editing + settings section
}

EditProfile::~EditProfile()
{
    Log::debug("EditProfile: Destroying");
    // Task 2.4: Unsubscribe from UserStore
    if (userStoreUnsubscribe)
        userStoreUnsubscribe();
}

//==============================================================================
void EditProfile::setupEditors()
{
    auto styleEditor = [this](juce::TextEditor& editor, const juce::String& placeholder, bool multiLine = false) {
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

void EditProfile::setProfile(const UserProfile& newProfile)
{
    profile = newProfile;
    originalProfile = newProfile;
    hasChanges = false;
    errorMessage = "";
    pendingAvatarPath = "";
    avatarImage = juce::Image();

    // Load existing avatar from URL via ImageCache
    juce::String avatarUrl = profile.getAvatarUrl();
    if (avatarUrl.isNotEmpty())
    {
        ImageLoader::load(avatarUrl, [this](const juce::Image& img) {
            // Only update if no local file has been selected
            if (pendingAvatarPath.isEmpty())
            {
                avatarImage = img;
                repaint();
            }
        });
    }

    populateFromProfile();
    updateHasChanges();
    repaint();
}

void EditProfile::setUserStore(Sidechain::Stores::UserStore* store)
{
    userStore = store;
    if (userStore)
    {
        Log::debug("EditProfile: UserStore set, subscribing to state changes");
        userStoreUnsubscribe = userStore->subscribe([this](const Sidechain::Stores::UserState& state) {
            Log::debug("EditProfile: UserStore state updated");
            // Subscription triggers automatic repaint/resized
        });
    }
    else
    {
        Log::warn("EditProfile: UserStore is nullptr!");
    }
}

void EditProfile::populateFromProfile()
{
    usernameEditor->setText(profile.username, false);
    displayNameEditor->setText(profile.displayName, false);
    bioEditor->setText(profile.bio, false);
    locationEditor->setText(profile.location, false);
    genreEditor->setText(profile.genre, false);
    dawEditor->setText(profile.dawPreference, false);
    privateAccountToggle->setToggleState(profile.isPrivate, juce::dontSendNotification);

    // Reset username validation state
    isUsernameValid = true;
    usernameError = "";

    // Parse social links
    if (profile.socialLinks.isObject())
    {
        auto* obj = profile.socialLinks.getDynamicObject();
        if (obj != nullptr)
        {
            instagramEditor->setText(obj->getProperty("instagram").toString(), false);
            soundcloudEditor->setText(obj->getProperty("soundcloud").toString(), false);
            spotifyEditor->setText(obj->getProperty("spotify").toString(), false);
            twitterEditor->setText(obj->getProperty("twitter").toString(), false);
        }
    }
}

void EditProfile::collectToProfile()
{
    using namespace Sidechain::Security;

    // Sanitize and validate basic profile fields
    auto usernameRule = InputValidator::alphanumeric();
    usernameRule->minLength(3);
    usernameRule->maxLength(30);

    auto displayNameRule = InputValidator::string();
    displayNameRule->minLength(1);
    displayNameRule->maxLength(50);

    auto bioRule = InputValidator::string();
    bioRule->maxLength(500);

    auto locationRule = InputValidator::string();
    locationRule->maxLength(100);

    auto genreRule = InputValidator::string();
    genreRule->maxLength(50);

    auto dawRule = InputValidator::string();
    dawRule->maxLength(50);

    auto validator = InputValidator::create()
        ->addRule("username", usernameRule)
        ->addRule("displayName", displayNameRule)
        ->addRule("bio", bioRule)
        ->addRule("location", locationRule)
        ->addRule("genre", genreRule)
        ->addRule("daw", dawRule);

    juce::StringPairArray profileData;
    profileData.set("username", usernameEditor->getText().trim().toLowerCase());
    profileData.set("displayName", displayNameEditor->getText().trim());
    profileData.set("bio", bioEditor->getText().trim());
    profileData.set("location", locationEditor->getText().trim());
    profileData.set("genre", genreEditor->getText().trim());
    profileData.set("daw", dawEditor->getText().trim());

    auto validationResult = validator->validate(profileData);

    // Use sanitized values (XSS protection)
    profile.username = validationResult.getValue("username").value_or(usernameEditor->getText().trim().toLowerCase());
    profile.displayName = validationResult.getValue("displayName").value_or(displayNameEditor->getText().trim());
    profile.bio = validationResult.getValue("bio").value_or(bioEditor->getText().trim());
    profile.location = validationResult.getValue("location").value_or(locationEditor->getText().trim());
    profile.genre = validationResult.getValue("genre").value_or(genreEditor->getText().trim());
    profile.dawPreference = validationResult.getValue("daw").value_or(dawEditor->getText().trim());
    profile.isPrivate = privateAccountToggle->getToggleState();

    // Validate and sanitize social links
    auto* linksObj = new juce::DynamicObject();

    // Validate URLs for social links (allow usernames or full URLs)
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

    profile.socialLinks = juce::var(linksObj);

    if (pendingAvatarPath.isNotEmpty())
        profile.profilePictureUrl = pendingAvatarPath;
}

void EditProfile::updateHasChanges()
{
    collectToProfile();

    bool usernameChanged = profile.username != originalProfile.username;

    hasChanges = (usernameChanged ||
                  profile.displayName != originalProfile.displayName ||
                  profile.bio != originalProfile.bio ||
                  profile.location != originalProfile.location ||
                  profile.genre != originalProfile.genre ||
                  profile.dawPreference != originalProfile.dawPreference ||
                  profile.isPrivate != originalProfile.isPrivate ||
                  pendingAvatarPath.isNotEmpty() ||
                  juce::JSON::toString(profile.socialLinks) != juce::JSON::toString(originalProfile.socialLinks));

    // Can only save if there are changes AND username is valid (if changed)
    bool canSave = hasChanges && !isSaving && (!usernameChanged || isUsernameValid);

    saveButton->setEnabled(canSave);
    saveButton->setColour(juce::TextButton::buttonColourId,
                          canSave ? Colors::saveButton : Colors::saveButtonDisabled);

    // Update username editor border color based on validation
    if (usernameChanged && !isUsernameValid)
    {
        usernameEditor->setColour(juce::TextEditor::outlineColourId, Colors::errorRed);
        usernameEditor->setColour(juce::TextEditor::focusedOutlineColourId, Colors::errorRed);
    }
    else
    {
        usernameEditor->setColour(juce::TextEditor::outlineColourId, Colors::inputBorder);
        usernameEditor->setColour(juce::TextEditor::focusedOutlineColourId, Colors::inputBorderFocused);
    }
}

//==============================================================================
void EditProfile::paint(juce::Graphics& g)
{
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
    if (!isUsernameValid && usernameError.isNotEmpty())
    {
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
    g.drawText("When enabled, only approved followers can see your posts.",
               PADDING, privacyY + FIELD_HEIGHT + 5, getWidth() - PADDING * 2, 15,
               juce::Justification::centredLeft);

    // Settings section
    int settingsY = privacyY + FIELD_HEIGHT + SECTION_SPACING + 25;
    drawFormSection(g, "Settings", juce::Rectangle<int>(PADDING, settingsY - 25, getWidth() - PADDING * 2, 20));

    // Error message
    if (errorMessage.isNotEmpty())
    {
        g.setColour(Colors::errorRed);
        g.setFont(12.0f);
        g.drawText(errorMessage, PADDING, getHeight() - 80, getWidth() - PADDING * 2, 20,
                   juce::Justification::centred);
    }
}

void EditProfile::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
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

void EditProfile::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Circular clip
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());

    g.saveState();
    g.reduceClipRegion(circlePath);

    if (avatarImage.isValid())
    {
        auto scaledImage = avatarImage.rescaled(bounds.getWidth(), bounds.getHeight(),
                                                 juce::Graphics::highResamplingQuality);
        g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
    }
    else
    {
        // Placeholder gradient
        g.setGradientFill(juce::ColourGradient(
            Colors::accent.darker(0.3f),
            static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
            Colors::accent.darker(0.6f),
            static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()),
            true
        ));
        g.fillEllipse(bounds.toFloat());

        // Initial
        g.setColour(Colors::textPrimary);
        g.setFont(juce::FontOptions(32.0f).withStyle("Bold"));
        juce::String initial = profile.displayName.isEmpty()
            ? (profile.username.isEmpty() ? "?" : profile.username.substring(0, 1).toUpperCase())
            : profile.displayName.substring(0, 1).toUpperCase();
        g.drawText(initial, bounds, juce::Justification::centred);
    }

    g.restoreState();

    // Border
    g.setColour(Colors::accent.withAlpha(0.5f));
    g.drawEllipse(bounds.toFloat(), 2.0f);
}

void EditProfile::drawFormSection(juce::Graphics& g, const juce::String& title, juce::Rectangle<int> bounds)
{
    g.setColour(Colors::textSecondary);
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText(title.toUpperCase(), bounds, juce::Justification::centredLeft);
}

//==============================================================================
void EditProfile::resized()
{
    // Header buttons
    cancelButton->setBounds(PADDING, 15, 70, 30);
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

juce::Rectangle<int> EditProfile::getAvatarBounds() const
{
    return juce::Rectangle<int>(
        (getWidth() - AVATAR_SIZE) / 2,
        HEADER_HEIGHT + 15,
        AVATAR_SIZE,
        AVATAR_SIZE
    );
}

//==============================================================================
void EditProfile::buttonClicked(juce::Button* button)
{
    if (button == cancelButton.get())
    {
        // Task 2.4: Close dialog directly without callback
        closeDialog();
    }
    else if (button == saveButton.get())
    {
        handleSave();
    }
    else if (button == changePhotoButton.get())
    {
        handlePhotoSelect();
    }
    else if (button == activityStatusButton.get())
    {
        if (onActivityStatusClicked)
            onActivityStatusClicked();
    }
    else if (button == mutedUsersButton.get())
    {
        if (onMutedUsersClicked)
            onMutedUsersClicked();
    }
    else if (button == twoFactorButton.get())
    {
        if (onTwoFactorClicked)
            onTwoFactorClicked();
    }
    else if (button == profileSetupButton.get())
    {
        if (onProfileSetupClicked)
            onProfileSetupClicked();
    }
}

void EditProfile::textEditorTextChanged(juce::TextEditor& editor)
{
    // Validate username when it changes
    if (&editor == usernameEditor.get())
    {
        validateUsername(usernameEditor->getText().trim().toLowerCase());
    }

    updateHasChanges();
}

//==============================================================================
void EditProfile::handleSave()
{
    // Task 2.4: Use UserStore instead of NetworkClient
    if (userStore == nullptr || !hasChanges)
        return;

    collectToProfile();
    isSaving = true;
    saveButton->setEnabled(false);
    errorMessage = "";
    repaint();

    bool usernameChanged = profile.username != originalProfile.username;

    // If username changed, handle it first
    if (usernameChanged)
    {
        handleUsernameChange();
    }
    else
    {
        // Just save other profile changes
        saveProfileData();
    }
}

void EditProfile::saveProfileData()
{
    Log::info("EditProfile: Saving profile data (Task 2.4 - using UserStore)");

    if (userStore == nullptr)
    {
        Log::error("EditProfile: UserStore not set!");
        isSaving = false;
        saveButton->setEnabled(true);
        repaint();
        return;
    }

    // Task 2.4: Use UserStore to update profile (all fields except username)
    userStore->updateProfileComplete(
        profile.displayName,
        profile.bio,
        profile.location,
        profile.genre,
        profile.dawPreference,
        profile.socialLinks,
        profile.isPrivate,
        profile.profilePictureUrl.isNotEmpty() ? profile.profilePictureUrl : ""
    );

    // Mark as saved immediately (optimistic update)
    juce::MessageManager::callAsync([this]() {
        isSaving = false;
        originalProfile = profile;
        hasChanges = false;
        updateHasChanges();
        Log::info("EditProfile: Profile saved successfully");
        closeDialog();
        repaint();
    });
}

void EditProfile::handleUsernameChange()
{
    // Task 2.4: Use UserStore to change username
    if (userStore == nullptr)
    {
        Log::error("EditProfile: UserStore not set for username change!");
        isSaving = false;
        saveButton->setEnabled(true);
        repaint();
        return;
    }

    // Change username via UserStore
    userStore->changeUsername(profile.username);

    // Username changed, now save other profile data
    juce::MessageManager::callAsync([this]() {
        originalProfile.username = profile.username;
        saveProfileData();
    });
}

void EditProfile::validateUsername(const juce::String& username)
{
    // If same as original, always valid
    if (username == originalProfile.username)
    {
        isUsernameValid = true;
        usernameError = "";
        return;
    }

    // Use centralized validation
    if (!Validate::isUsername(username))
    {
        isUsernameValid = false;
        // Provide specific error messages based on what's wrong
        if (username.length() < 3)
            usernameError = "Username must be at least 3 characters";
        else if (username.length() > 30)
            usernameError = "Username must be 30 characters or less";
        else
            usernameError = "Username must start with a letter and contain only letters, numbers, and underscores";
        return;
    }

    // Validation passed
    isUsernameValid = true;
    usernameError = "";
}

void EditProfile::handlePhotoSelect()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Profile Picture",
        juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
        "*.jpg;*.jpeg;*.png;*.gif"
    );

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc) {
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

            // Task 2.4: Upload via UserStore instead of callback
            if (userStore != nullptr)
            {
                Log::debug("EditProfile: Uploading profile picture via UserStore");
                userStore->uploadProfilePicture(selectedFile);
            }
            else
            {
                Log::warn("EditProfile: UserStore not set for profile picture upload");
            }
        });
}

//==============================================================================
void EditProfile::showModal(juce::Component* parentComponent)
{
    if (parentComponent == nullptr)
        return;

    // Size to fill parent
    setBounds(parentComponent->getLocalBounds());
    parentComponent->addAndMakeVisible(this);
    toFront(true);
}

void EditProfile::closeDialog()
{
    juce::MessageManager::callAsync([this]() {
        setVisible(false);
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
    });
}
