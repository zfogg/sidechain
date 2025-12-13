#include "NotificationSettings.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Result.h"

//==============================================================================
NotificationSettings::NotificationSettings()
{
    Log::info("NotificationSettings: Initializing");
    setSize(400, 550);
    setupToggles();
}

NotificationSettings::~NotificationSettings()
{
    Log::debug("NotificationSettings: Destroying");
}

//==============================================================================
void NotificationSettings::setupToggles()
{
    auto styleToggle = [this](juce::ToggleButton& toggle, const juce::String& label) {
        toggle.setButtonText(label);
        toggle.setColour(juce::ToggleButton::textColourId, Colors::textPrimary);
        toggle.setColour(juce::ToggleButton::tickColourId, Colors::accent);
        toggle.setColour(juce::ToggleButton::tickDisabledColourId, Colors::textSecondary);
        toggle.setToggleState(true, juce::dontSendNotification);
        toggle.onClick = [this, &toggle]() { handleToggleChange(&toggle); };
        addAndMakeVisible(toggle);
    };

    // Create toggle buttons
    likesToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*likesToggle, "Likes");

    commentsToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*commentsToggle, "Comments");

    followsToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*followsToggle, "New Followers");

    mentionsToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*mentionsToggle, "Mentions");

    dmsToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*dmsToggle, "Direct Messages");

    storiesToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*storiesToggle, "Stories");

    repostsToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*repostsToggle, "Reposts");

    challengesToggle = std::make_unique<juce::ToggleButton>();
    styleToggle(*challengesToggle, "MIDI Challenges");

    // Close button
    closeButton = std::make_unique<juce::TextButton>("Close");
    closeButton->setColour(juce::TextButton::buttonColourId, Colors::closeButton);
    closeButton->setColour(juce::TextButton::textColourOffId, Colors::textSecondary);
    closeButton->addListener(this);
    addAndMakeVisible(closeButton.get());
}

void NotificationSettings::loadPreferences()
{
    if (networkClient == nullptr)
    {
        Log::error("NotificationSettings: No network client set");
        return;
    }

    isLoading = true;
    errorMessage = "";
    repaint();

    networkClient->get("/notifications/preferences", [this](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result]() {
            isLoading = false;

            if (result.isOk())
            {
                auto response = result.getValue();
                if (response.hasProperty("preferences"))
                {
                    auto prefs = response["preferences"];

                    likesEnabled = prefs.getProperty("likes_enabled", true);
                    commentsEnabled = prefs.getProperty("comments_enabled", true);
                    followsEnabled = prefs.getProperty("follows_enabled", true);
                    mentionsEnabled = prefs.getProperty("mentions_enabled", true);
                    dmsEnabled = prefs.getProperty("dms_enabled", true);
                    storiesEnabled = prefs.getProperty("stories_enabled", true);
                    repostsEnabled = prefs.getProperty("reposts_enabled", true);
                    challengesEnabled = prefs.getProperty("challenges_enabled", true);

                    populateFromPreferences();
                    Log::info("NotificationSettings: Preferences loaded successfully");
                }
            }
            else
            {
                errorMessage = "Failed to load preferences: " + result.getError();
                Log::error("NotificationSettings: " + errorMessage);
            }

            repaint();
        });
    });
}

void NotificationSettings::populateFromPreferences()
{
    likesToggle->setToggleState(likesEnabled, juce::dontSendNotification);
    commentsToggle->setToggleState(commentsEnabled, juce::dontSendNotification);
    followsToggle->setToggleState(followsEnabled, juce::dontSendNotification);
    mentionsToggle->setToggleState(mentionsEnabled, juce::dontSendNotification);
    dmsToggle->setToggleState(dmsEnabled, juce::dontSendNotification);
    storiesToggle->setToggleState(storiesEnabled, juce::dontSendNotification);
    repostsToggle->setToggleState(repostsEnabled, juce::dontSendNotification);
    challengesToggle->setToggleState(challengesEnabled, juce::dontSendNotification);
}

void NotificationSettings::handleToggleChange(juce::ToggleButton* toggle)
{
    // Update local state based on which toggle changed
    if (toggle == likesToggle.get())
        likesEnabled = toggle->getToggleState();
    else if (toggle == commentsToggle.get())
        commentsEnabled = toggle->getToggleState();
    else if (toggle == followsToggle.get())
        followsEnabled = toggle->getToggleState();
    else if (toggle == mentionsToggle.get())
        mentionsEnabled = toggle->getToggleState();
    else if (toggle == dmsToggle.get())
        dmsEnabled = toggle->getToggleState();
    else if (toggle == storiesToggle.get())
        storiesEnabled = toggle->getToggleState();
    else if (toggle == repostsToggle.get())
        repostsEnabled = toggle->getToggleState();
    else if (toggle == challengesToggle.get())
        challengesEnabled = toggle->getToggleState();

    // Save immediately when changed
    savePreferences();
}

