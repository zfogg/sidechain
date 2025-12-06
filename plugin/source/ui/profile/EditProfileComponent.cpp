#include "EditProfileComponent.h"
#include "../../network/NetworkClient.h"
#include "../../util/ImageCache.h"
#include "../../util/Validate.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

//==============================================================================
EditProfileComponent::EditProfileComponent()
{
    Log::info("EditProfileComponent: Initializing");
    setSize(500, 780);  // Increased height for username field
    setupEditors();
}

EditProfileComponent::~EditProfileComponent()
{
    Log::debug("EditProfileComponent: Destroying");
}

//==============================================================================
void EditProfileComponent::setupEditors()
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
        editor.setFont(14.0f);
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
}

void EditProfileComponent::setProfile(const UserProfile& newProfile)
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

void EditProfileComponent::populateFromProfile()
{
    usernameEditor->setText(profile.username, false);
    displayNameEditor->setText(profile.displayName, false);
    bioEditor->setText(profile.bio, false);
    locationEditor->setText(profile.location, false);
    genreEditor->setText(profile.genre, false);
    dawEditor->setText(profile.dawPreference, false);

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

void EditProfileComponent::setUploadedProfilePictureUrl(const juce::String& s3Url)
{
    if (s3Url.isNotEmpty())
    {
        profile.profilePictureUrl = s3Url;
        pendingAvatarPath = ""; // Clear local path since we have the S3 URL now
        updateHasChanges();
    }
}

void EditProfileComponent::collectToProfile()
{
    profile.username = usernameEditor->getText().trim().toLowerCase();
    profile.displayName = displayNameEditor->getText().trim();
    profile.bio = bioEditor->getText().trim();
    profile.location = locationEditor->getText().trim();
    profile.genre = genreEditor->getText().trim();
    profile.dawPreference = dawEditor->getText().trim();

    // Build social links object
    auto* linksObj = new juce::DynamicObject();

    juce::String instagram = instagramEditor->getText().trim();
    juce::String soundcloud = soundcloudEditor->getText().trim();
    juce::String spotify = spotifyEditor->getText().trim();
    juce::String twitter = twitterEditor->getText().trim();

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

void EditProfileComponent::updateHasChanges()
{
    collectToProfile();

    bool usernameChanged = profile.username != originalProfile.username;

    hasChanges = (usernameChanged ||
                  profile.displayName != originalProfile.displayName ||
                  profile.bio != originalProfile.bio ||
                  profile.location != originalProfile.location ||
                  profile.genre != originalProfile.genre ||
                  profile.dawPreference != originalProfile.dawPreference ||
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
void EditProfileComponent::paint(juce::Graphics& g)
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

    // Error message
    if (errorMessage.isNotEmpty())
    {
        g.setColour(Colors::errorRed);
        g.setFont(12.0f);
        g.drawText(errorMessage, PADDING, getHeight() - 80, getWidth() - PADDING * 2, 20,
                   juce::Justification::centred);
    }
}

void EditProfileComponent::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(Colors::headerBg);
    g.fillRect(bounds);

    // Title
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Edit Profile", bounds, juce::Justification::centred);

    // Bottom border
    g.setColour(Colors::inputBorder);
    g.drawLine(0, static_cast<float>(bounds.getBottom()), static_cast<float>(getWidth()),
               static_cast<float>(bounds.getBottom()), 1.0f);
}

void EditProfileComponent::drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds)
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
        g.setFont(juce::Font(32.0f, juce::Font::bold));
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

void EditProfileComponent::drawFormSection(juce::Graphics& g, const juce::String& title, juce::Rectangle<int> bounds)
{
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(title.toUpperCase(), bounds, juce::Justification::centredLeft);
}

//==============================================================================
void EditProfileComponent::resized()
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
}

juce::Rectangle<int> EditProfileComponent::getAvatarBounds() const
{
    return juce::Rectangle<int>(
        (getWidth() - AVATAR_SIZE) / 2,
        HEADER_HEIGHT + 15,
        AVATAR_SIZE,
        AVATAR_SIZE
    );
}

//==============================================================================
void EditProfileComponent::buttonClicked(juce::Button* button)
{
    if (button == cancelButton.get())
    {
        if (onCancel)
            onCancel();
    }
    else if (button == saveButton.get())
    {
        handleSave();
    }
    else if (button == changePhotoButton.get())
    {
        handlePhotoSelect();
    }
}

void EditProfileComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    // Validate username when it changes
    if (&editor == usernameEditor.get())
    {
        validateUsername(usernameEditor->getText().trim().toLowerCase());
    }

    updateHasChanges();
}

//==============================================================================
void EditProfileComponent::handleSave()
{
    if (networkClient == nullptr || !hasChanges)
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

void EditProfileComponent::saveProfileData()
{
    Log::info("EditProfileComponent: Saving profile data");
    // Build update payload (everything except username)
    auto* updateData = new juce::DynamicObject();
    updateData->setProperty("display_name", profile.displayName);
    updateData->setProperty("bio", profile.bio);
    updateData->setProperty("location", profile.location);
    updateData->setProperty("genre", profile.genre);
    updateData->setProperty("daw_preference", profile.dawPreference);
    updateData->setProperty("social_links", profile.socialLinks);

    // Include profile picture URL if set (either from upload or existing)
    if (profile.profilePictureUrl.isNotEmpty())
        updateData->setProperty("profile_picture_url", profile.profilePictureUrl);

    juce::var payload(updateData);

    networkClient->put("/profile", payload, [this](Outcome<juce::var> responseOutcome) {
        juce::MessageManager::callAsync([this, responseOutcome]() {
            isSaving = false;

            if (responseOutcome.isOk())
            {
                auto response = responseOutcome.getValue();
                // Update original profile to reflect saved state
                originalProfile = profile;
                hasChanges = false;
                updateHasChanges();

                if (onSave)
                    onSave(profile);
            }
            else
            {
                errorMessage = "Failed to save profile: " + responseOutcome.getError();
                saveButton->setEnabled(true);
            }

            repaint();
        });
    });
}

void EditProfileComponent::handleUsernameChange()
{
    if (networkClient == nullptr)
        return;

    networkClient->changeUsername(profile.username, [this](Outcome<juce::var> responseOutcome) {
        juce::MessageManager::callAsync([this, responseOutcome]() {
            if (responseOutcome.isOk())
            {
                // Username changed successfully, now save other profile data
                originalProfile.username = profile.username;
                saveProfileData();
            }
            else
            {
                isSaving = false;

                // Show username-specific error
                usernameError = "Username not available: " + responseOutcome.getError();

                isUsernameValid = false;
                updateHasChanges();
                repaint();
            }
        });
    });
}

void EditProfileComponent::validateUsername(const juce::String& username)
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

void EditProfileComponent::handlePhotoSelect()
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

            // Notify parent for upload
            if (onProfilePicSelected)
                onProfilePicSelected(pendingAvatarPath);
        });
}