void NotificationSettings::savePreferences()
{
    if (networkClient == nullptr || isSaving)
        return;

    isSaving = true;
    errorMessage = "";

    // Build update payload
    auto* updateData = new juce::DynamicObject();
    updateData->setProperty("likes_enabled", likesEnabled);
    updateData->setProperty("comments_enabled", commentsEnabled);
    updateData->setProperty("follows_enabled", followsEnabled);
    updateData->setProperty("mentions_enabled", mentionsEnabled);
    updateData->setProperty("dms_enabled", dmsEnabled);
    updateData->setProperty("stories_enabled", storiesEnabled);
    updateData->setProperty("reposts_enabled", repostsEnabled);
    updateData->setProperty("challenges_enabled", challengesEnabled);

    juce::var payload(updateData);

    networkClient->put("/notifications/preferences", payload, [this](Outcome<juce::var> result) {
        juce::MessageManager::callAsync([this, result]() {
            isSaving = false;

            if (result.isOk())
            {
                Log::info("NotificationSettings: Preferences saved successfully");
            }
            else
            {
                errorMessage = "Failed to save: " + result.getError();
                Log::error("NotificationSettings: " + errorMessage);
            }

            repaint();
        });
    });
}

//==============================================================================
void NotificationSettings::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(Colors::background);

    // Header
    auto headerBounds = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    drawHeader(g, headerBounds);

    // Section labels
    int y = HEADER_HEIGHT + PADDING;

    // Social notifications section
    drawSection(g, "SOCIAL NOTIFICATIONS", juce::Rectangle<int>(PADDING, y, getWidth() - PADDING * 2, 20));
    y += 25 + TOGGLE_HEIGHT * 4; // 4 toggles

    // Content notifications section
    drawSection(g, "CONTENT NOTIFICATIONS", juce::Rectangle<int>(PADDING, y, getWidth() - PADDING * 2, 20));
    y += 25 + TOGGLE_HEIGHT * 2; // 2 toggles

    // Activity notifications section
    drawSection(g, "ACTIVITY NOTIFICATIONS", juce::Rectangle<int>(PADDING, y, getWidth() - PADDING * 2, 20));

    // Loading indicator
    if (isLoading)
    {
        g.setColour(Colors::textSecondary);
        g.setFont(14.0f);
        g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
    }

    // Error message
    if (errorMessage.isNotEmpty())
    {
        g.setColour(Colors::errorRed);
        g.setFont(12.0f);
        g.drawText(errorMessage, PADDING, getHeight() - 70, getWidth() - PADDING * 2, 20,
                   juce::Justification::centred);
    }
}

void NotificationSettings::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(Colors::headerBg);
    g.fillRect(bounds);

    // Title
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("Notification Settings", bounds, juce::Justification::centred);

    // Bottom border
    g.setColour(Colors::toggleBorder);
    g.drawLine(0, static_cast<float>(bounds.getBottom()), static_cast<float>(getWidth()),
               static_cast<float>(bounds.getBottom()), 1.0f);
}

void NotificationSettings::drawSection(juce::Graphics& g, const juce::String& title, juce::Rectangle<int> bounds)
{
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(title, bounds, juce::Justification::centredLeft);
}

//==============================================================================
void NotificationSettings::resized()
{
    // Close button in header
    closeButton->setBounds(getWidth() - PADDING - 60, 15, 60, 30);

    int y = HEADER_HEIGHT + PADDING + 25; // After section label
    int toggleWidth = getWidth() - PADDING * 2;

    // Social notifications
    likesToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT;

    commentsToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT;

    followsToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT;

    mentionsToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT + SECTION_SPACING + 25; // Section spacing + label

    // Content notifications
    storiesToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT;

    repostsToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT + SECTION_SPACING + 25; // Section spacing + label

    // Activity notifications
    dmsToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
    y += TOGGLE_HEIGHT;

    challengesToggle->setBounds(PADDING, y, toggleWidth, TOGGLE_HEIGHT);
}

//==============================================================================
void NotificationSettings::buttonClicked(juce::Button* button)
{
    if (button == closeButton.get())
    {
        if (onClose)
            onClose();
    }
}
